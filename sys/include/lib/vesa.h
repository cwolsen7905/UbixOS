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
  uint8_t  VbeSignature[4];    /* "VESA" on return */
  uint16_t VbeVersion;         /* 0x0200 = VBE 2.0 */
  uint32_t OemStringPtr;       /* far ptr to OEM string */
  uint8_t  Capabilities[4];
  uint32_t VideoModePtr;       /* far ptr to mode list (seg:off) */
  uint16_t TotalMemory;        /* 64KB blocks of video RAM */
  uint16_t OemSoftwareRev;
  uint32_t OemVendorNamePtr;
  uint32_t OemProductNamePtr;
  uint32_t OemProductRevPtr;
  uint8_t  Reserved[222];
  uint8_t  OemData[256];
} __attribute__((packed)) vbe_info_t;

/* VBE 2.0 Mode Info Block — filled by INT 10h AX=0x4F01, CX=mode, ES:DI=VBE_MODE_PADDR */
typedef struct {
  uint16_t ModeAttributes;       /* bit 7: linear framebuffer available */
  uint8_t  WinAAttributes;
  uint8_t  WinBAttributes;
  uint16_t WinGranularity;       /* KB */
  uint16_t WinSize;              /* KB */
  uint16_t WinASegment;
  uint16_t WinBSegment;
  uint32_t WinFuncPtr;
  uint16_t BytesPerScanLine;
  uint16_t XResolution;
  uint16_t YResolution;
  uint8_t  XCharSize;
  uint8_t  YCharSize;
  uint8_t  NumberOfPlanes;
  uint8_t  BitsPerPixel;
  uint8_t  NumberOfBanks;
  uint8_t  MemoryModel;
  uint8_t  BankSize;
  uint8_t  NumberOfImagePages;
  uint8_t  Reserved1;
  uint8_t  RedMaskSize;
  uint8_t  RedFieldPosition;
  uint8_t  GreenMaskSize;
  uint8_t  GreenFieldPosition;
  uint8_t  BlueMaskSize;
  uint8_t  BlueFieldPosition;
  uint8_t  RsvdMaskSize;
  uint8_t  RsvdFieldPosition;
  uint8_t  DirectColorModeInfo;
  uint32_t PhysBasePtr;          /* physical address of linear framebuffer */
  uint32_t OffScreenMemOffset;
  uint16_t OffScreenMemSize;
  uint8_t  Reserved2[206];
} __attribute__((packed)) vbe_mode_info_t;

int  vesa_init(uint16_t mode);
void vesa_text_mode(void);
void vesa_map_fb(void);
void vesa_draw_test(void);
void vesa_draw_circle(uint32_t cx, uint32_t cy, uint32_t r, uint32_t color);

/* Populated by vesa_init() for use by graphics code */
extern uint32_t vesa_fb_paddr;    /* physical base of linear framebuffer */
extern uint16_t vesa_pitch;       /* bytes per scanline */
extern uint16_t vesa_width;
extern uint16_t vesa_height;
extern uint8_t  vesa_bpp;

#endif /* _LIB_VESA_H */
