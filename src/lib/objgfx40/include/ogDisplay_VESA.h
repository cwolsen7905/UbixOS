#ifndef OGDISPLAY_VESA_H
#define OGDISPLAY_VESA_H

#include "objgfx40.h"

struct ogModeInfo {
    uInt16 modeAttributes     __attribute__((packed));
    uInt8  windowAFlags       __attribute__((packed));
    uInt8  windowBFlags       __attribute__((packed));
    uInt16 granularity        __attribute__((packed));
    uInt16 windowSize         __attribute__((packed));
    uInt16 windowASeg         __attribute__((packed));
    uInt16 windowBSeg         __attribute__((packed));
    void*  bankSwitch         __attribute__((packed));
    uInt16 bytesPerLine       __attribute__((packed));
    uInt16 xRes               __attribute__((packed));
    uInt16 yRes               __attribute__((packed));
    uInt8  charWidth          __attribute__((packed));
    uInt8  charHeight         __attribute__((packed));
    uInt8  numBitPlanes       __attribute__((packed));
    uInt8  bitsPerPixel       __attribute__((packed));
    uInt8  numberOfBanks      __attribute__((packed));
    uInt8  memoryModel        __attribute__((packed));
    uInt8  bankSize           __attribute__((packed));
    uInt8  numOfImagePages    __attribute__((packed));
    uInt8  reserved           __attribute__((packed));
    // Direct colour fields (required for Direct/6 and YUV/7 memory models
    uInt8  redMaskSize        __attribute__((packed));
    uInt8  redFieldPosition   __attribute__((packed));
    uInt8  greenMaskSize      __attribute__((packed));
    uInt8  greenFieldPosition __attribute__((packed));
    uInt8  blueMaskSize       __attribute__((packed));
    uInt8  blueFieldPosition  __attribute__((packed));
    uInt8  alphaMaskSize      __attribute__((packed));
    uInt8  alphaFieldPosition __attribute__((packed));
    uInt8  directColourMode   __attribute__((packed));
    // VESA 2.0 specific fields
    uInt32 physBasePtr        __attribute__((packed));
    void*  offScreenMemOffset __attribute__((packed));
    uInt16 offScreenMemSize   __attribute__((packed));
    uInt8  paddington[461]    __attribute__((packed));
};

struct ogVESAInfo {
    char    VBESignature[4]   __attribute__((packed));
    uInt8   minVersion        __attribute__((packed));
    uInt8   majVersion        __attribute__((packed));
    uInt32  OEMStringPtr      __attribute__((packed));
    uInt32  capabilities      __attribute__((packed));
    uInt32  videoModePtr      __attribute__((packed));
    uInt16  totalMemory       __attribute__((packed));
    // VESA 2.0 specific fields
    uInt16  OEMSoftwareRev    __attribute__((packed));
    uInt32  OEMVendorNamePtr  __attribute__((packed));
    uInt32  OEMProductNamePtr __attribute__((packed));
    uInt32  OEMProductRevPtr  __attribute__((packed));
    uInt8   paddington[474]   __attribute__((packed));
};

class ogDisplay_VESA : public ogSurface {
 protected:
  void *       pages[2];
  uInt32       activePage;
  uInt32       visualPage;
  uInt16       screenSelector;
  ogVESAInfo * VESAInfo;
  ogModeInfo * modeInfo;
  bool         inGraphics;
  uInt16       FindMode(uInt32, uInt32, uInt32);
  void         GetModeInfo(uInt16);
  void         GetVESAInfo(void);
  void         SetMode(uInt16);
  virtual uInt32 RawGetPixel(uInt32, uInt32);  
  virtual void RawSetPixel(uInt32, uInt32, uInt32);
  virtual void RawSetPixel(uInt32, uInt32, uInt8, uInt8, uInt8, uInt8);
  void         SetPal(void);
 public:
               ogDisplay_VESA(void);
  virtual bool ogAlias(ogSurface&, uInt32, uInt32, uInt32, uInt32);
  virtual bool ogAvail(void);
  virtual void ogClear(uInt32);
  virtual bool ogClone(ogSurface&);
  virtual void ogCopyLineTo(uInt32, uInt32, const void *, uInt32);
  virtual void ogCopyLineFrom(uInt32, uInt32, void *, uInt32);
  virtual void ogCopyPalette(ogSurface&);
  virtual bool ogCreate(uInt32, uInt32, ogPixelFmt);
  virtual uInt32 ogGetPixel(int32, int32);
  virtual void * ogGetPtr(uInt32, uInt32);
  virtual void ogHLine(int32, int32, int32, uInt32);
  virtual bool ogLoadPalette(const char *);  
  virtual void ogSetPalette(const ogRGBA8[]);
  virtual void ogSetPixel(int32, int32, uInt32);
  virtual void ogSetPalette(uInt8, uInt8, uInt8, uInt8);  
  virtual void ogSetPalette(uInt8, uInt8, uInt8, uInt8, uInt8);  
  virtual void ogVFlip(void);
  virtual void ogVLine(int32, int32, int32, uInt32);
  virtual      ~ogDisplay_VESA(void);
}; // ogDisplay_VESA

#endif
