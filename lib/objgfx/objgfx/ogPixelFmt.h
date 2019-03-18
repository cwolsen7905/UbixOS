#pragma once

#include "ogTypes.h"
#include "objgfx.h"

struct ogPixelFmt {
	const static uInt32 OG_MASKS[32];
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
    ogPixelFmt(void);
    ogPixelFmt(uInt8, uInt8, uInt8, uInt8, uInt8,
		uInt8, uInt8, uInt8, uInt8);
  //virtual ~ogPixelFmt(void) {}
}; // struct ogPixelFmt

extern ogPixelFmt const OG_NULL_PIXFMT;
extern ogPixelFmt const OG_PIXFMT_8BPP;
extern ogPixelFmt const OG_PIXFMT_15BPP;
extern ogPixelFmt const OG_PIXFMT_16BPP;
extern ogPixelFmt const OG_PIXFMT_24BPP;
extern ogPixelFmt const OG_PIXFMT_32BPP;
extern ogPixelFmt const OG_MAC_PIXFMT_16BPP;

