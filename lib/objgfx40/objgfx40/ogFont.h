#ifndef OGFONT_H
#define OGFONT_H

#include "objgfx40.h"

enum 
  ogTextAlign { 
    leftText,
    bottomText = leftText, 
    centerText,
    rightText,
    topText = rightText
  }; // textAlign

class 
  ogBitFont {
   protected:
     uInt32   fontDataIdx[256];
     uInt32   charWidthTable[256];
     uInt32   charHeightTable[256];
     uInt8  * fontData;
     uInt32   fontDataSize;
     ogRGBA8  BGColour;
     ogRGBA8  FGColour;
     uInt16   numOfChars;
     uInt8    width, height;
     uInt8    startingChar;
   public:
     ogBitFont();
     void     CenterTextX(ogSurface&, int32, const char *);
     uInt32   GetWidth(void) const { return width; }
     uInt32   GetHeight(void) const { return height; }
     void     JustifyText(ogSurface&, ogTextAlign, ogTextAlign, const char *);
     bool     Load(const char *, uInt32);
     void     PutChar(ogSurface&, int32, int32, const char);
     void     PutString(ogSurface&, int32, int32, const char *);
//     bool     Save(const char *);
     void     SetBGColor(uInt32, uInt32, uInt32, uInt32);
     void     SetFGColor(uInt32, uInt32, uInt32, uInt32);
     uInt32   TextHeight(const char *);
     uInt32   TextWidth(const char *);
     ~ogBitFont();
}; // ogBitFont

#endif
