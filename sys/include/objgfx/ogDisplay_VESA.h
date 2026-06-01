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

#ifndef _OBJGFX_OGDISPLAY_VESA_H
#define _OBJGFX_OGDISPLAY_VESA_H

#include <sys/types.h>
#include <objgfx40/objgfx40.h>

struct TMode_Rec {
    u_int16_t ModeAttributes __attribute__((packed));
    u_int8_t WindowAFlags __attribute__((packed));
    u_int8_t WindowBFlags __attribute__((packed));
    u_int16_t Granularity __attribute__((packed));
    u_int16_t WindowSize __attribute__((packed));
    u_int16_t WindowASeg __attribute__((packed));
    u_int16_t WindowBSeg __attribute__((packed));
    void* BankSwitch __attribute__((packed));
    u_int16_t BytesPerLine __attribute__((packed));
    u_int16_t xRes __attribute__((packed));
    u_int16_t yRes __attribute__((packed));
    u_int8_t CharWidth __attribute__((packed));
    u_int8_t CharHeight __attribute__((packed));
    u_int8_t NumBitPlanes __attribute__((packed));
    u_int8_t BitsPerPixel __attribute__((packed));
    u_int8_t NumberOfBanks __attribute__((packed));
    u_int8_t MemoryModel __attribute__((packed));
    u_int8_t BankSize __attribute__((packed));
    u_int8_t NumOfImagePages __attribute__((packed));
    u_int8_t Reserved __attribute__((packed));
    // Direct colour fields (required for Direct/6 and YUV/7 memory models
    u_int8_t RedMaskSize __attribute__((packed));
    u_int8_t RedFieldPosition __attribute__((packed));
    u_int8_t GreenMaskSize __attribute__((packed));
    u_int8_t GreenFieldPosition __attribute__((packed));
    u_int8_t BlueMaskSize __attribute__((packed));
    u_int8_t BlueFieldPosition __attribute__((packed));
    u_int8_t AlphaMaskSize __attribute__((packed));
    u_int8_t AlphaFieldPosition __attribute__((packed));
    u_int8_t DirectColourMode __attribute__((packed));
    // VESA 2.0 specific fields
    u_int32_t physBasePtr __attribute__((packed));
    void *OffScreenMemOffset __attribute__((packed));
    u_int16_t OffScreenMemSize __attribute__((packed));
    u_int8_t paddington[461] __attribute__((packed));
};

struct TVESA_Rec {
    char VBESignature[4] __attribute__((packed));
    u_int8_t minVersion __attribute__((packed));
    u_int8_t majVersion __attribute__((packed));
    u_int32_t OEMStringPtr __attribute__((packed));
    u_int32_t Capabilities __attribute__((packed));
    u_int32_t VideoModePtr __attribute__((packed));
    u_int16_t TotalMemory __attribute__((packed));
    // VESA 2.0 specific fields
    u_int16_t OEMSoftwareRev __attribute__((packed));
    u_int32_t OEMVendorNamePtr __attribute__((packed));
    u_int32_t OEMProductNamePtr __attribute__((packed));
    u_int32_t OEMProductRevPtr __attribute__((packed));
    u_int8_t paddington[474] __attribute__((packed));
};

class ogDisplay_VESA: public ogSurface {
  protected:
    u_int16_t ScreenSelector;
    TVESA_Rec* VESARec;
    TMode_Rec* ModeRec;
    bool InGraphics;
    u_int16_t findMode(u_int32_t, u_int32_t, u_int32_t);
    void getModeInfo(u_int16_t);
    void getVESAInfo(void);
    void setMode(u_int16_t);
    virtual u_int32_t rawGetPixel(u_int32_t, u_int32_t);
    virtual void rawSetPixel(u_int32_t, u_int32_t, u_int32_t);
    virtual void rawLine(u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t);
    void setPal(void);
  public:
    ogDisplay_VESA(void);
    virtual bool ogAvail(void);
    virtual bool ogAlias(ogSurface&, u_int32_t, u_int32_t, u_int32_t, u_int32_t);
    virtual void ogClear(u_int32_t);
    virtual bool ogClone(ogSurface&);
    virtual void ogCopyLineTo(u_int32_t, u_int32_t, const void *, u_int32_t);
    virtual void ogCopyLineFrom(u_int32_t, u_int32_t, void *, u_int32_t);
    virtual void ogCopyPal(ogSurface&);
    virtual bool ogCreate(u_int32_t, u_int32_t, ogPixelFmt);
    virtual u_int32_t ogGetPixel(int32, int32);
    virtual void * ogGetPtr(u_int32_t, u_int32_t);
    virtual void ogHLine(int32, int32, int32, u_int32_t);
    virtual bool ogLoadPal(const char *);
    virtual void ogSetPixel(int32, int32, u_int32_t);
    virtual void ogSetRGBPalette(u_int8_t, u_int8_t, u_int8_t, u_int8_t);
    virtual void ogVFlip(void);
    virtual void ogVLine(int32, int32, int32, u_int32_t);
    virtual ~ogDisplay_VESA(void);
};
// ogDisplay_VESA

#endif
