#ifndef OGPIXCON_H
#define OGPIXCON_H

#include "objgfx40.h"

class ogPixCon {
 protected:
  uInt32  srcMasker;
  uInt32  srcShifter;
  uInt32  dstShifter;
 public:
  ogPixCon(ogPixelFmt, ogPixelFmt);
  uInt32 ConvPix(uInt32);
}; // ogPixCon

#endif
