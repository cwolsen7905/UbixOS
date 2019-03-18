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

#include <sys/types.h>
#include <objgfx40/objgfx40.h>

struct ogModeInfo {
    uint16_t modeAttributes __attribute__((packed));
    uint8_t windowAFlags __attribute__((packed));
    uint8_t windowBFlags __attribute__((packed));
    uint16_t granularity __attribute__((packed));
    uint16_t windowSize __attribute__((packed));
    uint16_t windowASeg __attribute__((packed));
    uint16_t windowBSeg __attribute__((packed));
    void* bankSwitch __attribute__((packed));
    uint16_t bytesPerLine __attribute__((packed));
    uint16_t xRes __attribute__((packed));
    uint16_t yRes __attribute__((packed));
    uint8_t charWidth __attribute__((packed));
    uint8_t charHeight __attribute__((packed));
    uint8_t numBitPlanes __attribute__((packed));
    uint8_t bitsPerPixel __attribute__((packed));
    uint8_t numberOfBanks __attribute__((packed));
    uint8_t memoryModel __attribute__((packed));
    uint8_t bankSize __attribute__((packed));
    uint8_t numOfImagePages __attribute__((packed));
    uint8_t reserved __attribute__((packed));
    // Direct colour fields (required for Direct/6 and YUV/7 memory models
    uint8_t redMaskSize __attribute__((packed));
    uint8_t redFieldPosition __attribute__((packed));
    uint8_t greenMaskSize __attribute__((packed));
    uint8_t greenFieldPosition __attribute__((packed));
    uint8_t blueMaskSize __attribute__((packed));
    uint8_t blueFieldPosition __attribute__((packed));
    uint8_t alphaMaskSize __attribute__((packed));
    uint8_t alphaFieldPosition __attribute__((packed));
    uint8_t directColourMode __attribute__((packed));
    // VESA 2.0 specific fields
    uint32_t physBasePtr __attribute__((packed));
    void* offScreenMemOffset __attribute__((packed));
    uint16_t offScreenMemSize __attribute__((packed));
    uint8_t paddington[461] __attribute__((packed));
};

struct ogVESAInfo {
    char VBESignature[4] __attribute__((packed));
    uint8_t minVersion __attribute__((packed));
    uint8_t majVersion __attribute__((packed));
    uint32_t OEMStringPtr __attribute__((packed));
    uint32_t capabilities __attribute__((packed));
    uint32_t videoModePtr __attribute__((packed));
    uint16_t totalMemory __attribute__((packed));
    // VESA 2.0 specific fields
    uint16_t OEMSoftwareRev __attribute__((packed));
    uint32_t OEMVendorNamePtr __attribute__((packed));
    uint32_t OEMProductNamePtr __attribute__((packed));
    uint32_t OEMProductRevPtr __attribute__((packed));
    uint8_t paddington[474] __attribute__((packed));
};

class ogDisplay_UbixOS: public ogSurface {
  protected:
    void * pages[2];
    uint32_t activePage;
    uint32_t visualPage;
    ogVESAInfo * VESAInfo;
    ogModeInfo * modeInfo;

    uint16_t FindMode(uint32_t, uint32_t, uint32_t);
    void GetModeInfo(uint16_t);
    void GetVESAInfo(void);
    void SetMode(uint16_t);
    void SetPal(void);
  public:
    ogDisplay_UbixOS(void);
    virtual bool ogAlias(ogSurface&, uint32_t, uint32_t, uint32_t, uint32_t);
    virtual bool ogClone(ogSurface&);
    virtual void ogCopyPalette(ogSurface&);
    virtual bool ogCreate(uint32_t, uint32_t, ogPixelFmt);
    virtual bool ogLoadPalette(const char *);
    virtual void ogSetPalette(const ogRGBA8[]);
    virtual void ogSetPalette(uint8_t, uint8_t, uint8_t, uint8_t);
    virtual void ogSetPalette(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
    virtual ~ogDisplay_UbixOS(void);
};
// ogDisplay_UbixOS

#endif
