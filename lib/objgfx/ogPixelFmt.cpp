#include "ogPixelFmt.h"
#include "objgfx.h"

ogPixelFmt::ogPixelFmt(void) : 
	BPP(0),
    redFieldPosition(0), 
	greenFieldPosition(0),
	blueFieldPosition(0), 
	alphaFieldPosition(0),
    redMaskSize(0), 
	greenMaskSize(0),
    blueMaskSize(0), 
	alphaMaskSize(0) 
{ 
	for (size_t i = 0; i < 7; i++) reserved[i] = 0;
} // ogPixelFmt::ogPixelFmt()

ogPixelFmt::ogPixelFmt(
	uInt8 bitsPerPix, 
    uInt8 RFP, 
	uInt8 GFP, 
	uInt8 BFP, 
	uInt8 AFP,
    uInt8 RMS, 
	uInt8 GMS, 
	uInt8 BMS, 
	uInt8 AMS) 
{

	BPP = bitsPerPix;
	redFieldPosition   = RFP;
	greenFieldPosition = GFP;
	blueFieldPosition  = BFP;
	alphaFieldPosition = AFP;
	redMaskSize   = RMS;
	greenMaskSize = GMS;
	blueMaskSize  = BMS;
	alphaMaskSize = AMS;
  
	for (size_t i = 0; i < 7; i++) reserved[i] = 0;

} // ogPixelFmt::ogPixelFmt()

ogPixelFmt const OG_PIXFMT_NONE      = ogPixelFmt(0,  0,0,0,0,   0,0,0,0);
ogPixelFmt const OG_PIXFMT_8BPP      = ogPixelFmt(8,  0,0,0,0,   0,0,0,0);
ogPixelFmt const OG_PIXFMT_15BPP     = ogPixelFmt(15, 10,5,0,15, 5,5,5,1);
ogPixelFmt const OG_PIXFMT_16BPP     = ogPixelFmt(16, 11,5,0,0,  5,6,5,0);
ogPixelFmt const OG_PIXFMT_24BPP     = ogPixelFmt(24, 16,8,0,0,  8,8,8,0);
ogPixelFmt const OG_PIXFMT_32BPP     = ogPixelFmt(32, 16,8,0,24, 8,8,8,8);
ogPixelFmt const OG_MAC_PIXFMT_16BPP = ogPixelFmt(16, 8,4,0,12,  4,4,4,4);

const uInt32 ogPixelFmt::OG_MASKS[32] = 
{
   0x00, 
   0x01, 
   0x03, 
   0x07, 
   0x0F, 
   0x1F, 
   0x3F, 
   0x7F, 
   0x0FF, 
   0x1FF, 
   0x3FF, 
   0x7FF, 
   0x0FFF, 
   0x1FFF, 
   0x3FFF,
   0x7FFF,
   0x0FFFF,
   0x1FFFF,
   0x3FFFF, 
   0x7FFFF, 
   0x0FFFFF, 
   0x1FFFFF, 
   0x3FFFFF, 
   0x7FFFFF, 
   0x0FFFFFF, 
   0x1FFFFFF, 
   0x3FFFFFF, 
   0x7FFFFFF, 
   0x0FFFFFFF, 
   0x1FFFFFFF, 
   0x3FFFFFFF, 
   0x7FFFFFFF
}; // OG_MASKS[]
 
