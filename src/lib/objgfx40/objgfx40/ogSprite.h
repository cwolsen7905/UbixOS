#ifndef OGSPRITE_H
#define OGSPRITE_H

#include "objgfx40.h"

class ogSprite {
  protected:
   void       * image;           // image data
   uInt32       imageSize;       // memory size of the image pointer
   ogRGBA8    * pal;             // palette (used for 8bpp sprites)
   uInt32       width, height;   // width and height (in pixels)
   uInt32       bitDepth;        // make this 32-bit just for alignment purposes
   uInt32       RFP;             // red field position
   uInt32       GFP;             // green field position
   uInt32       BFP;             // blue field position
   uInt32       AFP;             // alpha field position
   uInt32       rShift;          // red shifter
   uInt32       gShift;          // green shifter
   uInt32       bShift;          // blue shifter
   uInt32       aShift;          // alpha shifter
   uInt32       tColour;         // original transparent colour
   uInt32       pixelFmtID;      // pixel format id
   uInt32       bytesPerPixel;   // bytes per pixel
   uInt32       dAlpha;          // default alpha
   uInt32       GetPixel(void *);
   void         SetPixel(void *, uInt32);
   void         Unpack(uInt32, uInt8&, uInt8&, uInt8&, uInt8&);
  public:
   ogSprite &   operator=(ogSprite const &);
                ogSprite(void);
                ogSprite(const ogSprite &);
   void         Get(ogSurface&, int32, int32, int32, int32);
   uInt32       GetHeight(void) { return height; }
   uInt32       GetSize(void);
   uInt32       GetWidth(void) { return width; }
   bool         Load(const char *);
   virtual bool LoadFrom(const char *, uInt32);
   virtual void Put(ogSurface&, int32, int32);
   bool         Save(const char *);
   virtual bool SaveTo(const char *, int32);
   virtual      ~ogSprite(void);
   
}; // ogSprite
#endif
