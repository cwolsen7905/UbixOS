#include <objgfx40/ogPixelFmt.h>
#include <objgfx40/objgfx40.h>

extern "C" {
#ifdef __UBIXOS_KERNEL__
#include <sys/types.h>
#else
#include <sys/types.h>
#endif
}

ogPixelFmt::ogPixelFmt(void) : BPP(0),
                               redFieldPosition(0), greenFieldPosition(0),
                               blueFieldPosition(0), alphaFieldPosition(0),
                               redMaskSize(0), greenMaskSize(0),
                               blueMaskSize(0), alphaMaskSize(0) { 
  for (int i = 0; i < 7; i++) reserved[i] = 0;
  return;
}

ogPixelFmt::ogPixelFmt(uInt8 bitsPerPix, 
                       uInt8 RFP, uInt8 GFP, uInt8 BFP, uInt8 AFP,
                       uInt8 RMS, uInt8 GMS, uInt8 BMS, uInt8 AMS) {

  BPP = bitsPerPix;
  redFieldPosition   = RFP;
  greenFieldPosition = GFP;
  blueFieldPosition  = BFP;
  alphaFieldPosition = AFP;
  redMaskSize   = RMS;
  greenMaskSize = GMS;
  blueMaskSize  = BMS;
  alphaMaskSize = AMS;
  
  for (int i = 0; i < 7; i++) reserved[i] = 0;

  return; 
} // ogPixelFmt::ogPixelFmt()
