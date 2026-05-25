/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <lib/vesa.h>
#include <lib/bioscall.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <vmm/paging.h>
#include <string.h>

u_int32_t vesa_fb_paddr    = 0;
u_int16_t vesa_pitch       = 0;
u_int16_t vesa_width       = 0;
u_int16_t vesa_height      = 0;
u_int8_t  vesa_bpp         = 0;
u_int16_t vesa_current_mode = 0;

int vesa_init(u_int16_t mode) {
  struct biosRegs r;
  vbe_info_t    *info  = (vbe_info_t    *)VBE_INFO_PADDR;
  vbe_mode_info_t *mi  = (vbe_mode_info_t *)VBE_MODE_PADDR;

  /* --- GetVBEInfo (INT 10h AX=4F00h, ES:DI = VBE info buffer) --- */
  memset(info, 0, 512);
  info->VbeSignature[0] = 'V';   /* "VBE2" requests VBE 2.0+ format */
  info->VbeSignature[1] = 'B';
  info->VbeSignature[2] = 'E';
  info->VbeSignature[3] = '2';

  /*
   * ES:DI points to the VBE info buffer.
   * biosCallEx param order: biosInt, eax, ebx, ecx, edx, esi, edi, es, ds, out
   * ES = VBE_INFO_SEG (0x80), DI = 0 → physical 0x800
   */
  biosCallEx(0x10, 0x4F00, 0, 0, 0, 0, 0, VBE_INFO_SEG, 0, &r);
  if (r.ax != 0x004F) {
    kprintf("vesa: GetVBEInfo failed: AX=0x%X\n", r.ax);
    return -1;
  }
  if (info->VbeSignature[0] != 'V' || info->VbeSignature[1] != 'E' ||
      info->VbeSignature[2] != 'S' || info->VbeSignature[3] != 'A') {
    kprintf("vesa: bad VBE signature\n");
    return -1;
  }
  kprintf("vesa: VBE %d.%d detected, %dKB video RAM\n",
      info->VbeVersion >> 8, info->VbeVersion & 0xFF,
      (u_int32_t)info->TotalMemory * 64);

  /* --- GetModeInfo (INT 10h AX=4F01h, CX=mode, ES:DI = mode info buffer) --- */
  memset(mi, 0, 256);
  /*
   * CX = mode number, ES:DI = mode info buffer
   * ES = VBE_MODE_SEG (0x90), DI = 0 → physical 0x900
   * ecx carries the mode number
   */
  biosCallEx(0x10, 0x4F01, 0, mode, 0, 0, 0, VBE_MODE_SEG, 0, &r);
  if (r.ax != 0x004F) {
    kprintf("vesa: GetModeInfo failed for mode 0x%X: AX=0x%X\n", mode, r.ax);
    return -1;
  }

  kprintf("vesa: mode 0x%X: %dx%dx%d bpp, pitch=%d, LFB=0x%X, attrs=0x%X\n",
      mode,
      mi->XResolution, mi->YResolution, mi->BitsPerPixel,
      mi->BytesPerScanLine, mi->PhysBasePtr, mi->ModeAttributes);

  if (!(mi->ModeAttributes & 0x80)) {
    kprintf("vesa: mode 0x%X has no linear framebuffer (attrs=0x%X)\n",
        mode, mi->ModeAttributes);
    return -1;
  }

  /* --- SetMode (INT 10h AX=4F02h, BX=mode|0x4000 for LFB) --- */
  biosCallEx(0x10, 0x4F02, (u_int16_t)(mode | 0x4000), 0, 0, 0, 0, 0, 0, &r);
  if (r.ax != 0x004F) {
    kprintf("vesa: SetMode failed: AX=0x%X\n", r.ax);
    return -1;
  }

  vesa_fb_paddr     = mi->PhysBasePtr;
  vesa_pitch        = mi->BytesPerScanLine;
  vesa_width        = mi->XResolution;
  vesa_height       = mi->YResolution;
  vesa_bpp          = mi->BitsPerPixel;
  vesa_current_mode = mode;

  kprintf("vesa: mode 0x%X set OK — %dx%dx%d, LFB @ 0x%X, pitch=%d\n",
      mode, vesa_width, vesa_height, vesa_bpp, vesa_fb_paddr, vesa_pitch);
  return 0;
}

/*
 * vesa_text_mode — switch the VGA adapter back to 80×25 text mode (mode 3).
 * Uses INT 10h AX=0003h via the V86 bioscall trampoline.  Must only be called
 * from normal task context (not from an ISR or the scheduler), because
 * biosCall internally calls sched_yield() while the V86 task runs.
 */
void
vesa_text_mode(void)
{
    biosCall(0x10, 0x0003, 0, 0, 0, 0, 0, 0, 0);
    /* Clear the saved VESA dimensions so sys_mapfb knows VESA is gone */
    vesa_fb_paddr = 0;
    vesa_pitch    = 0;
    vesa_width    = 0;
    vesa_height   = 0;
    vesa_bpp      = 0;
    kprintf("vesa: switched to text mode\n");
}

void vesa_map_fb(void) {
  u_int32_t base, end, addr;

  if (!vesa_fb_paddr || !vesa_pitch || !vesa_width || !vesa_height || !vesa_bpp)
    return;

  base = vesa_fb_paddr & ~0xFFF;
  end  = vesa_fb_paddr + (u_int32_t)vesa_pitch * vesa_height;

  for (addr = base; addr < end; addr += PAGE_SIZE)
    vmm_remap_io_page(addr, KERNEL_PAGE_DEFAULT | PAGE_CACHE_DISABLED, sysID);

  kprintf("vesa: LFB mapped 0x%X–0x%X (%u pages)\n",
      base, end, (end - base + PAGE_SIZE - 1) / PAGE_SIZE);
}

void vesa_draw_test(void) {
  u_int8_t  *fb    = (u_int8_t *)vesa_fb_paddr;
  u_int32_t  bpp   = vesa_bpp / 8;
  u_int32_t  x, y;

  if (!vesa_fb_paddr || !vesa_pitch)
    return;

  /* Draw a simple gradient: red fills top third, green middle, blue bottom */
  for (y = 0; y < vesa_height; y++) {
    u_int32_t color;
    if (y < vesa_height / 3)
      color = 0xFF0000;                  /* red */
    else if (y < (u_int32_t)(vesa_height * 2 / 3))
      color = 0x00FF00;                  /* green */
    else
      color = 0x0000FF;                  /* blue */

    for (x = 0; x < vesa_width; x++) {
      u_int8_t *p = fb + y * vesa_pitch + x * bpp;
      p[0] = color & 0xFF;              /* blue  channel (BGR layout) */
      p[1] = (color >> 8) & 0xFF;      /* green channel */
      p[2] = (color >> 16) & 0xFF;     /* red   channel */
    }
  }
}

void vesa_draw_circle(u_int32_t cx, u_int32_t cy, u_int32_t r, u_int32_t color) {
  u_int8_t  *fb  = (u_int8_t *)vesa_fb_paddr;
  u_int32_t  bpp = vesa_bpp / 8;
  int32_t   x, y, dx, dy;

  if (!vesa_fb_paddr || !vesa_pitch)
    return;

  /* Midpoint circle algorithm — integer only, no sqrt */
  x  = (int32_t)r;
  y  = 0;
  dx = 1 - 2 * (int32_t)r;
  dy = 1;
  int32_t err = 0;

#define PLOT(px, py) do { \
    if ((px) >= 0 && (py) >= 0 && \
        (u_int32_t)(px) < vesa_width && (u_int32_t)(py) < vesa_height) { \
      u_int8_t *p = fb + (py) * vesa_pitch + (px) * bpp; \
      p[0] = color & 0xFF; \
      p[1] = (color >> 8) & 0xFF; \
      p[2] = (color >> 16) & 0xFF; \
    } \
  } while (0)

  while (x >= y) {
    PLOT((int32_t)cx + x, (int32_t)cy + y);
    PLOT((int32_t)cx + x, (int32_t)cy - y);
    PLOT((int32_t)cx - x, (int32_t)cy + y);
    PLOT((int32_t)cx - x, (int32_t)cy - y);
    PLOT((int32_t)cx + y, (int32_t)cy + x);
    PLOT((int32_t)cx + y, (int32_t)cy - x);
    PLOT((int32_t)cx - y, (int32_t)cy + x);
    PLOT((int32_t)cx - y, (int32_t)cy - x);
    err += dy;
    dy  += 2;
    y++;
    if (2 * err + dx > 0) {
      err += dx;
      dx  += 2;
      x--;
    }
  }
#undef PLOT
}
