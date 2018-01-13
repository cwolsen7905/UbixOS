/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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

#include <objgfx40/objgfx40.h>
#include <objgfx40/defpal.inc>
#include <sde/ogDisplay_UbixOS.h>

extern "C" {
#include <lib/bioscall.h>
#include <vmm/paging.h>
#include <lib/kprintf.h>
#include <sys/io.h>
#include <lib/string.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
}

/*
 *
 *              ogDisplay methods
 *
 */

void initVESAMode(uInt16 mode) {
  //kprintf("Pre-initVESAMode\n");
  biosCall(0x10, 0x4F02, mode, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0);
  //kprintf("Post-initVESAMode\n");
  return;
}

ogDisplay_UbixOS::ogDisplay_UbixOS(void) {
  pages[0] = pages[1] = NULL;
  activePage = visualPage = 0;
  pal = new ogRGBA8[256];
  attributes = new ogAttributes();

  VESAInfo = (ogVESAInfo *) 0x11000;
  modeInfo = (ogModeInfo *) 0x11200;
  GetVESAInfo();
  return;
} // ogDisplay_UbixOS::ogDisplay_UbixOS

void ogDisplay_UbixOS::GetModeInfo(uInt16 mode) {
  //kprintf("Pre-getModeInfo\n");
  biosCall(0x10, 0x4F01, 0x0, mode, 0x0, 0x0, 0x0, 0x1120, 0x0);
  //kprintf("Post-getModeInfo\n");
  return;
} // ogDisplay_UbixOS::GetModeInfo

void ogDisplay_UbixOS::GetVESAInfo(void) {
  VESAInfo->VBESignature[0] = 'V'; // First off initialize the structure.
  VESAInfo->VBESignature[1] = 'B';
  VESAInfo->VBESignature[2] = 'E';
  VESAInfo->VBESignature[3] = '2';
  //kprintf("Pre-getVESAInfo\n");
  biosCall(0x10, 0x4F00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1100, 0x0);
  //kprintf("Post-getVESAInfo\n");
  return;
} // ogDisplay_UbixOS::GetVESAInfo

uInt16 ogDisplay_UbixOS::FindMode(uInt32 _xRes, uInt32 _yRes, uInt32 _BPP) {
  uInt16 mode;

  if ((_xRes == 320) && (_yRes == 200) && (_BPP == 8))
    return 0x13;

//  if ((VESAInfo==NULL) || (VESAInfo->videoModePtr==NULL)) return 0;
  if (modeInfo == NULL)
    return 0;

  for (mode = 0x100; mode < 0x1FF; mode++) {
    GetModeInfo(mode);
    if ((modeInfo->xRes >= _xRes) && (modeInfo->yRes >= _yRes) && (modeInfo->bitsPerPixel == _BPP))
      return mode;
  }

  return 0;
} // ogDisplay_UbixOS::FindMode

void ogDisplay_UbixOS::SetMode(uInt16 mode) {

  uInt32 size = 0x0, count = 0x0, i = 0x0;

  if (mode == 0x13) {

    xRes = 320;
    yRes = 200;
    maxX = 319;
    maxY = 199;
    BPP = 8;
    bytesPerPix = 1;

    redFieldPosition = 0;
    greenFieldPosition = 0;
    blueFieldPosition = 0;
    alphaFieldPosition = 0;

    redShifter = 0;
    greenShifter = 0;
    blueShifter = 0;
    alphaFieldPosition = 0;

    // UBU, THIS IS NULL BECAUSE WE DON'T EVER USE 320x200x256c!
    // THIS COMMENT WILL COME BACK TO BITE YOU ON THE ASS
    buffer = NULL;

  }
  else {
    buffer = NULL;
    mode |= 0x4000;  // attempt lfb
    GetModeInfo(mode);
    if (modeInfo->physBasePtr == 0)
      return;
    buffer = (void *) modeInfo->physBasePtr;
    size = (modeInfo->yRes * modeInfo->bytesPerLine + 4095);

    xRes = modeInfo->bytesPerLine;
    yRes = modeInfo->yRes;
    maxX = modeInfo->xRes - 1;
    maxY = yRes - 1;

    BPP = modeInfo->bitsPerPixel;
    bytesPerPix = (BPP + 7) >> 3;

    redFieldPosition = modeInfo->redFieldPosition;
    greenFieldPosition = modeInfo->greenFieldPosition;
    blueFieldPosition = modeInfo->blueFieldPosition;
    alphaFieldPosition = modeInfo->alphaFieldPosition;

    if (bytesPerPix == 4) {
      modeInfo->alphaMaskSize = 8;
      while ((alphaFieldPosition == redFieldPosition) || (alphaFieldPosition == greenFieldPosition) || (alphaFieldPosition == blueFieldPosition))
        alphaFieldPosition += 8;
    } // if

    redShifter = 8 - modeInfo->redMaskSize;
    greenShifter = 8 - modeInfo->greenMaskSize;
    blueShifter = 8 - modeInfo->blueMaskSize;
    alphaShifter = 8 - modeInfo->alphaMaskSize;

    if (modeInfo->alphaMaskSize != 0)
      alphaMasker = ~(OG_MASKS[modeInfo->alphaMaskSize] << alphaFieldPosition);
    else
      alphaMasker = ~0;

  } // else not mode 0x13

  printOff = 0;
  for (i = 0x0; i < ((size) / 4096); i++) {
    if ((vmm_remapPage(modeInfo->physBasePtr + (i * 0x1000), modeInfo->physBasePtr + (i * 0x1000), KERNEL_PAGE_DEFAULT)) == 0x0)
      kpanic("Error: vmm_remapPage failed\n");
  } // for i

  owner = this;
  dataState = ogAliasing;

  initVESAMode(mode);

  if ((lineOfs != NULL) && (lSize != 0))
    delete[] lineOfs;
  lSize = yRes * sizeof(uInt32);
  lineOfs = new uInt32[yRes];
  ;
  if (lineOfs == NULL)
    return;

  lineOfs[0] = 0;
  for (count = 1; count < yRes; count++)
    lineOfs[count] = lineOfs[count - 1] + xRes;

  if (1 == bytesPerPix) {
    pixFmtID = 0x08080808;
  }
  else {
    pixFmtID = (redFieldPosition) | (greenFieldPosition << 8) | (blueFieldPosition << 16) | (alphaFieldPosition << 24);
  } // else

  ogSetAntiAliasing(BPP > 8);
  if (pal == NULL)
    pal = new ogRGBA8[256];
  ogSetPalette(DEFAULT_PALETTE);

  return;
} // ogDisplay_UbixOS::SetMode

void ogDisplay_UbixOS::ogSetPalette(const ogRGBA8 newPal[256]) {
  ogSurface::ogSetPalette(newPal);
  SetPal();
  return;
} // ogDisplay_UbixOS::ogSetPalette

void ogDisplay_UbixOS::SetPal(void) {
  if (bytesPerPix != 1)
    return;
  outportByte(0x3c8, 0);
  for (uInt32 c = 0; c < 256; c++) {
    outportByte(0x3c9, pal[c].red >> 2);
    outportByte(0x3c9, pal[c].green >> 2);
    outportByte(0x3c9, pal[c].blue >> 2);
  } // for
  return;
} // ogDisplay_UbixOS::SetPal

bool ogDisplay_UbixOS::ogAlias(ogSurface& SrcObject, uInt32 x1, uInt32 y1, uInt32 x2, uInt32 y2) {
  ogSetLastError(ogNoAliasing);
  return false;
} // ogDisplay_UbixOS::ogAlias

bool ogDisplay_UbixOS::ogCreate(uInt32 _xRes, uInt32 _yRes, ogPixelFmt _pixFormat) {
  uInt16 mode;
  mode = 0x114;  // was 0x111
  SetMode(mode);
  /*
   mode = FindMode(_xRes, _yRes, _pixFormat.BPP);
   if ((mode == 0) && ((_pixFormat.BPP==24) || (_pixFormat.BPP==32))) {
   if (_pixFormat.BPP==24) _pixFormat.BPP=32; else _pixFormat.BPP=24;
   mode=FindMode(_xRes,_yRes,_pixFormat.BPP);
   } // if
   if (mode!=0) SetMode(mode);
   */
  return (mode != 0);
} // ogDisplay_UbixOS::ogCreate

bool ogDisplay_UbixOS::ogClone(ogSurface& SrcObject) {
  ogSetLastError(ogNoCloning);
  return false;
} // ogDisplay_UbixOS::ogClone

void ogDisplay_UbixOS::ogCopyPalette(ogSurface& SrcObject) {
  ogSurface::ogCopyPalette(SrcObject);
  SetPal();
  return;
} // ogDisplay_UbixOS::ogCopyPalette

bool ogDisplay_UbixOS::ogLoadPalette(const char *palfile) {
  bool result;
  if ((result = ogSurface::ogLoadPalette(palfile)) == true)
    SetPal();
  return result;
} // ogDisplay_UbixOS::ogLoadPalette

void ogDisplay_UbixOS::ogSetPalette(uInt8 colour, uInt8 red, uInt8 green, uInt8 blue) {
  if (pal == NULL)
    return;
  ogSurface::ogSetPalette(colour, red, green, blue);
  outportByte(0x3c8, colour);
  outportByte(0x3c9, red >> 2);
  outportByte(0x3c9, green >> 2);
  outportByte(0x3c9, blue >> 2);

  return;
} // ogDisplay_UbixOS::ogSetPalette

void ogDisplay_UbixOS::ogSetPalette(uInt8 colour, uInt8 red, uInt8 green, uInt8 blue, uInt8 alpha) {
  if (pal == NULL)
    return;
  ogSurface::ogSetPalette(colour, red, green, blue, alpha);
  outportByte(0x3c8, colour);
  outportByte(0x3c9, red >> 2);
  outportByte(0x3c9, green >> 2);
  outportByte(0x3c9, blue >> 2);

  return;
} // ogDisplay_UbixOS::ogSetPalette

ogDisplay_UbixOS::~ogDisplay_UbixOS(void) {
  delete attributes;
  delete pal;
//mji  delete VESAInfo;
//mji  delete modeInfo;
  return;
} // ogDisplay_UbixOS::~ogDisplay_UbixOS

/***
 END
 ***/

