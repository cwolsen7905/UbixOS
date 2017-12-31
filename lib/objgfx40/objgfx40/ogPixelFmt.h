#ifndef OGPIXELFMT_H
#define OGPIXELFMT_H

#include "ogTypes.h"
#include "objgfx40.h"

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
           ogPixelFmt(void);
           ogPixelFmt(uInt8, uInt8, uInt8, uInt8, uInt8,
                      uInt8, uInt8, uInt8, uInt8);
  virtual ~ogPixelFmt(void) {}
}; // ogPixelFmt

static ogPixelFmt OG_NULL_PIXFMT      = ogPixelFmt(0,  0,0,0,0,   0,0,0,0);
static ogPixelFmt OG_PIXFMT_8BPP      = ogPixelFmt(8,  0,0,0,0,   0,0,0,0);
static ogPixelFmt OG_PIXFMT_15BPP     = ogPixelFmt(15, 10,5,0,15, 5,5,5,1);
static ogPixelFmt OG_PIXFMT_16BPP     = ogPixelFmt(16, 11,5,0,0,  5,6,5,0);
static ogPixelFmt OG_PIXFMT_24BPP     = ogPixelFmt(24, 16,8,0,0,  8,8,8,0);
static ogPixelFmt OG_PIXFMT_32BPP     = ogPixelFmt(32, 16,8,0,24, 8,8,8,8);
static ogPixelFmt OG_MAC_PIXFMT_16BPP = ogPixelFmt(16, 8,4,0,12,  4,4,4,4);

#endif
