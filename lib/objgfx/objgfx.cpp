#include <map>
#include <functional>
#include <iostream>
#include <fstream>
#include <math.h>

#include "ogTypes.h"
#include "ogEdgeTable.h"
#include "ogPixCon.h"
#include "ogPixelFmt.h"
#include "objgfx.h"

const static ogRGBA8 DEFAULT_PALETTE[256] =
	{{0,   0,   0,   255},       // 0
	 {0,   0,   170, 255},
	 {0,   170, 0,   255},
	 {0,   170, 170, 255},   // 3
	 {170, 0,   0,   255},
	 {170, 0,   170, 255},
	 {170, 85,  0,   255},
	 {170, 170, 170, 255}, // 7
	 {85,  85,  85,  255},
	 {85,  85,  255, 255},
	 {85,  255, 85,  255},
	 {85,  255, 255, 255},  // 11
	 {255, 85,  85,  255},
	 {255, 85,  255, 255},
	 {255, 255, 85,  255},
	 {255, 255, 255, 255}, //15
	 {16,  16,  16,  255},    // 16
	 {32,  32,  32,  255},
	 {48,  48,  48,  255},
	 {64,  64,  64,  255},
	 {80,  80,  80,  255},
	 {96,  96,  96,  255},
	 {112, 112, 112, 255},
	 {128, 128, 128, 255},
	 {144, 144, 144, 255},
	 {160, 160, 160, 255},
	 {176, 176, 176, 255},
	 {192, 192, 192, 255},
	 {208, 208, 208, 255},
	 {224, 224, 224, 255},
	 {240, 240, 240, 255},
	 {255, 255, 255, 255}, //31
	 {59,  0,   0,   255},      // 32
	 {79,  0,   0,   255},
	 {103, 0,   0,   255},
	 {123, 0,   0,   255},
	 {143, 7,   7,   255},
	 {167, 7,   7,   255},
	 {187, 11,  11,  255},
	 {211, 15,  15,  255},
	 {231, 19,  19,  255},
	 {255, 27,  27,  255},
	 {255, 59,  59,  255},
	 {255, 91,  91,  255},
	 {255, 119, 119, 255},
	 {255, 151, 151, 255},
	 {255, 183, 183, 255},
	 {255, 215, 215, 255},
	 {55,  55,  0,   255},   // 48
	 {71,  71,  0,   255},
	 {87,  87,  0,   255},
	 {103, 103, 7,   255},
	 {119, 119, 7,   255},
	 {135, 135, 11,  255},
	 {155, 155, 19,  255},
	 {171, 171, 23,  255},
	 {187, 187, 31,  255},
	 {203, 203, 35,  255},
	 {219, 219, 43,  255},
	 {239, 239, 59,  255},
	 {255, 255, 63,  255},
	 {255, 255, 127, 255},
	 {255, 255, 187, 255},
	 {255, 255, 255, 255},
	 {0,   43,  0,   255},   // 64
	 {0,   63,  0,   255},
	 {0,   83,  0,   255},
	 {0,   103, 0,   255},
	 {7,   127, 7,   255},
	 {7,   147, 7,   255},
	 {11,  167, 11,  255},
	 {15,  187, 15,  255},
	 {19,  211, 19,  255},
	 {27,  231, 27,  255},
	 {59,  235, 59,  255},
	 {91,  239, 91,  255},
	 {127, 239, 127, 255},
	 {159, 243, 159, 255},
	 {195, 247, 195, 255},
	 {231, 251, 231, 255},
	 {0,   55,  55,  255},   // 80
	 {0,   71,  71,  255},
	 {0,   87,  87,  255},
	 {7,   103, 103, 255},
	 {7,   119, 119, 255},
	 {11,  135, 135, 255},
	 {19,  155, 155, 255},
	 {23,  171, 171, 255},
	 {31,  187, 187, 255},
	 {35,  203, 203, 255},
	 {43,  219, 219, 255},
	 {51,  235, 235, 255},
	 {63,  255, 255, 255},
	 {127, 255, 255, 255},
	 {187, 255, 255, 255},
	 {255, 255, 255, 255},
	 {15,  15,  55,  255},   // 96
	 {19,  19,  79,  255},
	 {27,  27,  103, 255},
	 {31,  31,  127, 255},
	 {35,  35,  155, 255},
	 {39,  39,  179, 255},
	 {43,  43,  203, 255},
	 {47,  47,  227, 255},
	 {51,  51,  255, 255},
	 {71,  71,  255, 255},
	 {91,  91,  255, 255},
	 {111, 111, 255, 255},
	 {131, 131, 255, 255},
	 {151, 151, 255, 255},
	 {175, 175, 255, 255},
	 {195, 195, 255, 255},
	 {59,  51,  59,  255},   // 112
	 {79,  63,  79,  255},
	 {103, 71,  103, 255},
	 {123, 75,  123, 255},
	 {143, 75,  143, 255},
	 {167, 71,  167, 255},
	 {187, 67,  187, 255},
	 {211, 55,  211, 255},
	 {231, 43,  231, 255},
	 {255, 27,  255, 255},
	 {255, 59,  255, 255},
	 {255, 91,  255, 255},
	 {255, 119, 255, 255},
	 {255, 151, 255, 255},
	 {255, 183, 255, 255},
	 {255, 215, 255, 255},
	 {59,  51,  59,  255},   // 128
	 {71,  59,  71,  255},
	 {83,  71,  83,  255},
	 {95,  83,  95,  255},
	 {111, 95,  111, 255},
	 {123, 103, 123, 255},
	 {135, 115, 135, 255},
	 {147, 127, 147, 255},
	 {163, 139, 163, 255},
	 {175, 151, 175, 255},
	 {187, 159, 187, 255},
	 {203, 171, 203, 255},
	 {215, 183, 215, 255},
	 {227, 191, 227, 255},
	 {239, 203, 239, 255},
	 {255, 215, 255, 255},
	 {55,  27,  27,  255},   // 144
	 {71,  35,  35,  255},
	 {91,  43,  43,  255},
	 {107, 55,  55,  255},
	 {127, 67,  67,  255},
	 {143, 75,  75,  255},
	 {163, 87,  87,  255},
	 {179, 99,  99,  255},
	 {199, 111, 111, 255},
	 {203, 127, 127, 255},
	 {211, 139, 139, 255},
	 {219, 159, 159, 255},
	 {223, 175, 175, 255},
	 {231, 191, 191, 255},
	 {239, 211, 211, 255},
	 {247, 231, 231, 255},
	 {91,  63,  27,  255},   // 160
	 {111, 75,  31,  255},
	 {127, 87,  39,  255},
	 {147, 103, 43,  255},
	 {167, 115, 51,  255},
	 {187, 127, 55,  255},
	 {207, 139, 63,  255},
	 {227, 155, 67,  255},
	 {247, 167, 75,  255},
	 {247, 175, 95,  255},
	 {247, 183, 119, 255},
	 {247, 195, 139, 255},
	 {247, 203, 159, 255},
	 {247, 215, 183, 255},
	 {247, 227, 203, 255},
	 {251, 239, 227, 255},
	 {63,  63,  31,  255},   // 176
	 {75,  75,  35,  255},
	 {87,  87,  43,  255},
	 {99,  99,  51,  255},
	 {115, 115, 55,  255},
	 {127, 127, 63,  255},
	 {139, 139, 67,  255},
	 {151, 151, 75,  255},
	 {167, 167, 83,  255},
	 {175, 175, 95,  255},
	 {183, 183, 107, 255},
	 {191, 191, 123, 255},
	 {203, 203, 139, 255},
	 {211, 211, 159, 255},
	 {219, 219, 175, 255},
	 {231, 231, 195, 255},
	 {27,  59,  47,  255},   // 192
	 {31,  75,  59,  255},
	 {39,  87,  67,  255},
	 {47,  103, 79,  255},
	 {55,  119, 91,  255},
	 {59,  135, 99,  255},
	 {67,  151, 111, 255},
	 {71,  167, 119, 255},
	 {79,  183, 127, 255},
	 {87,  199, 139, 255},
	 {91,  215, 147, 255},
	 {99,  231, 155, 255},
	 {127, 235, 183, 255},
	 {163, 239, 211, 255},
	 {195, 243, 231, 255},
	 {231, 251, 247, 255},
	 {23,  55,  55,  255},   // 208
	 {31,  71,  71,  255},
	 {39,  87,  87,  255},
	 {47,  103, 103, 255},
	 {55,  119, 119, 255},
	 {67,  139, 139, 255},
	 {75,  155, 155, 255},
	 {87,  171, 171, 255},
	 {99,  187, 187, 255},
	 {111, 203, 203, 255},
	 {123, 223, 223, 255},
	 {143, 227, 227, 255},
	 {163, 231, 231, 255},
	 {183, 235, 235, 255},
	 {203, 239, 239, 255},
	 {227, 247, 247, 255},
	 {39,  39,  79,  255},   // 224
	 {47,  47,  91,  255},
	 {55,  55,  107, 255},
	 {63,  63,  123, 255},
	 {71,  71,  139, 255},
	 {79,  79,  151, 255},
	 {87,  87,  167, 255},
	 {99,  99,  183, 255},
	 {107, 107, 199, 255},
	 {123, 123, 203, 255},
	 {139, 139, 211, 255},
	 {155, 155, 219, 255},
	 {171, 171, 223, 255},
	 {187, 187, 231, 255},
	 {207, 207, 239, 255},
	 {227, 227, 247, 255},
	 {63,  27,  63,  255},   // 240
	 {75,  31,  75,  255},
	 {91,  39,  91,  255},
	 {103, 47,  103, 255},
	 {119, 51,  119, 255},
	 {131, 59,  131, 255},
	 {147, 67,  147, 255},
	 {163, 75,  163, 255},
	 {175, 83,  175, 255},
	 {191, 91,  191, 255},
	 {199, 107, 199, 255},
	 {207, 127, 207, 255},
	 {215, 147, 215, 255},
	 {223, 171, 223, 255},
	 {231, 195, 231, 255},
	 {243, 219, 243, 255}};

const double ogSurface::INTENSITIES[32] = {
	1.0,             // 0
	0.984250984251,  // 1
	0.968245836552,  // 2
	0.951971638233,  // 3
	0.935414346693,  // 4
	0.938558653544,  // 5
	0.901387818866,  // 6
	0.883883476483,  // 7
	0.866025403784,  // 8
	0.847791247891,  // 9
	0.829156197589,  // 10
	0.810092587301,  // 11
	0.790569415042,  // 12
	0.770551750371,  // 13
	0.75,            // 14
	0.728868986856,  // 15
	0.707106781187,  // 16
	0.684653196881,  // 17
	0.661437827766,  // 18
	0.637377439199,  // 19
	0.612372435696,  // 20
	0.586301969978,  // 21
	0.559016994375,  // 22
	0.53033008589,   // 23
	0.5,             // 24
	0.467707173347,  // 25
	0.433012701892,  // 26
	0.395284707521,  // 27
	0.353553390593,  // 28
	0.306186217848,  // 29
	0.25,            // 30
	0.176776695297   // 31
}; // INTENSITIES[]

// ogSurface constructor
ogSurface::ogSurface(void) 
{
	version		= ogVERSION;

	dataState	= ogNone;
	buffer		= NULL;
	lineOfs		= NULL;
	pal			= NULL;
	attributes	= NULL;
	xRes		= 0;
	yRes		= 0;
	maxX		= 0;
	maxY		= 0;
	bSize		= 0;
	lSize		= 0;
	BPP			= 0;
	bytesPerPix	= 0;
	pixFmtID	= 0;
	redShifter	= 0;
	greenShifter= 0;
	blueShifter	= 0;
	alphaShifter= 0;
	redFieldPosition   = 0;
	greenFieldPosition = 0;
	blueFieldPosition  = 0;
	alphaFieldPosition = 0;
	alphaMasker = 0;
	lastError	= ogOK;

	// Set these function pointers to do nothing (but not crash)
	// in case somebody actually calls them.
	setPixel = ([] (void * ptr, uint32_t p) -> void { });
	getPixel = ([] (void * ptr) -> uint32_t { return 0; });
} // ogSurface::ogSurface()


void ogSurface::AARawLine(uint32_t x1, uInt32 y1, uInt32 x2, uInt32 y2, uInt32 colour) 
{
	/*
	* aaRawLine
	*
	* private method
	*
	* draws an unclipped anti-aliased line from (x1,y1) to (x2,y2) using colour
	*
	*/
	uint32_t erradj, erracc;
	uint32_t erracctmp, intshift, wgt, wgtCompMask;
	int32  dx, dy, tmp, xDir, i;
	uInt8 r, g, b, a;
	uint32_t alphas[32];
	bool oldBlending;

	if (y1 > y2) 
	{
		tmp= y1;
		y1 = y2;
		y2 = tmp;

		tmp= x1;
		x1 = x2;
		x2 = tmp;
	} // if

	dx = (x2-x1);
	if (dx >= 0) xDir=1; else { dx = -dx; xDir=-1; }
	//  dx = abs(dx);
	dy = (y2 - y1);

	if (dy == 0) 
	{
		ogHLine(x1, x2, y1, colour);
		return;
	}

	if (dx == 0) 
	{
		ogVLine(x1, y1, y2, colour);
		return;
	}

	ogUnpack(colour, r, g, b, a);
	if (!ogIsBlending()) a = 255;

	for (i = 0; i < 32; i++) 
	{
		alphas[i] = static_cast<uint32_t>(INTENSITIES[i]*a + 0.5f);
	} // for

	oldBlending = ogSetBlending(true);

	RawSetPixel(x1, y1, r, g, b, a);

	// this is incomplete.. diagonal lines don't travel through the 
	// center of pixels exactly

	do {

		if (dx == dy) 
		{
			for (; dy != 0; dy--) 
			{
				x1 += xDir;
				++y1;
				RawSetPixel(x1, y1, r, g, b, a);
			} // for
			break;  // pop out to the bottom and restore the old blending state
		} // if dx==dy

		erracc = 0;
		intshift = 32-5;
		wgt = 12;
		wgtCompMask = 31;

		if (dy > dx) {
			/* y-major.  Calculate 32-bit fixed point fractional part of a pixel that
			* X advances every time Y advances 1 pixel, truncating the result so that
			* we won't overrun the endpoint along the X axis 
			*/
#ifndef __UBIXOS__
			erradj = ((uInt64) dx << 32) / (uInt64)dy;
#else
			asm volatile (  // fixed
				" xor %%eax, %%eax        \n"
				" div %%ecx              \n"
				: "=a" (erradj)
				: "d" (dx), "c" (dy)
				);
#endif

			while (--dy) {
				erracctmp = erracc;
				erracc += erradj;
				if (erracc <= erracctmp) x1 += xDir;
				y1++;     // y-major so always advance Y
				/* the nbits most significant bits of erracc give us the intensity
				*  weighting for this pixel, and the complement of the weighting for
				*  the paired pixel. 
				*/
				wgt = erracc >> intshift;
				ogSetPixel(x1, y1, r, g, b, alphas[wgt]);
				ogSetPixel(x1+xDir, y1, r, g, b, alphas[wgt ^ wgtCompMask]);
			} // while

		} else {

			/* x-major line.  Calculate 32-bit fixed-point fractional part of a pixel
			* that Y advances each time X advances 1 pixel, truncating the result so
			* that we won't overrun the endpoint along the X axis. 
			*/
#ifndef __UBIXOS__
			erradj = ((uInt64)dy << 32) / (uInt64)dx;
#else
			asm volatile(  // fixed
				" xor %%eax, %%eax        \n"
				" div %%ecx              \n"
				: "=a" (erradj)
				: "d" (dy), "c" (dx)
				);
#endif
			// draw all pixels other than the first and last 
			while (--dx) {
				erracctmp = erracc;
				erracc += erradj;
				if (erracc <= erracctmp) y1++; 
				x1 += xDir;                 // x-major so always advance X
				/* the nbits most significant bits of erracc give us the intensity
				* weighting for this pixel, and the complement of the weighting for
				* the paired pixel. 
				*/
				wgt = erracc >> intshift;
				ogSetPixel(x1, y1, r, g, b, alphas[wgt]);
				ogSetPixel(x1, y1+1, r, g, b, alphas[wgt ^ wgtCompMask]);
			} // while
		} // else
		RawSetPixel(x2, y2, r, g, b, alphas[wgt]);

	} while (false);

	ogSetBlending(oldBlending);
} // void ogSurface::AARawLine()

bool ogSurface::ClipLine(int32& x1, int32& y1, int32& x2, int32& y2) 
{
	/*
	*  clipLine()
	*
	*  private method 
	*
	*  clips a line to (0,0),(maxX,maxY); returns true if
	*  the line segment is in bounds, false if none of the line segment is
	*  on the screen.  Uses HJI's line clipping algorithm.
	*/

	int32 tx1, ty1, tx2, ty2;
	int32 outCode;
	uint32_t andResult, orResult;
	andResult = 15;
	orResult = 0;
	outCode = 0;
	if (x1 < 0) outCode+=8;
	if (x1 > (int32)maxX) outCode+=4;
	if (y1 < 0) outCode+=2;
	if (y1 > (int32)maxY) outCode++;

	andResult &= outCode;
	orResult |= outCode;
	outCode = 0;

	if (x2 < 0) outCode+=8;
	if (x2 > (int32)maxX) outCode+=4;
	if (y2 < 0) outCode+=2;
	if (y2 > (int32)maxY) outCode++;

	andResult &= outCode;
	orResult |= outCode;

	if (andResult > 0) return false;
	if (orResult == 0) return true;

	// some clipping is required here.  

	tx1 = x1;
	ty1 = y1;
	tx2 = x2;
	ty2 = y2;

	if (x1 < 0) 
	{
		ty1 = (x2*y1-x1*y2) / (x2-x1);
		tx1 = 0;
	} // if
	else
	{
		if (x2 < 0) 
		{
			ty2 = (x2*y1-x1*y2) / (x2-x1);
			tx2 = 0;
		} // elseif
	}

	if (x1 > (int32)maxX) 
	{
		ty1 = (y1*(x2-maxX)+y2*(maxX-x1)) / (x2-x1);
		tx1 = maxX;
	} // if
	else
	{
		if (x2 > (int32)maxX) 
		{
			ty2 = (y1*(x2-maxX)+y2*(maxX-x1)) / (x2-x1);
			tx2 = maxX;
		} // elseif
	}

	if (((ty1 < 0) && (ty2 < 0)) || 
		((ty1>(int32)maxY) && (ty2>(int32)maxY))) return false;

	if (ty1 < 0) 
	{
		tx1 = (x1*y2-x2*y1) / (y2-y1);
		ty1 = 0;
	} // if
	else
	{
		if (ty2 < 0) 
		{
			tx2 = (x1*y2-x2*y1) / (y2-y1);
			ty2 = 0;
		} // elseif 
	}

	if (ty1 > (int32)maxY) 
	{
		tx1 = (x1*(y2-maxY)+x2*(maxY-y1)) / (y2-y1);
		ty1 = maxY;
	} // if
	else
	{
		if (ty2 > (int32)maxY) 
		{
			tx2 = (x1*(y2-maxY)+x2*(maxY-y1)) / (y2-y1);
			ty2 = maxY;
		} // elseif 
	}

	if (((uint32_t)tx1 > maxX) || ((uInt32)tx2 > maxX)) return false;

	x1 = tx1;
	y1 = ty1;
	x2 = tx2;
	y2 = ty2;

	return true;
} // bool ogSurface::ClipLine()

uint32_t ogSurface::RawGetPixel(uInt32 x, uInt32 y) 
{
	//uInt8* ptr = (uInt8*)buffer + lineOfs[y];	// Pointer to pixel at (0,y).
	void * ptr = reinterpret_cast<void *>(buffer + lineOfs[y] + x*bytesPerPix);
	return getPixel(ptr);
#if 0
	uint32_t result = 0;
	uint32_t * buf32;
	uInt8 * buf24;
	uInt16 * buf16;
	uInt8 * buf8;

	switch (bytesPerPix) {
	case 4:
		buf32 = reinterpret_cast<uint32_t *>(buffer + lineOfs[y] + x*4);
		result = *buf32;
		//uint32_t * 
		//	uint32_t * buf = static_cast<uInt32 *>(buffer + lineOfs[y])
		_asm {
			mov		eax, this						;
			mov     edi, x							;
			mov     ebx, y							;

			shl		edi, 2							; // adjust for pixel size 
			mov     esi, [eax + ogSurface::lineOfs] ;
			add     edi, [esi + ebx*4]				;
			add     edi, [eax + ogSurface::buffer]	;

			mov     eax, [edi]						;
			mov		result, eax						;
		};
		break;
	case 3:
		buf24 = (buffer + lineOfs[y] + x);
		result = *buf24;
		break;
	case 2:
		buf16 = reinterpret_cast<uInt16 *>(buffer + lineOfs[y] + x*2);
		result = *buf16;
		break;
	case 1:
		buf8 = (buffer + lineOfs[y] + x);
		result = *buf8;
		break;
	} // switch
	return result;
#endif
} // uint32_t ogSurface::RawGetPixel()

// wu's double step line algorithm blatently borrowed from:
// http://www.edepot.com/linewu.html

void ogSurface::RawLine(uint32_t x1, uInt32 y1, uInt32 x2, uInt32 y2, uInt32 colour) 
{
	int32 dy = y2 - y1;
	int32 dx = x2 - x1;
	int32 stepx, stepy;

	if (dy < 0) { dy = -dy;  stepy = -1; } else { stepy = 1; }
	if (dx < 0) { dx = -dx;  stepx = -1; } else { stepx = 1; }

	RawSetPixel(x1, y1, colour);
	RawSetPixel(x2, y2, colour);

	if (dx > dy) 
	{
		int32 length = (dx - 1) >> 2;
		int32 extras = (dx - 1) & 3;
		int32 incr2 = (dy << 2) - (dx << 1);

		if (incr2 < 0) 
		{
			int32 c = dy << 1;
			int32 incr1 = c << 1;
			int32 d =  incr1 - dx;

			for (int32 i = 0; i < length; i++) 
			{
				x1 += stepx;
				x2 -= stepx;

				if (d < 0) 
				{                                      // Pattern:
					RawSetPixel(x1, y1, colour);                    //
					RawSetPixel(x1 += stepx, y1, colour);           //  x o o
					RawSetPixel(x2, y2, colour);                    //
					RawSetPixel(x2 -= stepx, y2, colour);

					d += incr1;
				} 
				else 
				{

					if (d < c) 
					{                                     // Pattern:
						RawSetPixel(x1, y1, colour);                   //      o
						RawSetPixel(x1 += stepx, y1 += stepy, colour); //  x o
						RawSetPixel(x2, y2, colour);                   //
						RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
					} 
					else 
					{
						RawSetPixel(x1, y1 += stepy, colour);             // Pattern:
						RawSetPixel(x1 += stepx, y1, colour);             //    o o 
						RawSetPixel(x2, y2 -= stepy, colour);             //  x
						RawSetPixel(x2 -= stepx, y2, colour);             //
					} // else

					d += incr2;
				} // else
			} // for i

			if (extras > 0) 
			{

				if (d < 0) 
				{
					RawSetPixel(x1 += stepx, y1, colour);
					if (extras > 1) RawSetPixel(x1 += stepx, y1, colour);
					if (extras > 2) RawSetPixel(x2 -= stepx, y2, colour);
				} 
				else
					if (d < c) {
						RawSetPixel(x1 += stepx, y1, colour);
						if (extras > 1) RawSetPixel(x1 += stepx, y1 += stepy, colour);
						if (extras > 2) RawSetPixel(x2 -= stepx, y2, colour);
					} 
					else 
					{
						RawSetPixel(x1 += stepx, y1 += stepy, colour);
						if (extras > 1) RawSetPixel(x1 += stepx, y1, colour);
						if (extras > 2) RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
					}
			} // if extras > 0
		} 
		else 
		{
			int32 c = (dy - dx) << 1;
			int32 incr1 = c << 1;
			int32 d =  incr1 + dx;

			for (int32 i = 0; i < length; i++) 
			{
				x1 += stepx;
				x2 -= stepx;

				if (d > 0) 
				{
					RawSetPixel(x1, y1 += stepy, colour);                   // Pattern:
					RawSetPixel(x1 += stepx, y1 += stepy, colour);          //      o
					RawSetPixel(x2, y2 -= stepy, colour);                   //    o
					RawSetPixel(x2 -= stepx, y2 -= stepy, colour);	  //  x
					d += incr1;
				} 
				else 
				{
					if (d < c) 
					{
						RawSetPixel(x1, y1, colour);                          // Pattern:
						RawSetPixel(x1 += stepx, y1 += stepy, colour);        //      o
						RawSetPixel(x2, y2, colour);                          //  x o
						RawSetPixel(x2 -= stepx, y2 -= stepy, colour);        //
					} 
					else 
					{
						RawSetPixel(x1, y1 += stepy, colour);                 // Pattern:
						RawSetPixel(x1 += stepx, y1, colour);                 //    o o
						RawSetPixel(x2, y2 -= stepy, colour);                 //  x
						RawSetPixel(x2 -= stepx, y2, colour);                 //
					}

					d += incr2;
				} // else
			} // for i

			if (extras > 0) 
			{
				if (d > 0) 
				{
					RawSetPixel(x1 += stepx, y1 += stepy, colour);
					if (extras > 1) RawSetPixel(x1 += stepx, y1 += stepy, colour);
					if (extras > 2) RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
				} 
				else
					if (d < c) 
					{
						RawSetPixel(x1 += stepx, y1, colour);
						if (extras > 1) RawSetPixel(x1 += stepx, y1 += stepy, colour);
						if (extras > 2) RawSetPixel(x2 -= stepx, y2, colour);
					} 
					else 
					{

						RawSetPixel(x1 += stepx, y1 += stepy, colour);

						if (extras > 1) RawSetPixel(x1 += stepx, y1, colour);
						if (extras > 2) 
						{
							if (d > c)
								RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
							else
								RawSetPixel(x2 -= stepx, y2, colour);
						} // if extras > 2

					} // else
			} // if extras > 0
		} // else

	} 
	else 
	{

		int32 length = (dy - 1) >> 2;
		int32 extras = (dy - 1) & 3;
		int32 incr2 = (dx << 2) - (dy << 1);

		if (incr2 < 0) 
		{
			int32 c = dx << 1;
			int32 incr1 = c << 1;
			int32 d =  incr1 - dy;

			for (int32 i = 0; i < length; i++) 
			{
				y1 += stepy;
				y2 -= stepy;

				if (d < 0) {
					RawSetPixel(x1, y1, colour);
					RawSetPixel(x1, y1 += stepy, colour);
					RawSetPixel(x2, y2, colour);
					RawSetPixel(x2, y2 -= stepy, colour);

					d += incr1;
				} 
				else 
				{

					if (d < c) {
						RawSetPixel(x1, y1, colour);
						RawSetPixel(x1 += stepx, y1 += stepy, colour);
						RawSetPixel(x2, y2, colour);
						RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
					} else {
						RawSetPixel(x1 += stepx, y1, colour);
						RawSetPixel(x1, y1 += stepy, colour);
						RawSetPixel(x2 -= stepx, y2, colour);
						RawSetPixel(x2, y2 -= stepy, colour);
					} // else

					d += incr2;
				} // else
			} // for i

			if (extras > 0) 
			{
				if (d < 0) 
				{
					RawSetPixel(x1, y1 += stepy, colour);
					if (extras > 1) RawSetPixel(x1, y1 += stepy, colour);
					if (extras > 2) RawSetPixel(x2, y2 -= stepy, colour);
				} 
				else
					if (d < c) {
						RawSetPixel(x1, y1 += stepy, colour);
						if (extras > 1) RawSetPixel(x1 += stepx, y1 += stepy, colour);
						if (extras > 2) RawSetPixel(x2, y2 -= stepy, colour);
					} else {
						RawSetPixel(x1 += stepx, y1 += stepy, colour);
						if (extras > 1) RawSetPixel(x1, y1 += stepy, colour);
						if (extras > 2) RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
					} // else
			} // if extras > 0
		} 
		else 
		{
			int32 c = (dx - dy) << 1;
			int32 incr1 = c << 1;
			int32 d =  incr1 + dy;

			for (int32 i = 0; i < length; i++) 
			{
				y1 += stepy;
				y2 -= stepy;

				if (d > 0) 
				{
					RawSetPixel(x1 += stepx, y1, colour);
					RawSetPixel(x1 += stepx, y1 += stepy, colour);
					RawSetPixel(x2 -= stepx, y2, colour);
					RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
					d += incr1;
				} 
				else 
				{
					if (d < c) {
						RawSetPixel(x1, y1, colour);
						RawSetPixel(x1 += stepx, y1 += stepy, colour);
						RawSetPixel(x2, y2, colour);
						RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
					} else {
						RawSetPixel(x1 += stepx, y1, colour);
						RawSetPixel(x1, y1 += stepy, colour);
						RawSetPixel(x2 -= stepx, y2, colour);
						RawSetPixel(x2, y2 -= stepy, colour);
					} // else
					d += incr2;
				} // else
			} // for

			if (extras > 0) 
			{
				if (d > 0) 
				{
					RawSetPixel(x1 += stepx, y1 += stepy, colour);
					if (extras > 1) RawSetPixel(x1 += stepx, y1 += stepy, colour);
					if (extras > 2) RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
				} 
				else
					if (d < c) 
					{
						RawSetPixel(x1, y1 += stepy, colour);
						if (extras > 1) RawSetPixel(x1 += stepx, y1 += stepy, colour);
						if (extras > 2) RawSetPixel(x2, y2 -= stepy, colour);
					} 
					else 
					{
						RawSetPixel(x1 += stepx, y1 += stepy, colour);
						if (extras > 1) RawSetPixel(x1, y1 += stepy, colour);
						if (extras > 2) 
						{
							if (d > c)
								RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
							else
								RawSetPixel(x2, y2 -= stepy, colour);
						} // if extras > 2
					} // else
			} // if extras > 0
		} // else
	} // else
} // void ogSurface::RawLine()

void ogSurface::RawSetPixel(uint32_t x, uInt32 y, uInt32 colour) 
{
	if (ogIsBlending()) 
	{
		uInt8 sR, sG, sB, sA;
		uInt8 dR, dG, dB;
		ogUnpack(colour, sR, sG, sB, sA);
		if (sA == 0) return;
		if (sA != 255) 
		{
			uint32_t inverseA = 255 - sA;
			ogUnpack(RawGetPixel(x, y), dR, dG, dB);
			uint32_t newR = (dR * inverseA + sR * sA) >> 8;
			uint32_t newG = (dG * inverseA + sG * sA) >> 8;
			uint32_t newB = (dB * inverseA + sB * sA) >> 8;
			//mji for gtk      colour = ogPack(newR, newG, newB, inverseA);
			colour = ogPack(newR, newG, newB, 255);
		}
	} // if

	void * ptr = reinterpret_cast<void *>(buffer + lineOfs[y] + x*bytesPerPix);
	setPixel(ptr, colour);
#if 0
	//ptr = static_cast<uInt8*>(buffer) + lineOfs[y];

	switch (bytesPerPix) {
	case 4:
		_asm {
			mov     eax, this						;
			mov     edi, [eax + ogSurface::buffer]	;
			mov     esi, [eax + ogSurface::lineOfs]	;
			mov     ebx, [y]						;
			mov     ecx, [x]						;

			// Calculate offset, prepare the pixel to be drawn 
			mov     edx, [esi + ebx * 4]
			nop										;
			add     edi, edx						;
			nop										;

			shl		ecx, 2	  ; //  adjust for pixel size
			add     edi, ecx  ;

			// Draw the pixel 
			mov     eax, [colour]					;
			mov     [edi], eax						;
		}; // asm
		break;
	case 3:
		_asm {
			mov     eax, this						;
			mov     edi, [eax + ogSurface::buffer]	;
			mov     esi, [eax + ogSurface::lineOfs]	;
			mov     ebx, [y]						;
			mov     ecx, [x]						;

			// Calculate offset, prepare the pixel to be drawn 
			mov     edx, [esi + ebx * 4]			;
			lea     ecx, [ecx *2 + ecx]				;
			add     edi, edx						;
			add     edi, ecx						; //  adjust for pixel size 

			// Draw the pixel 
			mov     ax, word ptr [colour]			;
			mov     bl, byte ptr [colour+2]			;
			mov     [edi], ax						;
			mov     [edi+2], bl						;
		}; //
		break;
	case 2:
		_asm {
			mov     eax, this						;
			mov     edi, [eax + ogSurface::buffer]	;
			mov     esi, [eax + ogSurface::lineOfs]	;
			mov     ebx, [y]						;
			mov     ecx, [x]						;

			// Calculate offset, prepare the pixel to be drawn 
			mov     edx, [esi + ebx * 4]			;
			add     edi, edx						;
			add     ecx, ecx						; // adjust for pixel size 
			mov     ax, word ptr [colour]			;
			add     edi, ecx						;

			// Draw the pixel 
			mov     [edi], ax						;
		}; // asm
	case 1:
		_asm {
			mov     eax, this						;
			mov     edi, [eax + ogSurface::buffer]	;
			mov     esi, [eax + ogSurface::lineOfs]	;
			mov     ebx, [y]						;
			mov     ecx, [x]						;

			// Calculate offset, prepare the pixel to be drawn 
			mov     edx, [esi + ebx * 4]			;
			nop										;
			add     edi, edx						;
			nop										;
			mov     al, byte ptr [colour]			;
			add     edi, ecx						;

			// Draw the pixel 
			mov     [edi], al						;
		};
		break;
	} // switch
#endif
} // void ogSurface::RawSetPixel()

void ogSurface::RawSetPixel(uint32_t x, uInt32 y, uInt8 r, uInt8 g, uInt8 b, uInt8 a) 
{
	uint32_t newR, newG, newB, inverseA;
	uInt8 dR, dG, dB;
	uint32_t colour;

	do {
		if (ogIsBlending()) {
			if (a == 0) return;
			if (a == 255) {
				colour = ogPack(r, g, b, a);
				break;
			} // if a == 255

			inverseA = 255 - a;
			ogUnpack(RawGetPixel(x, y), dR, dG, dB);
			newR = (dR * inverseA + r * a) >> 8;
			newG = (dG * inverseA + g * a) >> 8;
			newB = (dB * inverseA + b * a) >> 8;
			//mji for gtk      colour = ogPack(newR, newG, newB, inverseA);
			colour = ogPack(newR, newG, newB, 255);
		} else colour = ogPack(r, g, b, a);
	} while (false);

	void * ptr = reinterpret_cast<void *>(buffer + lineOfs[y] + x*bytesPerPix);
	setPixel(ptr, colour);

} // void ogSurface::RawSetPixel()


bool ogSurface::ogAlias(ogSurface& src, uint32_t x1, uInt32 y1, uInt32 x2, uInt32 y2) 
{
	uint32_t tmp;

	if (dataState == ogOwner) 
	{ 
		ogSetLastError(ogAlreadyOwner);
		return false;
	} // if

	if (x2 < x1) 
	{
		tmp= x2;
		x2 = x1;
		x1 = tmp;
	} // if

	if (y2 < y1) 
	{
		tmp= y2;
		y2 = y1;
		y1 = tmp;
	} // if 

	maxX = (x2-x1);
	maxY = (y2-y1);

	dataState = ogAliasing;

	bSize = 0;
	lSize = 0;

	owner = &src;
	buffer =(reinterpret_cast<uInt8*>(src.buffer)+x1*(src.bytesPerPix));
	lineOfs=&src.lineOfs[y1];
	attributes = src.attributes;

	pal  = src.pal;
	xRes = src.xRes;
	yRes = src.yRes;
	BPP  = src.BPP;
	bytesPerPix = src.bytesPerPix;
	pixFmtID = src.pixFmtID;

	// For 8bpp modes the next part doesn't matter
	redFieldPosition   = src.redFieldPosition;
	greenFieldPosition = src.greenFieldPosition;
	blueFieldPosition  = src.blueFieldPosition;
	alphaFieldPosition = src.alphaFieldPosition;
	// The next part is only used by 15/16bpp
	redShifter   = src.redShifter;
	greenShifter = src.greenShifter;
	blueShifter  = src.blueShifter;
	alphaShifter = src.alphaShifter;

	alphaMasker = src.alphaMasker;

	// Use the current pixel functions
	setPixel = src.setPixel;
	getPixel = src.getPixel;

	return true;
} // bool ogSurface::ogAlias()

void ogSurface::ogArc(int32 xCenter, int32 yCenter, uint32_t radius, 
					  uint32_t sAngle, uInt32 eAngle, uInt32 colour) 
{
	int32 p;
	uint32_t x, y, tmp;
	double alpha;

	if (radius == 0) 
	{
		ogSetPixel(xCenter, yCenter, colour);
		return;
	} // if

	sAngle %= 361;
	eAngle %= 361;

	if (sAngle > eAngle) {
		tmp = sAngle;
		sAngle = eAngle;
		eAngle = tmp;
	} // if

	x = 0;
	y = radius;
	p = 3-2*radius;

	while (x <= y) 
	{
		alpha = (180.0/3.14159265358979)*atan((double)x/(double)y);

		if ((alpha >= sAngle) && (alpha <= eAngle))
			ogSetPixel(xCenter-x, yCenter-y, colour);

		if ((90-alpha >= sAngle) && (90-alpha <= eAngle))
			ogSetPixel(xCenter-y, yCenter-x, colour);

		if ((90+alpha >= sAngle) && (90+alpha <= eAngle))
			ogSetPixel(xCenter-y, yCenter+x, colour);

		if ((180-alpha >= sAngle) && (180-alpha <= eAngle))
			ogSetPixel(xCenter-x, yCenter+y, colour);

		if ((180+alpha >= sAngle) && (180+alpha <= eAngle))
			ogSetPixel(xCenter+x, yCenter+y, colour);

		if ((270-alpha >= sAngle) && (270-alpha <= eAngle))
			ogSetPixel(xCenter+y, yCenter+x, colour);

		if ((270+alpha >= sAngle) && (270+alpha <= eAngle))
			ogSetPixel(xCenter+y, yCenter-x, colour);

		if ((360-alpha >= sAngle) && (360-alpha <= eAngle))
			ogSetPixel(xCenter+x, yCenter-y, colour);

		if (p < 0)
			p += 4*x+6;
		else {
			p += 4*(x-y)+10;
			--y;
		} // else
		++x;
	} // while
} // void ogSurface::ogArc()

bool ogSurface::ogAvail()
{
	return ((buffer != NULL) && (lineOfs != NULL));
} // bool ogSurface::ogAvail()

void ogSurface::ogBSpline(uint32_t numPoints, ogPoint2d* points, uInt32 segments, uInt32 colour) 
{
	if (points == NULL) return;

	auto calculate = ([] (double mu, int32 p0, int32 p1, int32 p2, int32 p3) 
	{
		double mu2, mu3;
		mu2 = mu*mu;
		mu3 = mu2*mu;
		return (int32)(0.5f+(1.0/6.0)*(mu3*(-p0+3.0*p1-3.0*p2+p3)+
			mu2*(3.0*p0-6.0*p1+3.0*p2)+
			mu*(-3.0*p0+3.0*p2)+(p0+4.0*p1+p2)));
	}); // calculate

	if ((numPoints < 4) || (numPoints > 255) || (segments == 0)) return;

	double mu, mudelta;
	int32 x1, y1, x2, y2;
	uint32_t n, h;
	mudelta = 1.0/segments;
	for (n=3; n<numPoints; n++) {
		mu = 0.0;
		x1=calculate(mu,points[n-3].x,points[n-2].x, points[n-1].x,points[n].x);
		y1=calculate(mu,points[n-3].y,points[n-2].y, points[n-1].y,points[n].y);
		mu += mudelta;

		for (h=0; h<segments; h++) {
			x2=calculate(mu,points[n-3].x,points[n-2].x, points[n-1].x,points[n].x);
			y2=calculate(mu,points[n-3].y,points[n-2].y, points[n-1].y,points[n].y);
			ogLine(x1, y1, x2, y2, colour);
			mu += mudelta;
			x1 = x2;
			y1 = y2;
		} // for h

	} // for n
} // void ogSurface::ogBSpline()


void ogSurface::ogCircle(int32 xCenter, int32 yCenter, uint32_t radius, uInt32 colour) 
{
	int32 x, y, d;

	x = 0;
	y = radius;
	d = 2*(1-radius);
	while (y >= 0) 
	{
		ogSetPixel(xCenter+x, yCenter+y, colour);
		ogSetPixel(xCenter+x, yCenter-y, colour);
		ogSetPixel(xCenter-x, yCenter+y, colour);
		ogSetPixel(xCenter-x, yCenter-y, colour);

		if (d + y > 0) {
			--y;
			d -= 2*y+1;
		} // if

		if (x > d) {
			++x;
			d += 2*x+1;
		} // if
	} // while
} // void ogSurface::ogCircle()

void ogSurface::ogClear(uint32_t colour) 
{
	if (!ogAvail()) return;

	if (ogIsBlending()) 
	{
		uInt8 r, g, b, a;
		ogUnpack(colour, r, g, b, a);
		if (a == 0) return;
		if (a != 255) 
		{
			for (uint32_t yy = 0; yy <= maxY; yy++) 
				for (uint32_t xx = 0; xx <= maxX; xx++) 
					RawSetPixel(xx, yy, r, g, b, a);
			return;
		} // if
	} // if blending

	for (uint32_t yy = 0; yy <= maxY; yy++) 
		for (uint32_t xx = 0; xx <= maxX; xx++) 
			RawSetPixel(xx, yy, colour);
} // void ogSurface::ogClear()

void ogSurface::ogClear()
{
	ogClear(ogGetTransparentColor());
} // void ogSurface::ogClear()

bool ogSurface::ogClone(ogSurface& src) 
{
	ogPixelFmt pixFmt;

	if (src.dataState == ogNone) 
	{
		ogSetLastError(ogNoSurface);
		return false;
	} // if

	src.ogGetPixFmt(pixFmt);

	if (!ogCreate(src.maxX+1, src.maxY+1, pixFmt)) return false;

	*attributes = *src.attributes;

	ogCopyPalette(src);
	ogCopy(src);
	return true;
} // bool ogSurface::ogClone()

void ogSurface::ogCopy(ogSurface& src) 
{
	uint32_t pixMap[256];
	uint32_t count, xCount, yCount;
	uint32_t xx, yy;
	uInt8  r, g, b, a;
	void * srcPtr;

	if (!ogAvail()) return;
	if (!src.ogAvail()) return;

	xCount = src.maxX+1;
	if (xCount > maxX+1) xCount = maxX+1;
	yCount = src.maxY+1;
	if (yCount > maxY+1) yCount = maxY+1;

	if (ogIsBlending()) 
	{

		for (yy = 0; yy < yCount; yy++) 
			for (xx = 0; xx < xCount; xx++) 
			{
				src.ogUnpack(src.RawGetPixel(xx, yy), r, g, b, a);
				RawSetPixel(xx, yy, r, g, b, a);
			} // for xx

			return;
	} // if blending

	if (pixFmtID != src.pixFmtID) 
	{
		if (src.bytesPerPix == 1) 
		{
			for (xx = 0; xx < 256; xx++)
				pixMap[xx] = ogPack(src.pal[xx].red,
				src.pal[xx].green,
				src.pal[xx].blue,
				src.pal[xx].alpha);

			for (yy = 0; yy < yCount; yy++) 
				for (xx = 0; xx < xCount; xx++) 
					RawSetPixel(xx, yy, pixMap[src.RawGetPixel(xx, yy)]);

		} 
		else 
		{  // if src.bytesPerPix == 1
			ogPixelFmt srcPixFmt, dstPixFmt;
			src.ogGetPixFmt(srcPixFmt);
			ogGetPixFmt(dstPixFmt);
			ogPixCon pc(srcPixFmt, dstPixFmt);

			for (yy = 0; yy < yCount; yy++) 
				for (xx = 0; xx < xCount; xx++) 
					RawSetPixel(xx, yy, pc.ConvPix(src.RawGetPixel(xx, yy)));

		} // else
	} 
	else 
	{

		xCount *= bytesPerPix;

		for (count = 0; count < yCount; count++)
			if ((srcPtr = src.ogGetPtr(0, count)) == NULL) 
			{
				/*
				* if we are here then we couldn't get a direct memory pointer
				* from the source object.  This means that it is not a normal
				* "memory" buffer and we have to use the implementation inspecific
				* interface.  We let the source buffer fill a "temporary" buffer
				* and then we copy it to where it needs to go.
				*/
#ifdef __UBIXOS_KERNEL__
				srcPtr = kmalloc(xCount);  // allocate space
#else
				srcPtr = malloc(xCount);  // allocate space
#endif
				if (srcPtr != NULL) {
					src.ogCopyLineFrom(0, count, srcPtr, xCount);
					ogCopyLineTo(0, count, srcPtr, xCount);
#ifdef __UBIXOS_KERNEL__
					kfree(srcPtr);
#else
					free(srcPtr);
#endif
				} // if srcPtr!=NULL
			} 
			else 
			{
				ogCopyLineTo(0, count, srcPtr, xCount);
			}
	} // else
} // void ogSurface::ogCopy()


void ogSurface::ogCopyBuf(int32 dX1, int32 dY1, 
						  ogSurface& src, 
						  int32 sX1, int32 sY1, int32 sX2, int32 sY2) 
{
	uint32_t pixMap[256];
	int32 xx, yy, count, xCount, yCount;
	uInt8 r, g, b, a;
	void *srcPtr;

	ogPixelFmt srcPixFmt, dstPixFmt;

	if (!ogAvail()) return;
	if (!src.ogAvail()) return;

	if ((dX1 > (int32)maxX) || (dY1 > (int32)maxY)) return;

	// if any of the source buffer is out of bounds then do nothing 
	if (( (uint32_t)sX1 > src.maxX) || ((uInt32)sX2 > src.maxX) ||
		( (uint32_t)sY1 > src.maxY) || ((uInt32)sY2 > src.maxY)) return;

	if (sX1 > sX2) 
	{
		int32 xSwap= sX1;
		sX1= sX2;
		sX2= xSwap;
	} // if

	if (sY1 > sY2) 
	{
		int32 ySwap = sY1;
		sY1= sY2;
		sY2= ySwap;
	} // if

	xCount = abs(sX2-sX1)+1;
	yCount = abs(sY2-sY1)+1;

	if (dX1+xCount > (int32)maxX+1) xCount = maxX-dX1+1;
	if (dY1+yCount > (int32)maxY+1) yCount = maxY-dY1+1;

	if (dX1 < 0) 
	{
		xCount += dX1;
		sX1 -= dX1;
		dX1 = 0;
	} // if

	if (dY1 < 0) 
	{
		yCount += dY1;
		sY1 -= dY1;
		dY1 = 0;
	} // if

	if ((dX1+xCount < 0) || (dY1+yCount < 0)) return;

	if (ogIsBlending()) 
	{
		for (yy = 0; yy < yCount; yy++) 
			for (xx = 0; xx < xCount; xx++) 
			{
				src.ogUnpack(src.RawGetPixel(sX1+xx, sY1+yy), r, g, b, a);
				RawSetPixel(dX1+xx, dY1+yy, r, g, b, a);
			} // for xx
			return;
	} // if IsBlending

	if (pixFmtID != src.pixFmtID) 
	{

		if (src.bytesPerPix == 1) 
		{
			for (xx = 0; xx < 256; xx++) 
				pixMap[xx] = ogPack(src.pal[xx].red,
				src.pal[xx].green,
				src.pal[xx].blue,
				src.pal[xx].alpha
				);

			for (yy = 0; yy < yCount; yy++) 
				for (xx = 0; xx < xCount; xx++)
					RawSetPixel(dX1+xx,dY1+yy,	pixMap[src.ogGetPixel(sX1+xx,sY1+yy)]); 
		} 
		else 
		{

			src.ogGetPixFmt(srcPixFmt);
			ogGetPixFmt(dstPixFmt);
			ogPixCon pc(srcPixFmt, dstPixFmt);  // allocate the pixel converter

			for (yy = 0; yy < yCount; yy++)
				for (xx = 0; xx < xCount; xx++) 
					RawSetPixel(dX1+xx, dY1+yy, pc.ConvPix(src.RawGetPixel(sX1+xx, sY1+yy)));

		} // else
	} 
	else 
	{
		xCount *= bytesPerPix;

		for (count = 0; count < yCount; count++)
			if ((srcPtr = src.ogGetPtr(sX1, sY1+count)) == NULL) 
			{
				// if we are here then we couldn't get a direct memory pointer
				// from the source object.  This means that it is not a normal
				// "memory" buffer and we have to use the implementation inspecific
				// interface.  We let the source buffer fill a "temporary" buffer
				// and then we copy it to where it needs to go.

#ifdef __UBIXOS_KERNEL__
				srcPtr = kmalloc(xCount);  // allocate space
#else
				srcPtr = malloc(xCount);  // allocate space
#endif
				if (srcPtr != NULL) {
					src.ogCopyLineFrom(sX1, sY1+count, srcPtr, xCount);
					ogCopyLineTo(dX1, dY1+count, srcPtr, xCount);
#ifdef __UBIXOS_KERNEL__
					kfree(srcPtr);
#else
					free(srcPtr);
#endif
				} // if srcPtr!=NULL
			} 
			else 
			{
				ogCopyLineTo(dX1,dY1+count,srcPtr,xCount);
			}
	} // else
} // void ogSurface::ogCopyBuf()

void ogSurface::ogCopyLineTo(uint32_t dx, uInt32 dy, const void * src, uInt32 size) 
{
	/*
	* CopyLineTo()
	*
	* Inputs:
	*
	* dx   - Destination X of the target buffer
	* dy   - Destination Y of the target buffer
	* src  - buffer to copy
	* size - size in bytes *NOT* pixels
	*
	* Copies a run of pixels (of the same format) to (x,y) of a buffer
	*
	* This method is required because of the different implementations of
	* copying a run of pixels to a buffer
	*
	* WARNING!!! This does *NO* error checking. It is assumed that you've
	* done all of that.  CopyLineTo and CopyLineFrom are the only
	* methods that don't check to make sure you're hosing things.  Don't
	* use this method unless YOU KNOW WHAT YOU'RE DOING!!!!!!!!!
	*/

	memcpy( buffer+lineOfs[dy]+dx*bytesPerPix,         // dest
		src,                                               // src
		size);                                             // size

} // ogSurface::ogCopyLineTo

void ogSurface::ogCopyLineFrom(uint32_t sx, uInt32 sy, void * dst, uInt32 size) 
{
	/*
	* CopyLineFrom()
	*
	* Inputs:
	*
	* sx   - Source X of the target buffer
	* sy   - Source Y of the target buffer
	* dest - where to put it
	* size - size in bytes *NOT* pixels
	*
	* Copies a run of pixels (of the same format) to (x,y) of a buffer
	*
	* This method is required because of the different implementations of
	* copying a run of pixels to a buffer
	*
	* WARNING!!! This does *NO* error checking. It is assumed that you've
	* done all of that.  CopyLineTo and CopyLineFrom are the only
	* methods that don't check to make sure you're hosing things.  Don't
	* use this method unless YOU KNOW WHAT YOU'RE DOING!!!!!!!!!
	*/

	memcpy( dst,                                               // dest
		(uInt8*)buffer+lineOfs[sy]+sx*bytesPerPix,         // src
		size);                                             // size

	return;
} // ogSurface::ogCopyLineFrom

void ogSurface::ogCopyPalette(ogSurface& src) 
{
	if (src.pal == NULL) return;
	if (pal == NULL) pal = new ogRGBA8[256];
	if (pal == NULL) return;
	src.ogGetPalette(pal);
	// memcpy(pal, src.pal, sizeof(ogRGBA8)*256);
	return;
} // void ogSurface::ogCopyPalette()


bool ogSurface::ogCreate(uint32_t _xRes, uInt32 _yRes, ogPixelFmt _pixFormat)
{
	/*
	*  ogSurface::ogCreate()
	*  Allocates memory for a buffer of size _xRes by _yRes with
	*  the pixel format defined in _pixformat.  Allocates memory
	*  for pal and lineOfs.
	*/
	uInt8		* newBuffer = NULL;
	ptrdiff_t	* newLineOfs = NULL;
	ogRGBA8     * newPal = NULL;
	ogAttribute * newAttributes = NULL;

	uint32_t        newBSize;
	uint32_t        newLSize;

	bool          status = false;

	switch (_pixFormat.BPP) {
	case 8:
		getPixel = ([] (void * ptr) mutable -> uint32_t {	return *(reinterpret_cast<uInt8*>(ptr)); });
		setPixel = ([] (void * ptr, uint32_t colour) -> void { *(reinterpret_cast<uInt8*>(ptr)) = colour; });
		break;
	case 15:
	case 16:
		getPixel = ([] (void * ptr) mutable -> uint32_t { return *(reinterpret_cast<uInt16*>(ptr)); });
		setPixel = ([] (void * ptr, uint32_t colour) -> void { *(reinterpret_cast<uInt16*>(ptr)) = colour; });
		break;
	case 24:
		getPixel = ([] (void * ptr) -> uint32_t { 
			uint32_t colour = 0;
			uInt8* src = reinterpret_cast<uInt8*>(ptr);
			uInt8* dest = reinterpret_cast<uInt8*>(&colour);
			// This may break depending on endian-ness. TODO: Requires testing.
			*dest++ = *src++;
			*dest++ = *src++;
			*dest++ = *src++;
			return colour;
		}); //  getPixel() 24bpp lambda
		setPixel = ([] (void * ptr, uint32_t colour) -> void { 
			uInt8* src = reinterpret_cast<uInt8*>(&colour);
			uInt8* dest = reinterpret_cast<uInt8*>(ptr);
			// This may break depending on endian-ness. TODO: Requires testing.
			*dest++ = *src++;
			*dest++ = *src++;
			*dest++ = *src++;
		}); // setPixel() 24bpp lambda
		break;
	case 32:
		getPixel = ([] (void * ptr) -> uint32_t { return *(reinterpret_cast<uInt32*>(ptr)); });
		setPixel = ([] (void * ptr, uint32_t colour)  -> void { *(reinterpret_cast<uInt32*>(ptr)) = colour; });
		break;
	default:
		ogSetLastError(ogBadBPP);
		return false;
	} // switch

	newBSize = _xRes * _yRes * ((_pixFormat.BPP + 7) >> 3);
	newLSize = _yRes * sizeof(uint32_t);  // number of scan lines * sizeof(uInt32)

#ifdef __UBIXOS_KERNEL__
	newBuffer = kmalloc(newBSize);
#else
	newBuffer = reinterpret_cast<uInt8 *>(malloc(newBSize));
#endif
	newLineOfs = new ptrdiff_t[_yRes];
	newPal = new ogRGBA8[256];
	newAttributes = new ogAttribute();

	do {

		if ((newBuffer == NULL) || 
			(newLineOfs == NULL) ||
			(newPal == NULL) || 
			(newAttributes == NULL))
		{
			ogSetLastError(ogMemAllocFail);
			break;                           // break out of do {...} while(false) 
		} // if 

		// check to see if we have already allocated memory .. if so, free it

		if (dataState == ogOwner) {
#ifdef __UBIXOS_KERNEL__
			kfree(buffer);
#else
			free((void *)buffer);
#endif
			delete [] lineOfs;
			delete [] pal;
			delete attributes;
		}  // if dataState

		buffer = newBuffer;
		lineOfs = newLineOfs;
		pal = newPal;
		attributes = newAttributes;
		bSize = newBSize;
		lSize = newLSize; 

		newBuffer     = NULL;
		newLineOfs    = NULL;
		newPal        = NULL;
		newAttributes = NULL;

		BPP = _pixFormat.BPP;
		bytesPerPix = (BPP + 7) >> 3;

		ogSetPalette(DEFAULT_PALETTE); 
		// memcpy(pal, DEFAULT_PALETTE, sizeof(ogRGBA8)*256);

		maxX = _xRes -1;
		xRes = _xRes * bytesPerPix;
		maxY = _yRes -1;
		yRes = _yRes;

		// in the pascal version we go from 1 to maxY .. here we use yy < yRes
		// (which is the same)

		lineOfs[0] = 0;
		for (size_t yy = 1; yy < yRes; yy++)
			lineOfs[yy] = lineOfs[yy-1]+xRes;

		dataState = ogOwner;

		// For 8bpp modes the next part doesn't matter 

		redFieldPosition   = _pixFormat.redFieldPosition;
		greenFieldPosition = _pixFormat.greenFieldPosition;
		blueFieldPosition  = _pixFormat.blueFieldPosition;
		alphaFieldPosition = _pixFormat.alphaFieldPosition;
		// The next part is only used by 15/16hpp 
		redShifter   = 8-_pixFormat.redMaskSize;
		greenShifter = 8-_pixFormat.greenMaskSize;
		blueShifter  = 8-_pixFormat.blueMaskSize;
		alphaShifter = 8-_pixFormat.alphaMaskSize;

		if (_pixFormat.alphaMaskSize != 0) 
			alphaMasker = ~(ogPixelFmt::OG_MASKS[_pixFormat.alphaMaskSize] << alphaFieldPosition);
		else
			alphaMasker = ~0;

		if (bytesPerPix == 1)
		{
			pixFmtID = 0x08080808;
			// turn anti aliasing off by default for 8bpp modes
			ogSetAntiAliasing(false);
		} 
		else 
		{
			pixFmtID = (redFieldPosition) | 
				(greenFieldPosition << 8) | 
				(blueFieldPosition << 16) |
				(alphaFieldPosition << 24);
			ogSetAntiAliasing(true);
		} // else

		ogClear(ogPack(0, 0, 0));

		owner = this;
		status = true;
	} while(false);

#ifdef __UBIXOS_KERNEL__
	if (newBuffer) kfree(newBuffer);
#else
	if (newBuffer) free(newBuffer);
#endif
	if (newLineOfs) delete [] newLineOfs;
	if (newPal) delete [] newPal;
	if (newAttributes) delete newAttributes;

	return status;
} // bool ogSurface::ogCreate()

void ogSurface::ogCubicBezierCurve(int32 x1, int32 y1, int32 x2, int32 y2,
								   int32 x3, int32 y3, int32 x4, int32 y4,
								   uint32_t segments, uInt32 colour) 
{
	double tX1, tY1, tX2, tY2, tX3, tY3, mu, mu2, mu3, mudelta;
	int32 xStart, yStart, xEnd, yEnd;
	uint32_t n;
	if (segments < 1) return;
	if (segments > 128) segments=128;

	mudelta = 1.0/segments;
	mu = mudelta;
	tX1 =-x1+3*x2-3*x3+x4;
	tY1 =-y1+3*y2-3*y3+y4;
	tX2 =3*x1-6*x2+3*x3;
	tY2 =3*y1-6*y2+3*y3;
	tX3 =-3*x1+3*x2;
	tY3 =-3*y1+3*y2;

	xStart = x1;
	yStart = y1;

	for (n = 1; n < segments; n++)
	{
		mu2 = mu*mu;
		mu3 = mu2*mu;
		xEnd = static_cast<int32>(mu3*tX1+mu2*tX2+mu*tX3+x1 +0.5f);
		yEnd = static_cast<int32>(mu3*tY1+mu2*tY2+mu*tY3+y1 +0.5f);
		ogLine(xStart, yStart, xEnd, yEnd, colour);
		mu += mudelta;
		xStart = xEnd;
		yStart = yEnd;
	} // for
} // void ogSurface::ogCubicBezierCurve()

void ogSurface::ogCurve(int32 x1, int32 y1, int32 x2, int32 y2, int32 x3, int32 y3,
						uint32_t segments, uInt32 colour) 
{
	// This is currently broken. 
	// ToDo: fix ogCurve

	int64 ex, ey, fx, fy;
	int64 t1, t2;

	if (segments<2) segments=2; else if (segments>128) segments=128;

	int64 s = segments;

	x2 = (x2*2)-((x1+x3)/2);
	y2 = (y2*2)-((y1+y3)/2);

	ex = (static_cast<int64>(x2-x1) << 17) / s;
	ey = (static_cast<int64>(y2-y1) << 17) / s;
	fx = (static_cast<int64>(x3-(2*x2)+x1) << 16) / (s*s);
	fy = (static_cast<int64>(y3-(2*y2)+y1) << 16) / (s*s);

	while (--s > 0) 
	{
		t1 = x3;
		t2 = y3;
		x3 = (static_cast<int64>((fx*segments+ex)*segments) / 65536L)+x1;
		y3 = (static_cast<int64>((fy*segments+ey)*segments) / 65536L)+y1;
		ogLine(static_cast<int32>(t1), static_cast<int32>(t2), x3, y3, colour);
	} // while

	ogLine(x3, y3, x1, y1, colour);

} // void ogSurface::ogCurve()


void ogSurface::ogFillCircle(int32 xCenter, int32 yCenter, uint32_t radius, uInt32 colour) 
{
	int32 x, y, d;
	x = 0;
	y = radius;
	d = 4*(1-radius);

	while (y >= 0) 
	{
		if (d + y > 0) 
		{
			ogHLine(xCenter-x, xCenter+x, yCenter-y, colour);
			if (y != 0) ogHLine(xCenter-x, xCenter+x, yCenter+y, colour);

			--y;
			d -= 4*y+1;
		} // if


		if (x > d) 
		{
			++x;
			d += 4*x+1;
		} // if
	} // while
} // void ogSurface::ogFillCircle()

void ogSurface::ogFillGouraudPolygon(uint32_t numPoints, ogPoint2d* polyPoints, ogRGBA8 * colours) 
{

	ogEdgeTable * edges;
	int32 currentY = ~0;

	if (numPoints < 3) return;

	edges = new ogEdgeTable();

	if (edges == NULL) return;  // sanity check

	edges->BuildGET_G(numPoints, polyPoints, colours);

	if (edges->globalEdges != NULL)
		currentY = edges->globalEdges->startY;

	while ((edges->globalEdges != NULL) || (edges->activeEdges != NULL)) 
	{
		edges->MoveXSortedToAET(currentY);
		edges->ScanOutAET_G(*this, currentY);
		edges->AdvanceAET();
		edges->XSortAET();
		++currentY;
		if (currentY > (int32)maxY) break; // if we've gone past the bottom, stop
	} // while

	delete edges;
} // void ogSurface::ogFillGouraudPolygon()

void ogSurface::ogFillPolygon(uint32_t numPoints, ogPoint2d* polyPoints, uInt32 colour) 
{
	ogEdgeTable * edges;
	int32 currentY = ~0;

	if (numPoints < 3) return;

	if (!ogIsBlending()) ogPolygon(numPoints, polyPoints, colour);

	edges = new ogEdgeTable();

	if (edges == NULL) return;  // sanity check

	edges->BuildGET(numPoints, polyPoints);

	if (edges->globalEdges != NULL)
		currentY = edges->globalEdges->startY;

	while ((edges->globalEdges != NULL) || (edges->activeEdges != NULL)) 
	{
		edges->MoveXSortedToAET(currentY);
		edges->ScanOutAET(*this, currentY, colour);
		edges->AdvanceAET();
		edges->XSortAET();
		++currentY;
		if (currentY > (int32)maxY) break; // if we've gone past the bottom, stop
	} // while
	delete edges;
} // void ogSurface::ogFillPolygon()


void ogSurface::ogFillRect(int32 x1, int32 y1, int32 x2, int32 y2, uint32_t colour) 
{
	int32 yy, tmp;

	if (x2 < x1) 
	{
		tmp= x2;
		x2 = x1;
		x1 = tmp;
	} // if

	if (y2 < y1) 
	{
		tmp= y2;
		y2 = y1;
		y1 = tmp;
	} // if 

	if ((y2 < 0) || (y1 > (int32)maxY)) return;
	if (y1 < 0) y1 = 0;
	if (y2 > (int32)maxY) y2 = maxY;
	for (yy = y1; yy <= y2; yy++)
		ogHLine(x1, x2, yy, colour);
} // ogSurface::ogFillRect

void ogSurface::ogFillTriangle(int32 x1, int32 y1, int32 x2, int32 y2,
							   int32 x3, int32 y3, uint32_t colour) 
{
	ogPoint2d points[3];
	points[0].x = x1;
	points[0].y = y1;
	points[1].x = x2;
	points[1].y = y2;
	points[2].x = x3;
	points[2].y = y3;

	ogFillPolygon(3, points, colour);

} // void ogSurface::ogFillTriangle()
uint32_t ogSurface::ogGetAlpha(void) 
{
	return (attributes != NULL ? attributes->defaultAlpha : 255L);
} // uint32_t ogSurface::ogGetAlpha()

uint32_t ogSurface::ogGetColorCount()
{
	if (!ogAvail() || ogGetBytesPerPix() != 1) return 0;

	uint32_t colourCount = 0;
	uint32_t colourCounter[256] = {};
	for (uint32_t y = 0; y <= ogGetMaxY(); y++)
	{
		for (uint32_t x = 0; x <= ogGetMaxX(); x++)
		{
			colourCounter[ogGetPixel(x, y)]++;
		} // for x
	} // for y

	for (size_t index = 0; index < std::extent<decltype(colourCounter)>::value; index++)
	{
		if (colourCounter[index] != 0) colourCount++;
	} // for index

	return colourCount;
} // void ogSurface::ogCountColors()

uint32_t ogSurface::ogGetPixel(int32 x, int32 y) 
{
	if (!ogAvail()) return ogGetTransparentColor();

	if (((uint32_t)x > maxX) || ((uInt32)y > maxY)) return ogGetTransparentColor();

	return RawGetPixel(x, y);
} // uint32_t ogSurface::ogGetPixel()

void ogSurface::ogGetPalette(ogRGBA8 _pal[256])
{
	if (pal == NULL) return;

	for (size_t index = 0; index <256; index++)
	{
		_pal[index].red = pal[index].red;
		_pal[index].green = pal[index].green;
		_pal[index].blue = pal[index].blue;
		_pal[index].alpha = pal[index].alpha;
	} // for index
} // void ogSurface::ogGetPalette()

void ogSurface::ogGetPixFmt(ogPixelFmt& pixfmt) 
{
	pixfmt.BPP = BPP;
	pixfmt.redFieldPosition   = redFieldPosition;
	pixfmt.greenFieldPosition = greenFieldPosition;
	pixfmt.blueFieldPosition  = blueFieldPosition;
	pixfmt.alphaFieldPosition = alphaFieldPosition;
	pixfmt.redMaskSize   = 8-redShifter;
	pixfmt.greenMaskSize = 8-greenShifter;
	pixfmt.blueMaskSize  = 8-blueShifter;
	pixfmt.alphaMaskSize = 8-alphaShifter;
} // void ogSurface::ogGetPixFmt()

void* ogSurface::ogGetPtr(uint32_t x, uInt32 y) 
{
	//  return (Avail() ? ( (uInt8*)buffer+(lineOfs[y]+x*((BPP+7) >> 3)) ) : NULL );
	return reinterpret_cast<uInt8*>(buffer+(lineOfs[y]+x*bytesPerPix));
} // void* ogSurface::ogGetPtr

uint32_t ogSurface::ogGetTransparentColor(void)
{
	return (attributes != NULL ? attributes->transparentColor : 0);
} // ogSurface::ogGetTransparentColor

void ogSurface::ogHFlip(void) 
{
	void * tmpBuf1;
	void * tmpBuf2;
	uint32_t xWidth, count;

	if (!ogAvail()) return;

	xWidth = (maxX+1)*bytesPerPix;

#ifdef __UBIXOS_KERNEL__
	tmpBuf1 = kmalloc(xWidth);
	tmpBuf2 = kmalloc(xWidth);
#else
	tmpBuf1 = malloc(xWidth);
	tmpBuf2 = malloc(xWidth);
#endif

	if ((tmpBuf1 != NULL) && (tmpBuf2 != NULL))
	{
		for (count = 0; count <= (maxY/2); count++) 
		{
			ogCopyLineFrom(0, count, tmpBuf1, xWidth);
			ogCopyLineFrom(0, maxY-count,tmpBuf2, xWidth);
			ogCopyLineTo(0, maxY-count,tmpBuf1, xWidth);
			ogCopyLineTo(0, count, tmpBuf2, xWidth);
		} // for count
	}
#ifdef __UBIXOS_KERNEL__
	kfree(tmpBuf2);
	kfree(tmpBuf1);
#else
	free(tmpBuf2);
	free(tmpBuf1);
#endif

} // void ogSurface::ogHFlip()

void ogSurface::ogHLine(int32 x1, int32 x2, int32 y, uint32_t colour) 
{
	int32 tmp;
	uInt8 r, g, b, a;

	if (!ogAvail()) return;
	if ((uint32_t)y > maxY) return;

	if (x1 > x2) 
	{
		tmp= x1;
		x1 = x2;
		x2 = tmp;
	} // if

	if (x1 < 0) x1 = 0;
	if (x2 > (int32)maxX) x2 = maxX;
	if (x2 < x1) return;

	if (ogIsBlending()) {
		ogUnpack(colour, r, g, b, a);
		if (a == 0) return;
		if (a == 255) {
			for (tmp = x1; tmp <= x2; tmp++) 
				RawSetPixel(tmp, y, r, g, b, a);
			return;
		} // if a == 255
	} // if blending
	for (int32 xx = x1; xx <= x2; xx++)
		RawSetPixel(xx, y, colour);

} // void ogSurface::ogHLine()

bool ogSurface::ogIsAntiAliasing(void) 
{
	return (attributes != NULL ? attributes->antiAlias : false);
} // bool ogSurface::ogIsAntiAliasing()

bool ogSurface::ogIsBlending(void)
{
	return (attributes != NULL ? attributes->blending : false);
} // bool ogSurface::ogIsBlending()

void ogSurface::ogLine(int32 x1, int32 y1, int32 x2, int32 y2, uint32_t colour) 
{
	if (ClipLine(x1,y1,x2,y2)) 
	{
		if (ogIsAntiAliasing()) 
			AARawLine(x1, y1, x2, y2, colour); 
		else  
			RawLine(x1, y1, x2, y2, colour);
	} // if clipLine
	return;
} // void ogSurface::ogLine()

bool ogSurface::ogLoadPalette(const char *palfile) 
{
	ogRGBA8 oldPalette[256];
#ifdef __UBIXOS_KERNEL__
	fileDescriptor *f;
	This is possibly incompatible with the kernel. Will need a rewrite.
#endif
	bool result;

	if (pal == NULL) 
	{
		pal = new ogRGBA8[256];
		if (pal == NULL) 
		{
			ogSetLastError(ogMemAllocFail);
			return false;
		} // if 
		ogSetPalette(DEFAULT_PALETTE);
		//   memcpy(pal, DEFAULT_PALETTE, sizeof(ogRGBA8)*256);
	} // if

	ogGetPalette(oldPalette);
	// memcpy(&oldPalette, pal, sizeof(ogRGBA8)*256);

	std::ifstream file;
	file.open(palfile, std::ios::in | std::ios::binary);

	if (!file) 
	{
		ogSetLastError(ogFileNotFound);
		return false;
	} // if !file

	size_t index = 0;
	while (index < sizeof(pal) / sizeof(pal[0])) 
	{
		if (!(file >> pal[index].red)) break;
		if (!(file >> pal[index].green)) break;
		if (!(file >> pal[index].blue)) break;
		if (!(file >> pal[index].alpha)) break;
		index++;
	}
	//lresult = fread(pal, sizeof(ogRGBA8), 256, f);
	result = (index == 256);

	if (!result) 
	{
		ogSetLastError(ogFileReadError);
		ogSetPalette(oldPalette);
		// memcpy(pal, &oldPalette, sizeof(ogRGBA8)*256);
	} // if

	file.close();
	return result;
} // bool ogSurface::ogLoadPalette()

void ogSurface::ogOptimize()
{
	if (!ogAvail() || ogGetBytesPerPix() != 1) return;

	int colourCounter[256] = {};	// count of how much that colour is used
	size_t swapIndices[256];		// swap indices
	size_t reverseIndices[256];		// reverse indices
	ogRGBA8 pal[256];				// We also swap the palette entries

	// First acquire a count of all the colours used
	for (uint32_t y = 0; y <= ogGetMaxY(); y++)
	{
		for (uint32_t x = 0; x <= ogGetMaxX(); x++)
		{
			colourCounter[ogGetPixel(x, y)]++;
		} // for x
	} // for y

	colourCounter[ogGetTransparentColor()] = INT_MAX;	// set 0 so high it's always first
	//SortDescending(colourCounter, swapIndices, 256);

	// Sort them in descending order
	for (size_t i = 0; i < 256; i++)
		swapIndices[i] = i;

	for (size_t k = 1; k < 256; k++)
	{
		for (size_t i = 0; i <256 - k; i++)
		{
			if (colourCounter[swapIndices[i]] < colourCounter[swapIndices[i+1]])
			{
				size_t temp = swapIndices[i];
				swapIndices[i] = swapIndices[i+1];
				swapIndices[i+1] = temp;
			}  // if
		} // for i
	} // for k

	ogGetPalette(pal);
	for (size_t i = 0; i < 256; i++)
	{
		reverseIndices[swapIndices[i]] = i;
		//cout << "colourCounter[" << i << "] = " << swapIndices[i] << endl;
		// Also swap the palette entries
		ogSetPalette(i, pal[swapIndices[i]].red, pal[swapIndices[i]].green, pal[swapIndices[i]].blue);
	} // for i

	for (uint32_t y = 0; y <= ogGetMaxY(); y++)
	{
		for (uint32_t x = 0; x <= ogGetMaxX(); x++)
		{
			ogSetPixel(x, y, reverseIndices[ogGetPixel(x, y)]);
		} // for x
	} // for y
} // void ogSurface::ogOptimize()

uint32_t ogSurface::ogPack(uInt8 red, uInt8 green, uInt8 blue) 
{
	uint32_t colour = 0;

	switch (bytesPerPix) {
	case 4:
		colour = ( (red << redFieldPosition) |
			(green << greenFieldPosition) |
			(blue << blueFieldPosition) |
			(ogGetAlpha() << alphaFieldPosition) );
		break;
	case 3:
		colour = ( (red << redFieldPosition) |
			(green << greenFieldPosition) |
			(blue << blueFieldPosition) );
		break;
	case 2:
		colour = ((red >> redShifter) << redFieldPosition) |
			((green >> greenShifter) << greenFieldPosition) |
			((blue >> blueShifter) << blueFieldPosition) |
			((ogGetAlpha() >> alphaShifter) << alphaFieldPosition);
		break;
	case 1:
		uint32_t dist = 255+255+255;
		for (uint32_t idx = 0; idx <= 255; idx++) 
		{
			uint32_t rd = abs(red-pal[idx].red);
			uint32_t gd = abs(green-pal[idx].green);
			uint32_t bd = abs(blue-pal[idx].blue);
			uint32_t newdist = rd + gd + bd;

			if (newdist < dist) 
			{
				dist = newdist;
				colour = idx;
			} // if
		} // for
		break;
	} // switch

	return colour;

} // uint32_t ogSurface::ogPack()

uint32_t ogSurface::ogPack(uInt8 red, uInt8 green, uInt8 blue, uInt8 alpha) 
{
	uint32_t colour = 0;

	switch (bytesPerPix) {
	case 4:
		colour = ( (red << redFieldPosition) |
			(green << greenFieldPosition) |
			(blue << blueFieldPosition) |
			(alpha << alphaFieldPosition) );
		break;
	case 3:
		colour = ( (red << redFieldPosition) |
			(green << greenFieldPosition) |
			(blue << blueFieldPosition) );
		break;
	case 2:
		colour = ((red >> redShifter) << redFieldPosition) |
			((green >> greenShifter) << greenFieldPosition) |
			((blue >> blueShifter) << blueFieldPosition) |
			((alpha >> alphaShifter) << alphaFieldPosition);
		break;
	case 1:
		uint32_t dist = 255+255+255;
		for (uint32_t idx = 0; idx <= 255; idx++)
		{
			uint32_t rd = abs(red-pal[idx].red);
			uint32_t gd = abs(green-pal[idx].green);
			uint32_t bd = abs(blue-pal[idx].blue);
			uint32_t newdist = rd + gd + bd;

			if (newdist < dist) 
			{
				dist = newdist;
				colour = idx;
			} // if
		} // for
		break;
	} // switch

	return colour;
} // uint32_t ogSurface::ogPack()

void ogSurface::ogPolygon(uint32_t numPoints, ogPoint2d* polyPoints, uInt32 colour) 
{
	if (polyPoints == NULL) return;

	if (numPoints == 1) 
	{
		ogSetPixel(polyPoints[0].x, polyPoints[0].y, colour);
	}
	else
	{
		for (size_t count = 0; count < numPoints; count++)
			ogLine(polyPoints[count].x, polyPoints[count].y,
			polyPoints[(count+1) % numPoints].x,
			polyPoints[(count+1) % numPoints].y,
			colour);
	}
} // void ogSurface::ogPolygon()

void ogSurface::ogRect(int32 x1, int32 y1, int32 x2, int32 y2, uint32_t colour) 
{
	if ((x1 == x2) || (y1 == y2)) 
	{

		if ((x1 == x2) && (y1 == y2))
			ogSetPixel(x1, y1, colour);
		else
			ogLine(x1, y1, x2, y2, colour);

	} 
	else 
	{

		if (y1 > y2) 
		{
			int32 tmp= y1;
			y1 = y2;
			y2 = tmp;
		} // if

		ogHLine(x1, x2, y1, colour);  // Horizline has built in clipping
		ogVLine(x1, y1+1, y2-1, colour);  // vertline has built in clipping too
		ogVLine(x2, y1+1, y2-1, colour);
		ogHLine(x1, x2, y2, colour);

	} // else

} // void ogSurface::ogRect()

void ogSurface::ogScale(ogSurface& src) {
		ogScaleBuf(0, 0, maxX, maxY, src, 0, 0, src.maxX, src.maxY);
		return;
} // ogSurface::ogScale

void ogSurface::ogScaleBuf(int32 dX1, int32 dY1, int32 dX2, int32 dY2,
						   ogSurface& src, 
						   int32 sX1, int32 sY1, int32 sX2, int32 sY2) 
{

	uint32_t sWidth, dWidth;
	uint32_t sHeight, dHeight;
	int32 sx, sy, xx, yy;
	uint32_t xInc, yInc;
	uint32_t origdX1, origdY1;
	ogPixelFmt pixFmt;
	ogSurface * tmpBuf;
	ogSurface * sBuf;
	ogSurface * dBuf;
	bool doCopyBuf;

	origdX1 = origdY1 = 0; // to keep the compiler from generating a warning

	if (!ogAvail()) return;
	if (!src.ogAvail()) return;

	if (sX1 > sX2) 
	{
		xx = sX1;
		sX1= sX2;
		sX2= xx;
	}

	if (sY1 > sY2) 
	{
		yy = sY1;
		sY1= sY2;
		sY2= yy;
	}

	// if any part of the source falls outside the buffer then don't do anything

	if (((uint32_t)sX1 > src.maxX) || ((uInt32)sX2 > src.maxX) ||
		((uint32_t)sY1 > src.maxY) || ((uInt32)sY2 > src.maxY)) return;

	if (dX1 > dX2) 
	{
		xx = dX1;
		dX1= dX1;
		dX2= xx;
	}

	if (dY1 > dY2) 
	{
		yy = dY1;
		dY1= dY2;
		dY2= yy;
	}

	dWidth = (dX2-dX1)+1;
	if (dWidth <= 0) return;

	dHeight = (dY2-dY1)+1;
	if (dHeight <= 0) return;

	sWidth = (sX2-sX1)+1;
	sHeight = (sY2-sY1)+1;

	// convert into 16:16 fixed point ratio
	xInc = (sWidth << 16) / dWidth;
	yInc = (sHeight << 16) / dHeight;

	if (dX2 > (int32)maxX) 
	{
		xx = (xInc*(dX1-maxX)) >> 16;
		sX1 -= xx;
		sWidth -= xx;
		dWidth -= (dX1-maxX);
		dX1 = maxX;
	}

	if (dY2 > (int32)maxY) 
	{
		yy = (yInc*(dY2-maxY)) >> 16;
		sY2 -= yy;
		sHeight -= yy;
		dHeight -= (dY2-maxY);
		dY2 = maxY;
	}

	if (dX1 < 0) 
	{
		xx = (xInc*(-dX1)) >> 16;
		sX1 += xx;
		sWidth -= xx;
		dWidth += dX1;
		dX1 = 0;
	}

	if (dY1 < 0) 
	{
		yy = (yInc*(-dY1)) >> 16;
		sY1 += yy;
		sHeight -= yy;
		dHeight += dY1;
		dY1 = 0;
	}

	if ((dWidth <= 0) || (dHeight <= 0)) return;
	if ((sWidth <= 0) || (sHeight <= 0)) return;

	// Do a quick check to see if the scale is 1:1 .. in that case just copy
	// the image

	if ((dWidth == sWidth) && (dHeight == sHeight)) 
	{
		ogCopyBuf(dX1, dY1, src, sX1, sY1, sX2, sY2);
		return;
	}

	tmpBuf = NULL;

	/*
	* Alright.. this is how we're going to optimize the case of different
	* pixel formats.  We are going to use copyBuf() to automagically do
	* the conversion for us using tmpBuf.  Here's how it works:
	* If the source buffer is smaller than the dest buffer (ie, we're making
	* something bigger) we will convert the source buffer first into the dest
	* buffer's pixel format.  Then we do the scaling.
	* If the source buffer is larger than the dest buffer (ie, we're making
	* something smaller) we will scale first and then use copyBuf to do
	* the conversion.
	* This method puts the onus of conversion on the copyBuf() function which,
	* while not excessively fast, does the job.
	* The case in which the source and dest are the same size is handled above.
	*
	*/
	if (pixFmtID != src.pixFmtID) 
	{

		tmpBuf = new ogSurface();
		if (tmpBuf == NULL) return;
		if (sWidth*sHeight*src.bytesPerPix <= dWidth*dHeight*bytesPerPix) 
		{
			// if the number of pixels in the source buffer is less than the
			// number of pixels in the dest buffer then...
			ogGetPixFmt(pixFmt);
			if (!tmpBuf->ogCreate(sWidth, sHeight, pixFmt)) return;
			tmpBuf->ogCopyPalette(src);
			tmpBuf->ogCopyBuf(0, 0, src, sX1, sY1, sX2, sY2);
			sX2 -= sX1;
			sY2 -= sY1;
			sX1 = 0;
			sY1 = 0;
			sBuf = tmpBuf;
			dBuf = this;
			doCopyBuf = false; // do we do a copyBuf later?
		} 
		else 
		{
			src.ogGetPixFmt(pixFmt);
			if (!tmpBuf->ogCreate(dWidth,dHeight,pixFmt)) return;
			tmpBuf->ogCopyPalette(*this);
			origdX1 = dX1;
			origdY1 = dY1;
			dX1 = 0;
			dY1 = 0;
			dX2 = tmpBuf->maxX;
			dY2 = tmpBuf->maxY;
			sBuf = &src;
			dBuf = tmpBuf;
			doCopyBuf = true;
		} // else
	} 
	else 
	{
		// pixel formats are identical
		sBuf = &src;
		dBuf = this;
		doCopyBuf = false;
	} // else

	sy = sY1 << 16;

	for (yy = dY1; yy <= dY2; yy++) 
	{
		sx = 0;
		for (xx = dX1; xx <= dX2; xx++) 
		{
			dBuf->RawSetPixel(xx, yy,
				sBuf->RawGetPixel(sX1+(sx >> 16),(sy>>16)));
			sx += xInc;
		} // for xx
		sy += yInc;
	} // for yy

	if ((doCopyBuf) && (tmpBuf != NULL))
		ogCopyBuf(origdX1, origdY1, *tmpBuf, 0, 0, tmpBuf->maxX, tmpBuf->maxY);

	delete tmpBuf;
} // void ogSurface::ogScaleBuf()

uint32_t ogSurface::ogSetAlpha(uInt32 newAlpha) 
{
	if (attributes != NULL) 
	{
		uint32_t tmp = attributes->defaultAlpha;
		attributes->defaultAlpha = newAlpha;
		return tmp;
	} 
	else 
	{
		return newAlpha;
	}
} // ogSurface::ogSetAlpha

bool ogSurface::ogSetAntiAliasing(bool antiAliasing) 
{
	if (attributes != NULL)
	{
		bool previousAA = attributes->antiAlias;
		attributes->antiAlias = antiAliasing;
		return previousAA;
	} 
	else 
	{
		return antiAliasing;	// fail quietly
	}
} // bool ogSurface::ogSetAntiAliasing()

bool ogSurface::ogSetBlending(bool _blending) 
{
	bool tmp;

	if (attributes != NULL) {
		tmp = attributes->blending;
		attributes->blending = _blending;
		return tmp;
	} else return _blending;

} // bool ogSurface::ogSetBlending()

ogErrorCode ogSurface::ogSetLastError(ogErrorCode latestError) 
{
	ogErrorCode tmp = lastError;
	lastError = latestError;
	return tmp;
} // ogErrorCode ogSurface::ogSetLastError()

void ogSurface::ogSetPalette(const ogRGBA8 newPal[256]) 
{
	if (pal == NULL) return;
	for (size_t index = 0; index < 256; index++)
	{
		pal[index].red = newPal[index].red;
		pal[index].green = newPal[index].green;
		pal[index].blue = newPal[index].blue;
		pal[index].alpha = newPal[index].alpha;
	} // for index
} // void ogSurface::ogSetPalette()

void ogSurface::ogSetPalette(uInt8 colour, uInt8 red, uInt8 green, uInt8 blue, uInt8 alpha) 
{
	if (pal == NULL) return;
	pal[colour].red   = red;
	pal[colour].green = green;
	pal[colour].blue  = blue;
	pal[colour].alpha = alpha;
} // void ogSurface::ogSetPalette()

void ogSurface::ogSetPalette(uInt8 colour, uInt8 red, uInt8 green, uInt8 blue) 
{
	if (pal == NULL) return;
	pal[colour].red   = red;
	pal[colour].green = green;
	pal[colour].blue  = blue;
	pal[colour].alpha = ogGetAlpha();
} // void ogSurface::ogSetPalette()

void ogSurface::ogSetPixel(int32 x, int32 y, uint32_t colour) 
{
	if (!ogAvail()) return;

	if (((uint32_t)x > maxX) || ((uInt32)y > maxY)) return;

	RawSetPixel(x, y, colour);
} // void ogSurface::ogSetPixel()

void ogSurface::ogSetPixel(int32 x, int32 y, uInt8 r, uInt8 g, uInt8 b, uInt8 a) {
	if (!ogAvail()) return;

	if (((uint32_t)x > maxX) || ((uInt32)y > maxY)) return;
	
	RawSetPixel(x, y, r, g, b, a);
} // void ogSurface::ogSetPixel()

uint32_t ogSurface::ogSetTransparentColor(uInt32 colour) 
{
	uint32_t preColour = 0;  

	if (attributes != NULL) 
	{
		preColour = attributes->transparentColor & ogGetAlphaMasker();
		attributes->transparentColor = colour & ogGetAlphaMasker();
	} // if

	return preColour;
} // uint32_t ogSurface::ogSetTransparentColor()

//static double f(double g) { return g*g*g-g; }

void ogSurface::ogSpline(uint32_t numPoints, ogPoint2d* points, uInt32 segments,
						 uint32_t colour) 
{
	int32 i, oldY, oldX, x, y, j;
	double part, t, xx, yy, tmp;
	double * zc;
	double * dx;
	double * dy;
	double * u;
	double * wndX1;
	double * wndY1;
	double * px;
	double * py;

	auto f = ([] (double g) -> double { return g*g*g*-g; });
	bool runOnce;

	if ((numPoints < 2) || (points == NULL)) return;

	zc = new double[numPoints];
	dx = new double[numPoints];
	dy = new double[numPoints];
	u  = new double[numPoints];
	wndX1 = new double[numPoints];
	wndY1 = new double[numPoints];
	px = new double[numPoints];
	py = new double[numPoints];

	do {
		if (zc == NULL) break;
		if (dx == NULL) break;
		if (dy == NULL) break;
		if (wndX1 == NULL) break;
		if (wndY1 == NULL) break;
		if (px == NULL) break;
		if (py == NULL) break;

		for (i = 0; (uint32_t)i < numPoints; i++) 
		{
			zc[i] = dx[i] = dy[i] = u[i] = wndX1[i] = wndY1[i] = px[i] = py[i] = 0.0f;
		}

		runOnce = false;
		oldX = oldY = 0;

		x = points[0].x;
		y = points[0].y;

		for (i = 1; (uint32_t)i < numPoints; i++) 
		{
			xx = points[i-1].x - points[i].x;
			yy = points[i-1].y - points[i].y;
			t = sqrt(xx*xx + yy*yy);
			zc[i] = zc[i-1]+t;
		} // for

		u[0] = zc[1] - zc[0] +1;
		for (i = 1; (uint32_t)i < numPoints-1; i++)
		{
			u[i] = zc[i+1]-zc[i]+1;
			tmp = 2*(zc[i+1]-zc[i-1]);
			dx[i] = tmp;
			dy[i] = tmp;
			wndY1[i] = 6.0f*((points[i+1].y-points[i].y)/u[i]-
				(points[i].y-points[i-1].y)/u[i-1]);
			wndX1[i] = 6.0f*((points[i+1].x-points[i].x)/u[i]-
				(points[i].x-points[i-1].x)/u[i-1]);
		} // for

		for (i = 1; (uint32_t)i < numPoints-2; i++) 
		{
			wndY1[i+1] = wndY1[i+1]-wndY1[i]*u[i]/dy[i];
			dy[i+1] = dy[i+1]-u[i]*u[i]/dy[i];
			wndX1[i+1] = wndX1[i+1]-wndX1[i]*u[i]/dx[i];
			dx[i+1] = dx[i+1]-u[i]*u[i]/dx[i];
		} // for

		for (i = numPoints-2; i > 0; i--) 
		{
			py[i] = (wndY1[i]-u[i]*py[i+1])/dy[i];
			px[i] = (wndX1[i]-u[i]*px[i+1])/dx[i];
		} // for

		for (i = 0; (uint32_t)i < numPoints-1; i++) 
		{
			for (j = 0; (uint32_t)j <= segments; j++) 
			{
				part = zc[i]-(((zc[i]-zc[i+1])/segments)*j);
				t = (part-zc[i])/u[i];
				part = t * points[i+1].y +
					(1.0-t)*points[i].y +
					u[i] * u[i] * ( f(t) * py[i+1] + f(1.0-t) * py[i]) /6.0;
				//        y = Round(part);
				y = static_cast<int32>(part+0.5f);
				part = zc[i]-(((zc[i]-zc[i+1])/segments)*j);
				t = (part-zc[i])/u[i];
				part = t*points[i+1].x+(1.0-t)*points[i].x+u[i]*u[i]*(f(t)*px[i+1]+
					f(1.0-t)*px[i])/6.0;

				//      x = Round(part);
				x = static_cast<int32>(part+0.5f);
				if (runOnce) ogLine(oldX, oldY, x, y, colour); else runOnce = true;
				oldX = x;
				oldY = y;
			} // for j
		} // for i
	} while (false);

	delete [] py;
	delete [] px;
	delete [] wndY1;
	delete [] wndX1;
	delete [] u;
	delete [] dy;
	delete [] dx;
	delete [] zc;

} // void ogSurface::ogSpline()

void ogSurface::ogTriangle(int32 x1, int32 y1, int32 x2, int32 y2, int32 x3, int32 y3, uint32_t colour) 
{
	ogLine(x1, y1, x2, y2,colour);
	ogLine(x2, y2, x3, y3,colour);
	ogLine(x3, y3, x1, y1,colour);
} // void ogSurface::ogTriangle()

void ogSurface::ogUnpack(uint32_t colour, uInt8& red, uInt8& green, uInt8& blue) 
{
	switch (bytesPerPix) 
	{
	case 4:
	case 3:
		red   = colour >> redFieldPosition;
		green = colour >> greenFieldPosition;
		blue  = colour >> blueFieldPosition;
		break;
	case 2:
		red   = ((colour >> redFieldPosition) << redShifter);
		green = ((colour >> greenFieldPosition) << greenShifter);
		blue  = ((colour >> blueFieldPosition) << blueShifter);
		if (red != 0)   red += ogPixelFmt::OG_MASKS[redShifter];
		if (green != 0) green += ogPixelFmt::OG_MASKS[greenShifter];
		if (blue != 0)  blue += ogPixelFmt::OG_MASKS[blueShifter];
		break;
	case 1:

		if (pal == NULL) 
		{
			red = green = blue = 0;
			return;
		} // if pal == null

		if (colour > 255) colour &= 255;
		red   = pal[colour].red;
		green = pal[colour].green;
		blue  = pal[colour].blue; 
		break;
	default:
		red   = 0;
		green = 0;
		blue  = 0;
	} // switch

} // void ogSurface::ogUnpack()

void ogSurface::ogUnpack(uint32_t colour, uInt8& red, uInt8& green, uInt8& blue, uInt8& alpha) 
{

	switch (bytesPerPix) {
	case 4:
		red   = colour >> redFieldPosition;
		green = colour >> greenFieldPosition;
		blue  = colour >> blueFieldPosition;
		alpha = colour >> alphaFieldPosition;
		break;
	case 3:
		red   = colour >> redFieldPosition;
		green = colour >> greenFieldPosition;
		blue  = colour >> blueFieldPosition;
		alpha = ogGetAlpha();
		break;
	case 2:
		red   = ((colour >> redFieldPosition)   << redShifter);
		green = ((colour >> greenFieldPosition) << greenShifter);
		blue  = ((colour >> blueFieldPosition)  << blueShifter);
		if (red != 0)   red += ogPixelFmt::OG_MASKS[redShifter];
		if (green != 0) green += ogPixelFmt::OG_MASKS[greenShifter];
		if (blue != 0)  blue += ogPixelFmt::OG_MASKS[blueShifter];

		if (alphaShifter != 8) {
			alpha = (colour >> alphaFieldPosition) << alphaShifter;
			if (alpha != 0) alpha += ogPixelFmt::OG_MASKS[alphaShifter];
		} else alpha = ogGetAlpha();

		break;
	case 1:

		if (pal == NULL) 
		{
			red = green = blue = alpha = 0;
			return;
		} // if pal == null

		if (colour > 255) colour &= 255;
		red   = pal[colour].red;
		green = pal[colour].green;
		blue  = pal[colour].blue; 
		alpha = pal[colour].alpha;
		break;
	default:
		red = green = blue = alpha = 0;
	} // switch

	return;
} // void ogSurface::ogUnpack()

void ogSurface::ogVFlip()
{
	if (!ogAvail()) return;

	for (uint32_t yy = 0; yy <= maxY; yy++)
	{
		for (uint32_t xx = 0; xx < maxX/2; xx++)
		{
			uint32_t swapColour = RawGetPixel(xx, yy);
			RawSetPixel(xx, yy, RawGetPixel(maxX-xx, yy));
			RawSetPixel(maxX-xx, yy, swapColour);
		} // for xx
	} // for yy
} // void ogSurface::ogVFlip()

void ogSurface::ogVLine(int32 x, int32 y1, int32 y2, uint32_t colour) 
{
	int32 tmp;
	uInt8 r, g, b, a;

	if (!ogAvail()) return;
	if ((uint32_t)x > maxX) return;

	if (y1 > y2) 
	{
		tmp= y1;
		y1 = y2;
		y2 = tmp;
	} // if

	if (y1 < 0) y1 = 0;
	if (y2 > (int32)maxY) y2 = maxY;
	if (y2 < y1) return;

	if (ogIsBlending()) {

		ogUnpack(colour, r, g, b, a);

		if (a == 0) return;

		if (a != 255) {
			for (tmp = y1; tmp <= y2; tmp++) 
				RawSetPixel(x, tmp, r, g, b, a);
			return;
		} // if 

	} // if blending
	for (int32 yy = y1; yy <= y2; yy++)
	{
		RawSetPixel(x, yy, colour);
	}
} // void ogSurface::ogVLine()

ogSurface::~ogSurface(void) {

	if (dataState == ogOwner) 
	{
		delete [] pal;
		delete [] lineOfs;
		delete attributes;
#ifdef __UBIXOS_KERNEL__
		kfree(buffer);
#else
		free(buffer);
#endif
	}  // if datastate == ogOwner

	pal    = NULL;
	lineOfs= NULL;
	buffer = NULL;
	attributes = NULL;
	bSize  = 0;
	lSize  = 0;
	dataState = ogNone;
	return;
} // ogSurface::~ogSurface
