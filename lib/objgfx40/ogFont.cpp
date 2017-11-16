#include <objgfx40/ogFont.h>
#include <objgfx40/objgfx40.h>

extern "C" {
#ifdef __UBIXOS_KERNEL__
  #include <vfs/file.h>
  #include <lib/string.h>
#else
  #include <string.h>
  #include <stdlib.h>
  #include <stdio.h>
#endif
  }

using namespace std;
typedef
  struct {
    char  ID[3];
    uInt8 version;
    uInt8 width, height;
    uInt8 numOfChars;
    uInt8 startingChar;
    uInt8 colourType;
    uInt8 paddington[7];
  } ogDPFHeader;

static 
bool
fileExists(const char *file)
{
#ifdef __UBIXOS_KERNEL__
  fileDescriptor *f = fopen(file, "rb");
#else
  FILE *f = fopen(file, "rb");
#endif
  if (!f)
    return false;
  fclose(f);
  return true;
}

ogBitFont::ogBitFont(void) {
  memset(fontDataIdx, 0, sizeof(fontDataIdx));
  memset(charWidthTable, 0, sizeof(charWidthTable));
  memset(charHeightTable, 0, sizeof(charHeightTable));

  fontData = NULL;
  fontDataSize = 0;
  numOfChars = 0;
  width = height = startingChar = 0;

  BGColour.red   = 0;
  BGColour.green = 0;
  BGColour.blue  = 0;
  BGColour.alpha = 0;
  
  FGColour.red   = 255;
  FGColour.green = 255;
  FGColour.blue  = 255;
  FGColour.alpha = 255;

  return;
} // ogBitFont::ogBitFont

void
ogBitFont::SetBGColor(uInt32 red, uInt32 green, uInt32 blue, uInt32 alpha) {
  BGColour.red   = red;
  BGColour.green = green;
  BGColour.blue  = blue;
  BGColour.alpha = alpha;
  return;
} // ogBitFont::SetBGColor

void
ogBitFont::SetFGColor(uInt32 red, uInt32 green, uInt32 blue, uInt32 alpha) {
  FGColour.red   = red;
  FGColour.green = green;
  FGColour.blue  = blue;
  FGColour.alpha = alpha;
  return;
} // ogBitFont::SetFGColor

ogBitFont::~ogBitFont(void) {
  memset(fontDataIdx, 0, sizeof(fontDataIdx));
  memset(charWidthTable, 0, sizeof(charWidthTable));
  memset(charHeightTable, 0, sizeof(charHeightTable));
  delete [] fontData;
  fontData = NULL;
  fontDataSize = 0;
  width = height = startingChar = 0;
  return;
} // ogBitFont::~ogBitFont;

void 
ogBitFont::CenterTextX(ogSurface& dest, int32 y, const char * textString) {
  int32 x;
  x = ((dest.ogGetMaxX()+1) - TextWidth(textString)) / 2;
  PutString(dest, x, y, textString);
  return;
} // ogBitFont::CenterTextX

void 
ogBitFont::JustifyText(ogSurface& dest, ogTextAlign horiz, ogTextAlign vert, 
                       const char * textString) {
  uInt32 x, y;

  switch (horiz) {
   case leftText:
    x = 0;
    break;
   case centerText:
    x = ((dest.ogGetMaxX())-TextWidth(textString)) / 2;
    break;
   case rightText:
    x = (dest.ogGetMaxX())-TextWidth(textString);
    break;
   default:
    return;
  } // switch

  switch (vert) {
   case topText:
    y = 0; 
    break;
   case centerText:
    y = ((dest.ogGetMaxY())-TextHeight(textString)) / 2;
    break;
   case bottomText:
    y = (dest.ogGetMaxY())-TextHeight(textString);
   default: 
    return;
  } // switch

  PutString(dest, x, y, textString);
  return;
} // ogBitFont::JustifyText

bool 
ogBitFont::Load(const char* fontFile, uInt32 offset = 0) {
#ifdef __UBIXOS_KERNEL__
  fileDescriptor * infile;
#else
  FILE * infile;
#endif
  ogDPFHeader header;
  uInt32 lresult, size;

  if (!fileExists(fontFile)) return false;

  delete [] fontData;

  infile = fopen(fontFile, "rb");
  fseek(infile, offset, SEEK_SET);
  lresult = fread(&header, sizeof(header), 1, infile);
  width = header.width;
  height = header.height;
  numOfChars = header.numOfChars;
  if (numOfChars == 0) numOfChars = 256;
  startingChar = header.startingChar;

  memset(fontDataIdx, 0, sizeof(fontDataIdx));
  memset(charWidthTable, 0, sizeof(charWidthTable));
  memset(charHeightTable, 0, sizeof(charHeightTable));
  
  size = (((uInt32)width+7) / 8)*(uInt32)height;
  fontDataSize = size* (uInt32)numOfChars;

  for (int32 tmp = startingChar; tmp <= startingChar+numOfChars-1; tmp++) {
    charWidthTable[tmp] = width;
    charHeightTable[tmp] = height;
    fontDataIdx[tmp] = (size*(tmp-startingChar));
  } // for tmp

  fontData = new uInt8[fontDataSize];

  lresult = fread(fontData, 1, fontDataSize, infile);

  fclose(infile);
  return true;
} // ogBitFont::Load

/*
bool 
ogFont::LoadFrom(const char* FontFile, uInt32 Offset) {
  return true;
} // ogFont::LoadFrom


bool 
ogFont::Save(const char* FontFile) {
  return saveTo(FontFile,0);
} // ogFont::Save
*/

uInt32 
ogBitFont::TextHeight(const char * textString) {
  uInt32 size, tmpsize;
  size = 0;
  const unsigned char * text = (const unsigned char *)textString;

  if (text != NULL)
    while (*text) {
      tmpsize = charHeightTable[*text++];
      if (tmpsize>size) size = tmpsize;
    } // while

  return size;
} // ogBitFont::TextHeight

uInt32
ogBitFont::TextWidth(const char * textString) {
  uInt32 size = 0;
  const unsigned char * text = (const unsigned char *)textString;

  if (text != NULL)
    while (*text) 
      size += charWidthTable[*text++];
  return size;
} // ogBitFont::TextWidth

/*
bool 
ogBitFont::SaveTo(const char * fontFile, int32 offset) {
  return true;
} // TDPFont::SaveTo
*/

void 
ogBitFont::PutChar(ogSurface& dest, int32 x, int32 y, const char ch) {

  uInt32 xx, xCount, yCount;
  uInt32 BGC, FGC, tColour;
  uInt8 * offset;
  uInt8 bits = 0;
  const unsigned char c = (const unsigned char)ch;

  if (fontData == NULL) return;
  if (!dest.ogAvail()) return;
 
  if (charWidthTable[c]  != 0) {
    BGC = dest.ogPack(BGColour.red, 
                      BGColour.green, 
                      BGColour.blue, 
                      BGColour.alpha); 

    BGC &= dest.ogGetAlphaMasker();

    tColour = dest.ogGetTransparentColor();

    FGC = dest.ogPack(FGColour.red, 
                      FGColour.green, 
                      FGColour.blue, 
                      FGColour.alpha); 
   
    offset = fontData;
    offset += fontDataIdx[c];

    for (yCount = 0; yCount < height; yCount++) {
      xCount = charWidthTable[c];
      xx = 0;

      do {
        if ((xx & 7) == 0) bits = *(offset++);
        if ((bits & 128) != 0)
          dest.ogSetPixel(x + xx, y+yCount , FGColour.red, FGColour.green, 
                          FGColour.blue, FGColour.alpha); 
        else
          if (BGC != tColour)
           dest.ogSetPixel(x + xx, y+yCount, BGC);

        bits += bits;
        ++xx;
      } while (--xCount);
    } // for yCount
  } // if
  return;
} // ogBitFont::PutChar

void
ogBitFont::PutString(ogSurface& dest, int32 x, int32 y, 
                     const char *textString) {

  const unsigned char *text;
  unsigned char ch;

  if (textString == NULL) return;
  if (0 == strlen(textString)) return;
  if (!dest.ogAvail()) return;

  text = (const unsigned char *)textString;

  while ((ch = *text++) != 0) {
    PutChar(dest, x, y, ch);
    x += charWidthTable[ch];
  } // while
  
  return;
} // ogBitFont::PutString
