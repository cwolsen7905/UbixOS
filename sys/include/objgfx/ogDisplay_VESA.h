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

#ifndef _OBJGFX_OGDISPLAY_VESA_H
#define _OBJGFX_OGDISPLAY_VESA_H

#include <sys/types.h>
#include <objgfx40/objgfx40.h>

struct TMode_Rec {
    uInt16 ModeAttributes __attribute__((packed));
    uInt8 WindowAFlags __attribute__((packed));
    uInt8 WindowBFlags __attribute__((packed));
    uInt16 Granularity __attribute__((packed));
    uInt16 WindowSize __attribute__((packed));
    uInt16 WindowASeg __attribute__((packed));
    uInt16 WindowBSeg __attribute__((packed));
    void* BankSwitch __attribute__((packed));
    uInt16 BytesPerLine __attribute__((packed));
    uInt16 xRes __attribute__((packed));
    uInt16 yRes __attribute__((packed));
    uInt8 CharWidth __attribute__((packed));
    uInt8 CharHeight __attribute__((packed));
    uInt8 NumBitPlanes __attribute__((packed));
    uInt8 BitsPerPixel __attribute__((packed));
    uInt8 NumberOfBanks __attribute__((packed));
    uInt8 MemoryModel __attribute__((packed));
    uInt8 BankSize __attribute__((packed));
    uInt8 NumOfImagePages __attribute__((packed));
    uInt8 Reserved __attribute__((packed));
    // Direct colour fields (required for Direct/6 and YUV/7 memory models
    uInt8 RedMaskSize __attribute__((packed));
    uInt8 RedFieldPosition __attribute__((packed));
    uInt8 GreenMaskSize __attribute__((packed));
    uInt8 GreenFieldPosition __attribute__((packed));
    uInt8 BlueMaskSize __attribute__((packed));
    uInt8 BlueFieldPosition __attribute__((packed));
    uInt8 AlphaMaskSize __attribute__((packed));
    uInt8 AlphaFieldPosition __attribute__((packed));
    uInt8 DirectColourMode __attribute__((packed));
    // VESA 2.0 specific fields
    uInt32 physBasePtr __attribute__((packed));
    void *OffScreenMemOffset __attribute__((packed));
    uInt16 OffScreenMemSize __attribute__((packed));
    uInt8 paddington[461] __attribute__((packed));
};

struct TVESA_Rec {
    char VBESignature[4] __attribute__((packed));
    uInt8 minVersion __attribute__((packed));
    uInt8 majVersion __attribute__((packed));
    uInt32 OEMStringPtr __attribute__((packed));
    uInt32 Capabilities __attribute__((packed));
    uInt32 VideoModePtr __attribute__((packed));
    uInt16 TotalMemory __attribute__((packed));
    // VESA 2.0 specific fields
    uInt16 OEMSoftwareRev __attribute__((packed));
    uInt32 OEMVendorNamePtr __attribute__((packed));
    uInt32 OEMProductNamePtr __attribute__((packed));
    uInt32 OEMProductRevPtr __attribute__((packed));
    uInt8 paddington[474] __attribute__((packed));
};

class ogDisplay_VESA: public ogSurface {
  protected:
    uInt16 ScreenSelector;
    TVESA_Rec* VESARec;
    TMode_Rec* ModeRec;
    bool InGraphics;
    uInt16 findMode(uInt32, uInt32, uInt32);
    void getModeInfo(uInt16);
    void getVESAInfo(void);
    void setMode(uInt16);
    virtual uInt32 rawGetPixel(uInt32, uInt32);
    virtual void rawSetPixel(uInt32, uInt32, uInt32);
    virtual void rawLine(uInt32, uInt32, uInt32, uInt32, uInt32);
    void setPal(void);
  public:
    ogDisplay_VESA(void);
    virtual bool ogAvail(void);
    virtual bool ogAlias(ogSurface&, uInt32, uInt32, uInt32, uInt32);
    virtual void ogClear(uInt32);
    virtual bool ogClone(ogSurface&);
    virtual void ogCopyLineTo(uInt32, uInt32, const void *, uInt32);
    virtual void ogCopyLineFrom(uInt32, uInt32, void *, uInt32);
    virtual void ogCopyPal(ogSurface&);
    virtual bool ogCreate(uInt32, uInt32, ogPixelFmt);
    virtual uInt32 ogGetPixel(int32, int32);
    virtual void * ogGetPtr(uInt32, uInt32);
    virtual void ogHLine(int32, int32, int32, uInt32);
    virtual bool ogLoadPal(const char *);
    virtual void ogSetPixel(int32, int32, uInt32);
    virtual void ogSetRGBPalette(uInt8, uInt8, uInt8, uInt8);
    virtual void ogVFlip(void);
    virtual void ogVLine(int32, int32, int32, uInt32);
    virtual ~ogDisplay_VESA(void);
};
// ogDisplay_VESA

#endif
