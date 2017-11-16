#ifndef OGBLIT_H
#define OGBLIT_H

#include "ogSprite.h"

class ogBlit: public ogSprite {
 protected:
  uInt8 *        blitMask;
  uInt32         blitMaskSize;
  uInt32         totalPixCount;
  int32          startX, startY;
  int32          endX, endY;
  
  void           BlitSize(ogSurface&, int32, int32, int32, int32);
 public:
                 ogBlit(void);
                 ogBlit(const ogBlit &, bool);
  virtual void   Get(ogSurface&, int32, int32, int32, int32);
  void           GetBlitMask(ogSurface &, int32, int32, int32, int32);
  uInt32         GetBlitMaskSize(void) const { return blitMaskSize; }
  void           GetBlitWithMask(ogSurface&, int32, int32);
  virtual uInt32 GetSize(void);
  virtual bool   LoadFrom(const char *, uInt32);
  virtual void   Put(ogSurface&, int32, int32);
  virtual bool   SaveTo(const char *, int32);
  virtual        ~ogBlit(void);
}; // ogBlit

#endif
