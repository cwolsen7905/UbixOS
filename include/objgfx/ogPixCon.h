#pragma once

#include "objgfx.h"
#include "ogTypes.h"

class ogPixCon {
 protected:
  uInt32  srcMasker;
  uInt32  srcShifter;
  uInt32  dstShifter;
 public:
  ogPixCon(ogPixelFmt, ogPixelFmt);
  uInt32 ConvPix(uInt32);
}; // ogPixCon

