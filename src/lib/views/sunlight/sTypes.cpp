#include <sStyle.h>
#include <sTypes.h>
#include <objgfx40.h>
#include <string>

sBGColor::sBGColor(void) {
  memset(colors, 0, sizeof(colors));
  return;
} // sBGColor::sBGColor

sBGColor::~sBGColor(void) {
  return;
} // sBGColor::~sBGColor

sRGB8Color::sRGB8Color(uInt8 r, uInt8 g, uInt8 b) {
  red   = r;
  green = g;
  blue  = b;
  return;
} // sRGB8Color::sRGB8Color

sRGBA8Color::sRGBA8Color(uInt8 r, uInt8 g, uInt8 b, uInt8 a) {
  red   = r;
  green = g;
  blue  = b;
  alpha = a;
  return;
} // sRGBA8Color::sRGBAColor

sSize::sSize(uInt32 w, uInt32 h) {
  size   = 0;
  width  = w;
  height = h;
  return;
} // sSize::sSize

sSize::sSize(uInt32 s, uInt32 w, uInt32 h) {
  size   = s;
  width  = w;
  height = h;
  return;
} // sSize::sSize

sSize::~sSize(void) {
  return;
}

sPixelFormat::sPixelFormat(uInt8 bitsPerPix,
                           uInt8 RFP, uInt8 GFP, uInt8 BFP, uInt8 AFP,
                           uInt8 RMS, uInt8 GMS, uInt8 BMS, uInt8 AMS) 
          : ogPixelFmt( bitsPerPix, RFP, GFP, BFP, AFP, RMS, GMS, BMS, AMS) {
  return;
} // sPixelFormat::sPixelFormat
