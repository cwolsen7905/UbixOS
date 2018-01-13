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

/*
 * objgfx30.h
 *
 *  Created on: Jan 12, 2018
 *      Author: cwolsen
 */

#ifndef SYS_INCLUDE_OBJGFX_OBJGFX30_H_
#define SYS_INCLUDE_OBJGFX_OBJGFX30_H_

#include <stdlib.h> // for NULL

#define RadToDeg 180.0/3.14159265358979;

typedef signed char Int8;
typedef signed short int Int16;
typedef signed long int Int32;

typedef unsigned char uInt8;
typedef unsigned short int uInt16;
typedef unsigned long int uInt32;

enum ogDataState {
  ogNONE, ogOWNER, ogALIASING
};

typedef struct {
    uInt8 red;
    uInt8 green;
    uInt8 blue;
} TRGB;

typedef struct {
    uInt8 red;
    uInt8 green;
    uInt8 blue;
    uInt8 alpha;
} TRGBA;

typedef struct {
    uInt16 ModeAttributes;
    uInt8 WindowAFlags;
    uInt8 WindowBFlags;
    uInt16 Granularity;
    uInt16 WindowSize;
    uInt16 WindowASeg;
    uInt16 WindowBSeg;
    void* BankSwitch;
    uInt16 BytesPerLine;
    uInt16 xRes, yRes;
    uInt8 CharWidth;
    uInt8 CharHeight;
    uInt8 NumBitPlanes;
    uInt8 BitsPerPixel;
    uInt8 NumberOfBanks;
    uInt8 MemoryModel;
    uInt8 BankSize;
    uInt8 NumOfImagePages;
    uInt8 Reserved;
    // Direct colour fields (required for Direct/6 and YUV/7 memory models
    uInt8 RedMaskSize;
    uInt8 RedFieldPosition;
    uInt8 GreenMaskSize;
    uInt8 GreenFieldPosition;
    uInt8 BlueMaskSize;
    uInt8 BlueFieldPosition;
    uInt8 AlphaMaskSize;
    uInt8 AlphaFieldPosition;
    uInt8 DirectColourMode;
    // VESA 2.0 specific fields
    uInt32 PhysBasePtr;
    void* OffScreenMemOffset;
    uInt16 OffScreenMemSize;
    uInt8 paddington[461];
} TMode_Rec;

typedef struct {
    char VBESignature[4];
    uInt8 minVersion;
    uInt8 majVersion;
    char * OEMStringPtr;
    uInt32 Capabilities;
    uInt16* VideoModePtr;
    uInt16 TotalMemory;
    // VESA 2.0 specific fields
    uInt16 OEMSoftwareRev;
    char * OEMVendorNamePtr;
    char * OEMProductNamePtr;
    char * OEMProductRevPtr;
    uInt8 paddington[474];
} TVESA_Rec;

typedef struct {
    Int32 x;
    Int32 y;
} TPoint;

typedef struct {
    uInt8 BPP;
    uInt8 RedFieldPosition;
    uInt8 GreenFieldPosition;
    uInt8 BlueFieldPosition;
    uInt8 AlphaFieldPosition;
    uInt8 RedMaskSize;
    uInt8 GreenMaskSize;
    uInt8 BlueMaskSize;
    uInt8 AlphaMaskSize;
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
    uInt32 xRes, yRes;
    uInt32 MaxX, MaxY;
    uInt32 bSize;       // buffer size (in bytes)
    uInt32 lSize;       // LineOfs size (in bytes)
    uInt32 TransparentColor;
    ogDataState DataState;
    uInt8 BPP;         // bits per pixel
    uInt8 RedFieldPosition;
    uInt8 GreenFieldPosition;
    uInt8 BlueFieldPosition;
    uInt8AlphaFieldPosition;
    uInt8 RedShifter;
    uInt8 GreenShifter;
    uInt8 BlueShifter;
    uInt8AlphaShifter;
    bool AntiAlias;
    bool clipLine(Int32&, Int32&, Int32&, Int32&);
    void rawLine(uInt32, uInt32, uInt32, uInt32, uInt32);
    void aaRawLine(uInt32, uInt32, uInt32, uInt32, uInt32);
  public:
    void* Buffer;
    uInt32* LineOfs;
    TRGB* pal;
    TGfx0(void);
    bool ogAlias(TGfx0&, uInt32, uInt32, uInt32, uInt32);
    void ogArc(Int32, Int32, uInt32, uInt32, uInt32, uInt32);
    void ogBSpline(uInt32, TPoint*, uInt32, uInt32);
    void ogCircle(Int32, Int32, uInt32, uInt32);
    void ogClear(uInt32);
    bool ogClone(TGfx0&);
    void ogCopy(TGfx0&);
    void ogCopyBuf(Int32, Int32, TGfx0&, Int32, Int32, Int32, Int32);
    bool ogCreate(uInt32, uInt32, TPixelFmt);
    void ogCubicBezierCurve(Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, uInt32, uInt32);
    void ogCurve(Int32, Int32, Int32, Int32, Int32, Int32, uInt32, uInt32);
    void ogFillCircle(Int32, Int32, uInt32, uInt32);
    void ogFillConvexPolygon(uInt32, TPoint*, uInt32);
    void ogFillPolygon(uInt32, TPoint*, uInt32);
    void ogFillRect(Int32, Int32, Int32, Int32, uInt32);
    void ogFillTriangle(Int32, Int32, Int32, Int32, Int32, Int32, uInt32);
    bool ogGetAntiAlias(void);
    uInt8 ogGetBPP(void);
    ogDataState ogGetDataState(void);
    uInt32 ogGetMaxX(void);
    uInt32 ogGetMaxY(void);
    void ogGetPal(void);
    void ogGetPixFmt(TPixelFmt&);
    uInt32 ogGetPixel(Int32, Int32);
    uInt32 ogGetTransparentColor(void);
    void ogHFlip(void);
    void ogHLine(Int32, Int32, Int32, uInt32);
    void ogLine(Int32, Int32, Int32, Int32, uInt32);
    void ogLoadPal(const char *);
    void ogPolygon(uInt32, TPoint*, uInt32);
    void ogRect(Int32, Int32, Int32, Int32, uInt32);
    uInt32 ogRGB(uInt8, uInt8, uInt8);
    void ogSavePal(const char *);
    void ogScaleBuf(Int32, Int32, Int32, Int32, TGfx0&, Int32, Int32, Int32, Int32);
    bool ogSetAntiAlias(bool);
    void ogSetPixel(uInt32, uInt32, uInt32);
    void ogSetRGBPalette(uInt8, uInt8, uInt8, uInt8);
    uInt32 ogSetTransparentColor(uInt32);
    void ogSpline(uInt32, TPoint*, uInt32, uInt32);
    void ogTriangle(Int32, Int32, Int32, Int32, Int32, Int32, uInt32);
    void ogUnpackRGB(uInt32, uInt8&, uInt8&, uInt8&);
    void ogVFlip(void);
    void ogVLine(Int32, Int32, Int32, uInt32);
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
    void setupMode(uInt16);
    ~TScreen(void);
};
// TScreen

#endif /* END SYS_INCLUDE_OBJGFX_OBJGFX30_H_ */
