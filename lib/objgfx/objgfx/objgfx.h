#pragma once

#include <functional>
#include <map>

#include "ogTypes.h"
#include "ogPixelFmt.h"

class ogSurface 
{
private:
	const static double INTENSITIES[32];

protected:

	double       version;
	uInt8      * buffer;
	ogSurface  * owner;
	ptrdiff_t  * lineOfs;
	ogRGBA8    * pal;
	ogAttribute* attributes;
	
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

	std::function<uInt32(void*)> getPixel;
	std::function<void(void*, uInt32)> setPixel;

	void         AARawLine(uInt32, uInt32, uInt32, uInt32, uInt32);
	bool         ClipLine(int32&, int32&, int32&, int32&);
	virtual uInt32 RawGetPixel(uInt32, uInt32);
	void         RawLine(uInt32, uInt32, uInt32, uInt32, uInt32);
	virtual void RawSetPixel(uInt32, uInt32, uInt32);
	virtual void RawSetPixel(uInt32, uInt32, uInt8, uInt8, uInt8, uInt8);

public:
	
				 ogSurface(void);
	virtual bool ogAlias(ogSurface&, uInt32, uInt32, uInt32, uInt32);
	virtual bool ogAvail(void);
	void         ogArc(int32, int32, uInt32, uInt32, uInt32, uInt32);
	void         ogBSpline(uInt32, ogPoint2d*, uInt32, uInt32);
	void         ogCircle(int32, int32, uInt32, uInt32);
	virtual void ogClear(uInt32);
	void		 ogClear();
	virtual bool ogClone(ogSurface&);
	void         ogCopy(ogSurface&);
	void         ogCopyBuf(int32 destX, int32 destY, ogSurface&, 
					int32 sourceX1, int32 sourceY1, int32 sourceX2, int32 sourceY2);
	virtual void ogCopyLineTo(uInt32, uInt32, const void *, uInt32);
	virtual void ogCopyLineFrom(uInt32, uInt32, void *, uInt32);
	virtual void ogCopyPalette(ogSurface&);
	virtual bool ogCreate(uInt32 _xRes, uInt32 _yRes, struct ogPixelFmt _pixFormat);
	void         ogCubicBezierCurve(int32, int32, int32, int32,
					int32, int32, int32, int32, uInt32, uInt32);
	void         ogCurve(int32,int32, int32,int32, int32,int32, uInt32, uInt32);
	void         ogFillCircle(int32, int32, uInt32, uInt32);
	void         ogFillGouraudPolygon(uInt32, ogPoint2d*, ogRGBA8 *);
	void         ogFillPolygon(uInt32, ogPoint2d*, uInt32);
	void         ogFillRect(int32, int32, int32, int32, uInt32);
	void         ogFillTriangle(int32, int32, int32, int32, int32, int32, uInt32);
	uInt32       ogGetAlpha(void);
	uInt32       ogGetAlphaMasker(void) const { return alphaMasker; }
	uInt32       ogGetBPP(void) const { return BPP; }
	uInt32		 ogGetColorCount(); // should probably be bigger
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
	virtual void ogHLine(int32 x1, int32 x2, int32 y, uInt32 colour);
	bool         ogIsAntiAliasing(void);
	bool         ogIsBlending(void); 
	void         ogLine(int32, int32, int32, int32, uInt32);
	virtual bool ogLoadPalette(const char *);
	void		 ogOptimize();
	void         ogPolygon(uInt32, ogPoint2d*, uInt32);
	uInt32       ogPack(uInt8, uInt8, uInt8);
	uInt32       ogPack(uInt8, uInt8, uInt8, uInt8);
	void         ogRect(int32, int32, int32, int32, uInt32);
	bool         ogSavePalette(const char *);
	void         ogScale(ogSurface&);
	void         ogScaleBuf(int32, int32, int32, int32, ogSurface&, int32, int32, int32, int32);
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
	virtual void ogVLine(int32 x, int32 y1, int32 y2, uInt32 colour);
	virtual      ~ogSurface(void);
}; // class ogSurface

