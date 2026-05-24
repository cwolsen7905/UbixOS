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

/*
 * objgfx30.h
 *
 *  Created on: Jan 12, 2018
 *      Author: cwolsen
 */

#ifndef SYS_INCLUDE_OBJGFX_OBJGFX30_H_
#define SYS_INCLUDE_OBJGFX_OBJGFX30_H_

#include <sys/types.h>

#define RadToDeg 180.0/3.14159265358979;

typedef signed char Int8;
typedef signed short int Int16;
typedef signed long int Int32;

enum ogDataState {
  ogNONE, ogOWNER, ogALIASING
};

typedef struct {
    u_int8_t red;
    u_int8_t green;
    u_int8_t blue;
} TRGB;

typedef struct {
    u_int8_t red;
    u_int8_t green;
    u_int8_t blue;
    u_int8_t alpha;
} TRGBA;

typedef struct {
    u_int16_t ModeAttributes;
    u_int8_t WindowAFlags;
    u_int8_t WindowBFlags;
    u_int16_t Granularity;
    u_int16_t WindowSize;
    u_int16_t WindowASeg;
    u_int16_t WindowBSeg;
    void* BankSwitch;
    u_int16_t BytesPerLine;
    u_int16_t xRes, yRes;
    u_int8_t CharWidth;
    u_int8_t CharHeight;
    u_int8_t NumBitPlanes;
    u_int8_t BitsPerPixel;
    u_int8_t NumberOfBanks;
    u_int8_t MemoryModel;
    u_int8_t BankSize;
    u_int8_t NumOfImagePages;
    u_int8_t Reserved;
    // Direct colour fields (required for Direct/6 and YUV/7 memory models
    u_int8_t RedMaskSize;
    u_int8_t RedFieldPosition;
    u_int8_t GreenMaskSize;
    u_int8_t GreenFieldPosition;
    u_int8_t BlueMaskSize;
    u_int8_t BlueFieldPosition;
    u_int8_t AlphaMaskSize;
    u_int8_t AlphaFieldPosition;
    u_int8_t DirectColourMode;
    // VESA 2.0 specific fields
    u_int32_t PhysBasePtr;
    void* OffScreenMemOffset;
    u_int16_t OffScreenMemSize;
    u_int8_t paddington[461];
} TMode_Rec;

typedef struct {
    char VBESignature[4];
    u_int8_t minVersion;
    u_int8_t majVersion;
    char * OEMStringPtr;
    u_int32_t Capabilities;
    u_int16_t* VideoModePtr;
    u_int16_t TotalMemory;
    // VESA 2.0 specific fields
    u_int16_t OEMSoftwareRev;
    char * OEMVendorNamePtr;
    char * OEMProductNamePtr;
    char * OEMProductRevPtr;
    u_int8_t paddington[474];
} TVESA_Rec;

typedef struct {
    Int32 x;
    Int32 y;
} TPoint;

typedef struct {
    u_int8_t BPP;
    u_int8_t RedFieldPosition;
    u_int8_t GreenFieldPosition;
    u_int8_t BlueFieldPosition;
    u_int8_t AlphaFieldPosition;
    u_int8_t RedMaskSize;
    u_int8_t GreenMaskSize;
    u_int8_t BlueMaskSize;
    u_int8_t AlphaMaskSize;
} TPixelFmt;

// Default pixel formats

const TPixelFmt NULL_PIXFMT = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
const TPixelFmt DEF_PIXFMT_8BPP = { 8, 0, 0, 0, 0, 0, 0, 0, 0 };
const TPixelFmt DEF_PIXFMT_15BPP = { 15, 10, 5, 0, 15, 5, 5, 5, 1 };
const TPixelFmt DEF_PIXFMT_16BPP = { 16, 11, 5, 0, 0, 5, 6, 5, 0 };
const TPixelFmt DEF_PIXFMT_24BPP = { 24, 16, 8, 0, 8, 8, 8 };
const TPixelFmt DEF_PIXFMT_32BPP = { 32, 16, 8, 0, 24, 8, 8, 8, 8 };
const TPixelFmt DEF_MAC_PIXFMT_16BPP = { 16, 8, 4, 0, 12, 4, 4, 4, 4 };

#include "defpal.inc"

class TGfx0 {
  protected:
    TGfx0* Owner;
    u_int32_t xRes, yRes;
    u_int32_t MaxX, MaxY;
    u_int32_t bSize;       // buffer size (in bytes)
    u_int32_t lSize;       // LineOfs size (in bytes)
    u_int32_t TransparentColor;
    ogDataState DataState;
    u_int8_t BPP;         // bits per pixel
    u_int8_t RedFieldPosition;
    u_int8_t GreenFieldPosition;
    u_int8_t BlueFieldPosition;
    uInt8AlphaFieldPosition;
    u_int8_t RedShifter;
    u_int8_t GreenShifter;
    u_int8_t BlueShifter;
    uInt8AlphaShifter;
    bool AntiAlias;
    bool clipLine(Int32&, Int32&, Int32&, Int32&);
    void rawLine(u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t);
    void aaRawLine(u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t);
  public:
    void* Buffer;
    u_int32_t* LineOfs;
    TRGB* pal;
    TGfx0(void);
    bool ogAlias(TGfx0&, u_int32_t, u_int32_t, u_int32_t, u_int32_t);
    void ogArc(Int32, Int32, u_int32_t, u_int32_t, u_int32_t, u_int32_t);
    void ogBSpline(u_int32_t, TPoint*, u_int32_t, u_int32_t);
    void ogCircle(Int32, Int32, u_int32_t, u_int32_t);
    void ogClear(u_int32_t);
    bool ogClone(TGfx0&);
    void ogCopy(TGfx0&);
    void ogCopyBuf(Int32, Int32, TGfx0&, Int32, Int32, Int32, Int32);
    bool ogCreate(u_int32_t, u_int32_t, TPixelFmt);
    void ogCubicBezierCurve(Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, u_int32_t, u_int32_t);
    void ogCurve(Int32, Int32, Int32, Int32, Int32, Int32, u_int32_t, u_int32_t);
    void ogFillCircle(Int32, Int32, u_int32_t, u_int32_t);
    void ogFillConvexPolygon(u_int32_t, TPoint*, u_int32_t);
    void ogFillPolygon(u_int32_t, TPoint*, u_int32_t);
    void ogFillRect(Int32, Int32, Int32, Int32, u_int32_t);
    void ogFillTriangle(Int32, Int32, Int32, Int32, Int32, Int32, u_int32_t);
    bool ogGetAntiAlias(void);
    u_int8_t ogGetBPP(void);
    ogDataState ogGetDataState(void);
    u_int32_t ogGetMaxX(void);
    u_int32_t ogGetMaxY(void);
    void ogGetPal(void);
    void ogGetPixFmt(TPixelFmt&);
    u_int32_t ogGetPixel(Int32, Int32);
    u_int32_t ogGetTransparentColor(void);
    void ogHFlip(void);
    void ogHLine(Int32, Int32, Int32, u_int32_t);
    void ogLine(Int32, Int32, Int32, Int32, u_int32_t);
    void ogLoadPal(const char *);
    void ogPolygon(u_int32_t, TPoint*, u_int32_t);
    void ogRect(Int32, Int32, Int32, Int32, u_int32_t);
    u_int32_t ogRGB(u_int8_t, u_int8_t, u_int8_t);
    void ogSavePal(const char *);
    void ogScaleBuf(Int32, Int32, Int32, Int32, TGfx0&, Int32, Int32, Int32, Int32);
    bool ogSetAntiAlias(bool);
    void ogSetPixel(u_int32_t, u_int32_t, u_int32_t);
    void ogSetRGBPalette(u_int8_t, u_int8_t, u_int8_t, u_int8_t);
    u_int32_t ogSetTransparentColor(u_int32_t);
    void ogSpline(u_int32_t, TPoint*, u_int32_t, u_int32_t);
    void ogTriangle(Int32, Int32, Int32, Int32, Int32, Int32, u_int32_t);
    void ogUnpackRGB(u_int32_t, u_int8_t&, u_int8_t&, u_int8_t&);
    void ogVFlip(void);
    void ogVLine(Int32, Int32, Int32, u_int32_t);
    ~TGfx0(void);
};
// TGfx0

class TScreen: public TGfx0 {
  protected:
    TVESA_Rec* VESARec;
    TMode_Rec* ModeRec;
    bool InGraphics;
  public:
    TScreen(void);
    void setupMode(u_int16_t);
    ~TScreen(void);
};
// TScreen

#endif /* END SYS_INCLUDE_OBJGFX_OBJGFX30_H_ */
