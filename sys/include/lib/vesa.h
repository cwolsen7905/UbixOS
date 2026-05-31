/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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

#ifndef _LIB_VESA_H
#define _LIB_VESA_H

#include <sys/types.h>

/* Physical addresses for VBE data buffers in free conventional RAM (above BDA) */
#define VBE_INFO_PADDR    0x800
#define VBE_MODE_PADDR    0x900
#define VBE_INFO_SEG      (VBE_INFO_PADDR >> 4)   /* 0x0080 */
#define VBE_MODE_SEG      (VBE_MODE_PADDR >> 4)   /* 0x0090 */

/* VBE 2.0 Info Block — filled by INT 10h AX=0x4F00, ES:DI=VBE_INFO_PADDR */
typedef struct {
  u_int8_t  VbeSignature[4];    /* "VESA" on return */
  u_int16_t VbeVersion;         /* 0x0200 = VBE 2.0 */
  u_int32_t OemStringPtr;       /* far ptr to OEM string */
  u_int8_t  Capabilities[4];
  u_int32_t VideoModePtr;       /* far ptr to mode list (seg:off) */
  u_int16_t TotalMemory;        /* 64KB blocks of video RAM */
  u_int16_t OemSoftwareRev;
  u_int32_t OemVendorNamePtr;
  u_int32_t OemProductNamePtr;
  u_int32_t OemProductRevPtr;
  u_int8_t  Reserved[222];
  u_int8_t  OemData[256];
} __attribute__((packed)) vbe_info_t;

/* VBE 2.0 Mode Info Block — filled by INT 10h AX=0x4F01, CX=mode, ES:DI=VBE_MODE_PADDR */
typedef struct {
  u_int16_t ModeAttributes;       /* bit 7: linear framebuffer available */
  u_int8_t  WinAAttributes;
  u_int8_t  WinBAttributes;
  u_int16_t WinGranularity;       /* KB */
  u_int16_t WinSize;              /* KB */
  u_int16_t WinASegment;
  u_int16_t WinBSegment;
  u_int32_t WinFuncPtr;
  u_int16_t BytesPerScanLine;
  u_int16_t XResolution;
  u_int16_t YResolution;
  u_int8_t  XCharSize;
  u_int8_t  YCharSize;
  u_int8_t  NumberOfPlanes;
  u_int8_t  BitsPerPixel;
  u_int8_t  NumberOfBanks;
  u_int8_t  MemoryModel;
  u_int8_t  BankSize;
  u_int8_t  NumberOfImagePages;
  u_int8_t  Reserved1;
  u_int8_t  RedMaskSize;
  u_int8_t  RedFieldPosition;
  u_int8_t  GreenMaskSize;
  u_int8_t  GreenFieldPosition;
  u_int8_t  BlueMaskSize;
  u_int8_t  BlueFieldPosition;
  u_int8_t  RsvdMaskSize;
  u_int8_t  RsvdFieldPosition;
  u_int8_t  DirectColorModeInfo;
  u_int32_t PhysBasePtr;          /* physical address of linear framebuffer */
  u_int32_t OffScreenMemOffset;
  u_int16_t OffScreenMemSize;
  u_int8_t  Reserved2[206];
} __attribute__((packed)) vbe_mode_info_t;

int  vesa_init(u_int16_t mode);
void vesa_text_mode(void);
void vesa_map_fb(void);
void vesa_draw_test(void);
void vesa_draw_circle(u_int32_t cx, u_int32_t cy, u_int32_t r, u_int32_t color);

/* One enumerated VBE mode (linear framebuffer, 24bpp).  Layout matches the
 * userland struct vesa_mode in include/api/display.h. */
struct vesa_mode
{
	u_int16_t mode; /* VBE mode number */
	u_int16_t width;
	u_int16_t height;
	u_int8_t  bpp;
};

/* Enumerate up to max LFB 24bpp modes the BIOS reports; returns the count.
 * MUST be called from the systemtask (kernel) context — the V86 BIOS-call
 * machinery races with task reaping when invoked from a user-process syscall. */
int vesa_enum_modes(struct vesa_mode *out, int max);

/* Cache of enumerated modes, filled once by the systemtask (where the BIOS call
 * is safe) and copied to userland by the sys_vesa_modes syscall. */
#define VESA_MAX_MODES 32
extern struct vesa_mode g_vesa_modes[VESA_MAX_MODES];
extern int g_vesa_mode_count; /* -1 until enumerated */

/* Populated by vesa_init() for use by graphics code */
extern u_int32_t vesa_fb_paddr;    /* physical base of linear framebuffer */
extern u_int16_t vesa_pitch;       /* bytes per scanline */
extern u_int16_t vesa_width;
extern u_int16_t vesa_height;
extern u_int8_t  vesa_bpp;
extern u_int16_t vesa_current_mode; /* mode number from last successful vesa_init() */

#endif /* _LIB_VESA_H */
