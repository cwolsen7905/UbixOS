/**************************************************************
$Id: objgfx40.h 89 2016-01-12 00:20:40Z reddawg $
**************************************************************/

#ifndef OBJGFX40_H
#define OBJGFX40_H

#include "ogTypes.h"
#include "ogPixelFmt.h"

#ifndef __UBIXOS_KERNEL__
#include <stdlib.h> // for NULL, true, false
#endif

#define ogVERSION 4.0;

class ogPixelFmt;

extern const uInt32 OG_MASKS[32];

#if 0
typedef 
  class ogPixelFmt {
   public:
    uInt8 BPP;
    uInt8 redFieldPosition;
    uInt8 greenFieldPosition;
    uInt8 blueFieldPosition;
    uInt8 alphaFieldPosition;
    uInt8 redMaskSize;
    uInt8 greenMaskSize;
    uInt8 blueMaskSize;
    uInt8 alphaMaskSize;
    uInt8 reserved[7];
             ogPixelFmt(uInt8, uInt8, uInt8, uInt8, uInt8, 
                        uInt8, uInt8, uInt8, uInt8);
    virtual ~ogPixelFmt(void) {};
  };
// Default pixel formats
const ogPixelFmt OG_NULL_PIXFMT      = { 0, 0,0,0,0,0,0,0,0, {0,0,0,0,0,0}};
const ogPixelFmt OG_PIXFMT_8BPP      = { 8, 0,0,0,0,0,0,0,0, {0,0,0,0,0,0}};
const ogPixelFmt OG_PIXFMT_15BPP     = {15, 10,5,0,15,5,5,5,1, {0,0,0,0,0,0}};
const ogPixelFmt OG_PIXFMT_16BPP     = {16, 11,5,0,0,5,6,5,0, {0,0,0,0,0,0}};
const ogPixelFmt OG_PIXFMT_24BPP     = {24, 16,8,0,0,8,8,8,0, {0,0,0,0,0,0}};
const ogPixelFmt OG_PIXFMT_32BPP     = {32, 16,8,0,24,8,8,8,8, {0,0,0,0,0,0}};
const ogPixelFmt OG_MAC_PIXFMT_16BPP = {16, 8,4,0,12,4,4,4,4, {0,0,0,0,0,0}};
#endif

#if 0
class 
  ogAttributes(uInt32 transparentColour = 0, 
                 uInt32 defaultAlpha = 255,
                 bool antiAlias = true,
                 bool blending = false);
#endif        

class ogAttributes {
 public:
  uInt32 transparentColor;
  uInt32 defaultAlpha;
  bool   antiAlias;
  bool   blending;
  ogAttributes():transparentColor(0), 
                 defaultAlpha(255),
                 antiAlias(true),
                 blending(false) { } 
  ogAttributes & operator=( ogAttributes const & copy ) {
    transparentColor = copy.transparentColor;
    defaultAlpha = copy.defaultAlpha;
    antiAlias = copy.antiAlias;
    blending = copy.blending;
    return * this;
  } // operator =
}; // ogAttributes

class ogSurface {
#ifdef __UBIXOS_KERNEL__
 public:
#else
 protected:
#endif
  float        version;
  void       * buffer;
  ogSurface  * owner;
  uInt32     * lineOfs;
  ogRGBA8    * pal;
  ogAttributes*attributes;

  uInt32       xRes, yRes;
  uInt32       maxX, maxY;
  uInt32       bSize;       // buffer size (in bytes)
  uInt32       lSize;       // LineOfs size (in bytes)

  uInt32       BPP;         // bits per pixel
  uInt32       bytesPerPix;
  uInt32       pixFmtID;

  uInt32       redFieldPosition;
  uInt32       greenFieldPosition;
  uInt32       blueFieldPosition;
  uInt32       alphaFieldPosition;

  uInt32       redShifter;
  uInt32       greenShifter;
  uInt32       blueShifter;
  uInt32       alphaShifter;
  uInt32       alphaMasker;
  ogErrorCode  lastError;
  ogDataState  dataState;

  bool         ClipLine(int32&, int32&, int32&, int32&);
  void         RawLine(uInt32, uInt32, uInt32, uInt32, uInt32);
  virtual uInt32 RawGetPixel(uInt32, uInt32);
  virtual void RawSetPixel(uInt32, uInt32, uInt32);
  virtual void RawSetPixel(uInt32, uInt32, uInt8, uInt8, uInt8, uInt8);
  void         AARawLine(uInt32, uInt32, uInt32, uInt32, uInt32);
 public:
               ogSurface(void);
  virtual bool ogAlias(ogSurface&, uInt32, uInt32, uInt32, uInt32);
  virtual bool ogAvail(void);
  void         ogArc(int32, int32, uInt32, uInt32, uInt32, uInt32);
  void         ogBSpline(uInt32, ogPoint2d*, uInt32, uInt32);
  void         ogCircle(int32, int32, uInt32, uInt32);
  virtual void ogClear(uInt32);
  virtual bool ogClone(ogSurface&);
  void         ogCopy(ogSurface&);
  void         ogCopyBuf(int32, int32,
                 ogSurface&, int32, int32, int32, int32);
  virtual void ogCopyLineTo(uInt32, uInt32, const void *, uInt32);
  virtual void ogCopyLineFrom(uInt32, uInt32, void *, uInt32);
  virtual void ogCopyPalette(ogSurface&);
  virtual bool ogCreate(uInt32, uInt32, ogPixelFmt);
  void         ogCubicBezierCurve(int32, int32, int32, int32,
                 int32, int32, int32, int32, uInt32, uInt32);
  void         ogCurve(int32,int32, int32,int32, int32,int32, uInt32, uInt32);
  void         ogFillCircle(int32, int32, uInt32, uInt32);
  void         ogFillGouraudPolygon(uInt32, ogPoint2d*, ogRGBA8 *);
  void         ogFillPolygon(uInt32, ogPoint2d*, uInt32);
  void         ogFillRect(int32, int32, int32, int32, uInt32);
  void         ogFillTriangle(int32,int32, int32,int32, int32,int32, uInt32);
  uInt32       ogGetAlpha(void);
  uInt32       ogGetAlphaMasker(void) const { return alphaMasker; }
  uInt32       ogGetBPP(void) const { return BPP; }
  uInt32       ogGetBytesPerPix(void) const { return bytesPerPix; }
  ogDataState  ogGetDataState(void) const { return dataState; }
  ogErrorCode  ogGetLastError(void);
  uInt32       ogGetMaxX(void) const { return maxX; }
  uInt32       ogGetMaxY(void) const { return maxY; }
  void         ogGetPalette(ogRGBA8[]);
  void         ogGetPixFmt(ogPixelFmt&);
  uInt32       ogGetPixFmtID(void) const { return pixFmtID; }
  virtual uInt32 ogGetPixel(int32, int32);
  virtual void * ogGetPtr(uInt32, uInt32);
  uInt32       ogGetTransparentColor(void); 
  void         ogHFlip(void);
  virtual void ogHLine(int32, int32, int32, uInt32);
  bool         ogIsAntiAliasing(void);
  bool         ogIsBlending(void); 
  void         ogLine(int32, int32, int32, int32, uInt32);
  virtual bool ogLoadPalette(const char *);
  void         ogPolygon(uInt32, ogPoint2d*, uInt32);
  void         ogRect(int32, int32, int32, int32, uInt32);
  uInt32       ogPack(uInt8, uInt8, uInt8);
  uInt32       ogPack(uInt8, uInt8, uInt8, uInt8);
  bool         ogSavePalette(const char *);
  void         ogScale(ogSurface&);
  void         ogScaleBuf(int32, int32, int32, int32,
                 ogSurface&, int32, int32, int32, int32);
  uInt32       ogSetAlpha(uInt32);
  bool         ogSetAntiAliasing(bool);
  bool         ogSetBlending(bool);
  virtual ogErrorCode ogSetLastError(ogErrorCode);
  virtual void ogSetPixel(int32, int32, uInt32);
  virtual void ogSetPixel(int32, int32, uInt8, uInt8, uInt8, uInt8);
  virtual void ogSetPalette(const ogRGBA8[]);
  virtual void ogSetPalette(uInt8, uInt8, uInt8, uInt8, uInt8);
  virtual void ogSetPalette(uInt8, uInt8, uInt8, uInt8);
  uInt32       ogSetTransparentColor(uInt32);
  void         ogSpline(uInt32, ogPoint2d*, uInt32, uInt32);
  void         ogTriangle(int32, int32, int32, int32, int32, int32, uInt32);
  void         ogUnpack(uInt32, uInt8&, uInt8&, uInt8&);
  void         ogUnpack(uInt32, uInt8&, uInt8&, uInt8&, uInt8&);
  virtual void ogVFlip(void);
  virtual void ogVLine(int32, int32, int32, uInt32);
  virtual      ~ogSurface(void);
}; // ogSurface

#endif
