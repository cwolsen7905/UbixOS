#ifndef STYPES_H
#define STYPES_H
 
#include <string>
#include <sStyle.h>
#include <ogTypes.h>
#include <objgfx40.h>
#include <ogPixelFmt.h>

class sString : public sStyle, public std::string {
 public:
           sString(void) : std::string("") { };
           sString(const std::string s) : std::string(s) { };
  virtual ~sString(void) { };
}; // sString

class sBGColor : public sStyle {
 public:
  ogRGBA8 colors[4];
           sBGColor(void);
  virtual ~sBGColor(void);
}; //  sBGColor

class sRGB8Color : public sStyle, public ogRGB8 {
 public:
           sRGB8Color(void) { red = green = blue = 0; }
           sRGB8Color(uInt8, uInt8, uInt8);
  virtual ~sRGB8Color(void) { };
}; // sRGB8Color

class sRGBA8Color : public sStyle, public ogRGBA8 {
 public:
           sRGBA8Color(void) { red = green = blue = alpha = 0; }
           sRGBA8Color(uInt8, uInt8, uInt8, uInt8);
  virtual ~sRGBA8Color(void) { };
}; // sRGBA8Color

class sSize : public sStyle {
 public:
   uInt32 size;
   uInt32 width;
   uInt32 height;
           sSize(void) { size = width = height = 0; }
           sSize(uInt32 _size) { size = _size; width = height = 0; }
           sSize(uInt32, uInt32);
           sSize(uInt32, uInt32, uInt32);
  virtual ~sSize(void); 
}; // sSize

class sPixelFormat : public sStyle, public ogPixelFmt {
 public:
//            sPixelFormat(void);
            sPixelFormat(uInt8, 
                         uInt8, uInt8, uInt8, uInt8, 
                         uInt8, uInt8, uInt8, uInt8);
  virtual  ~sPixelFormat(void) { };
}; // sPixelFormat

#endif
