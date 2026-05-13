/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/fb.h>
#include <sys/sysproto.h>
#include <sys/thread.h>
#include <vmm/paging.h>
#include <vmm/vmm.h>
#include <lib/vesa.h>
#include <lib/kprintf.h>
#include <ubixos/sched.h>
#include <isa/mouse.h>

int sys_mapfb(struct thread *td, struct sys_mapfb_args *args) {
  struct fb_info *out = args->info;
  uint32_t paddr, end, addr, vaddr;
  pidType pid = _current->id;

  if (!vesa_fb_paddr || !vesa_pitch || !vesa_width || !vesa_height || !vesa_bpp) {
    kprintf("sys_mapfb: VESA not initialised — send MPI 0x82 to system first\n");
    td->td_retval[0] = -1;
    return (-1);
  }

  /*
   * Map framebuffer physical pages into the calling process at a fixed
   * user-space virtual address.  The LFB physical address (e.g. 0xFD000000)
   * is above the 3 GB kernel/user split so we cannot use it as a virtual
   * address — map it at 0x10000000 instead, which is safely in user space.
   */
  paddr = vesa_fb_paddr & ~0xFFFU;
  end   = vesa_fb_paddr + (uint32_t)vesa_pitch * vesa_height;
  vaddr = 0x10000000U;

  for (addr = paddr; addr < end; addr += 0x1000, vaddr += 0x1000)
    vmm_remapPage(addr, vaddr, PAGE_DEFAULT | PAGE_CACHE_DISABLED, pid, 0);

  out->base   = (void *)0x10000000U;
  out->width  = vesa_width;
  out->height = vesa_height;
  out->pitch  = vesa_pitch;
  out->bpp    = vesa_bpp;

  kprintf("sys_mapfb: pid %d mapped fb paddr=0x%X vaddr=0x10000000 %dx%d bpp=%d\n",
      pid, vesa_fb_paddr, vesa_width, vesa_height, vesa_bpp);

  td->td_retval[0] = 0;
  return (0);
}

int sys_shareregion(struct thread *td, struct sys_shareregion_args *args) {
  uintptr_t client_vaddr;

  client_vaddr = vmm_share_region((uintptr_t)args->vaddr,
      (size_t)args->size, (pidType)args->dst_pid);

  if (client_vaddr == 0) {
    td->td_retval[0] = -1;
    return (-1);
  }

  *args->out_vaddr = (uint32_t)client_vaddr;
  td->td_retval[0] = 0;
  return (0);
}

int sys_getmouse(struct thread *td, struct sys_getmouse_args *args) {
  mouse_event_t *out = args->ev;
  mouse_event_t ev;

  if (mouse_getEvent(&ev) != 0) {
    td->td_retval[0] = -1;
    return (-1);
  }

  out->dx      = ev.dx;
  out->dy      = ev.dy;
  out->buttons = ev.buttons;

  td->td_retval[0] = 0;
  return (0);
}
