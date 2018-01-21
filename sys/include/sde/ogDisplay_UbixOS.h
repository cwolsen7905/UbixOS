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

#ifndef OGDISPLAY_UBIXOS_H
#define OGDISPLAY_UBIXOS_H

extern "C" {
#include <sys/types.h>
}

#include <objgfx40/objgfx40.h>

struct ogModeInfo {
    uInt16 modeAttributes __attribute__((packed));
    uInt8 windowAFlags __attribute__((packed));
    uInt8 windowBFlags __attribute__((packed));
    uInt16 granularity __attribute__((packed));
    uInt16 windowSize __attribute__((packed));
    uInt16 windowASeg __attribute__((packed));
    uInt16 windowBSeg __attribute__((packed));
    void* bankSwitch __attribute__((packed));
    uInt16 bytesPerLine __attribute__((packed));
    uInt16 xRes __attribute__((packed));
    uInt16 yRes __attribute__((packed));
    uInt8 charWidth __attribute__((packed));
    uInt8 charHeight __attribute__((packed));
    uInt8 numBitPlanes __attribute__((packed));
    uInt8 bitsPerPixel __attribute__((packed));
    uInt8 numberOfBanks __attribute__((packed));
    uInt8 memoryModel __attribute__((packed));
    uInt8 bankSize __attribute__((packed));
    uInt8 numOfImagePages __attribute__((packed));
    uInt8 reserved __attribute__((packed));
    // Direct colour fields (required for Direct/6 and YUV/7 memory models
    uInt8 redMaskSize __attribute__((packed));
    uInt8 redFieldPosition __attribute__((packed));
    uInt8 greenMaskSize __attribute__((packed));
    uInt8 greenFieldPosition __attribute__((packed));
    uInt8 blueMaskSize __attribute__((packed));
    uInt8 blueFieldPosition __attribute__((packed));
    uInt8 alphaMaskSize __attribute__((packed));
    uInt8 alphaFieldPosition __attribute__((packed));
    uInt8 directColourMode __attribute__((packed));
    // VESA 2.0 specific fields
    uInt32 physBasePtr __attribute__((packed));
    void* offScreenMemOffset __attribute__((packed));
    uInt16 offScreenMemSize __attribute__((packed));
    uInt8 paddington[461] __attribute__((packed));
};

struct ogVESAInfo {
    char VBESignature[4] __attribute__((packed));
    uInt8 minVersion __attribute__((packed));
    uInt8 majVersion __attribute__((packed));
    uInt32 OEMStringPtr __attribute__((packed));
    uInt32 capabilities __attribute__((packed));
    uInt32 videoModePtr __attribute__((packed));
    uInt16 totalMemory __attribute__((packed));
    // VESA 2.0 specific fields
    uInt16 OEMSoftwareRev __attribute__((packed));
    uInt32 OEMVendorNamePtr __attribute__((packed));
    uInt32 OEMProductNamePtr __attribute__((packed));
    uInt32 OEMProductRevPtr __attribute__((packed));
    uInt8 paddington[474] __attribute__((packed));
};

class ogDisplay_UbixOS: public ogSurface {
  protected:
    void * pages[2];
    uInt32 activePage;
    uInt32 visualPage;
    ogVESAInfo * VESAInfo;
    ogModeInfo * modeInfo;

    uInt16 FindMode(uInt32, uInt32, uInt32);
    void GetModeInfo(uInt16);
    void GetVESAInfo(void);
    void SetMode(uInt16);
    void SetPal(void);
  public:
    ogDisplay_UbixOS(void);
    virtual bool ogAlias(ogSurface&, uInt32, uInt32, uInt32, uInt32);
    virtual bool ogClone(ogSurface&);
    virtual void ogCopyPalette(ogSurface&);
    virtual bool ogCreate(uInt32, uInt32, ogPixelFmt);
    virtual bool ogLoadPalette(const char *);
    virtual void ogSetPalette(const ogRGBA8[]);
    virtual void ogSetPalette(uInt8, uInt8, uInt8, uInt8);
    virtual void ogSetPalette(uInt8, uInt8, uInt8, uInt8, uInt8);
    virtual ~ogDisplay_UbixOS(void);
};
// ogDisplay_UbixOS

#endif
