extern "C" {
  #include <string.h>
  #include <stdio.h>
  #include <stdlib.h>
}

#include <objgfx40/objgfx40.h>
#include <objgfx40/ogSprite.h>

static bool
fileExists(const char *file)
{
  FILE *f = fopen(file, "rb");
  if (!f)
    return false;
  fclose(f);
  return true;
}

ogSprite::ogSprite(const ogSprite& srcSprite) {
  image         = NULL;
  pal           = NULL;
  imageSize     = 0;
  width         = 0;
  height        = 0;
  bitDepth      = 0;
  RFP           = 0;
  GFP           = 0;
  BFP           = 0;
  AFP           = 0;
  rShift        = 0;
  gShift        = 0;
  bShift        = 0;
  aShift        = 0;
  tColour       = 0;
  pixelFmtID    = 0;
  bytesPerPixel = 0;
  dAlpha        = 0;

  if ((srcSprite.image == NULL) || (srcSprite.imageSize == 0)) return;

  // allocate space for the sprite
  image = malloc(srcSprite.imageSize);
  if (image == NULL) return;
  
  // copy the image size
  imageSize = srcSprite.imageSize;
 
  if (srcSprite.pal != NULL) {
    pal = new ogRGBA8[256];
    if (pal != NULL) memcpy(srcSprite.pal, pal, sizeof(ogRGBA8) * 256);
  } // if 

  width         = srcSprite.width;
  height        = srcSprite.height;
  bitDepth      = srcSprite.bitDepth;
  RFP           = srcSprite.RFP;
  GFP           = srcSprite.GFP;
  BFP           = srcSprite.BFP;
  AFP           = srcSprite.AFP;
  rShift        = srcSprite.rShift;
  gShift        = srcSprite.gShift;
  bShift        = srcSprite.bShift;
  aShift        = srcSprite.aShift;
  tColour       = srcSprite.tColour;
  pixelFmtID    = srcSprite.bytesPerPixel;
  dAlpha        = srcSprite.dAlpha;

  return;
} // ogSprite::ogSprite

ogSprite::ogSprite(void) {
  image         = NULL;
  pal           = NULL;
  imageSize     = 0;
  width         = 0;
  height        = 0;
  bitDepth      = 0;
  RFP           = 0;
  GFP           = 0;
  BFP           = 0;
  AFP           = 0;
  rShift        = 0;
  gShift        = 0;
  bShift        = 0;
  aShift        = 0;
  tColour       = 0;
  pixelFmtID    = 0;
  bytesPerPixel = 0;
  dAlpha        = 0;
  return;
} // ogSprite::ogSprite

ogSprite &
ogSprite::operator=(ogSprite const & srcSprite) {

  if ((srcSprite.image == NULL) || (srcSprite.imageSize == NULL)) return *this;

  free(image);
  delete [] pal;

  // allocate space for the sprite
  image = malloc(srcSprite.imageSize);

  // this is such a bad case it should probably throw an exception
  if (image == NULL) return *this;

  // copy the image size
  imageSize = srcSprite.imageSize;

  if (srcSprite.pal != NULL) {
    pal = new ogRGBA8[256];
    if (pal != NULL) memcpy(srcSprite.pal, pal, sizeof(ogRGBA8) * 256);
  } // if

  width         = srcSprite.width;
  height        = srcSprite.height;
  bitDepth      = srcSprite.bitDepth;
  RFP           = srcSprite.RFP;
  GFP           = srcSprite.GFP;
  BFP           = srcSprite.BFP;
  AFP           = srcSprite.AFP;
  rShift        = srcSprite.rShift;
  gShift        = srcSprite.gShift;
  bShift        = srcSprite.bShift;
  aShift        = srcSprite.aShift;
  tColour       = srcSprite.tColour;
  pixelFmtID    = srcSprite.bytesPerPixel;
  dAlpha        = srcSprite.dAlpha;

  return *this;
}
void
ogSprite::Get(ogSurface& srcObject, int32 x1, int32 y1, int32 x2, int32 y2) {
  ogPixelFmt pixfmt;
  uInt32     xx, yy, xOfs, yOfs;
  uInt32     rx1, ry1, rx2, ry2;
  uInt32     xCount, yCount, count;
  void       *p;
  uInt32     maxX, maxY;

  if (!srcObject.ogAvail()) return;

  free(image);
  free(pal);
  
  maxX = srcObject.ogGetMaxX();
  maxY = srcObject.ogGetMaxY();
  
  srcObject.ogGetPixFmt(pixfmt);

  bitDepth = pixfmt.BPP;
  RFP      = pixfmt.redFieldPosition;
  GFP      = pixfmt.greenFieldPosition;
  BFP      = pixfmt.blueFieldPosition;
  AFP      = pixfmt.alphaFieldPosition;
  rShift   = 8-pixfmt.redMaskSize;
  gShift   = 8-pixfmt.greenMaskSize;
  bShift   = 8-pixfmt.blueMaskSize;
  aShift   = 8-pixfmt.alphaMaskSize;
 
  pixelFmtID = srcObject.ogGetPixFmtID();

  dAlpha = srcObject.ogGetAlpha();
  tColour = srcObject.ogGetTransparentColor();

  bytesPerPixel = srcObject.ogGetBytesPerPix();

  if (bytesPerPixel == 1) {
    if (pal == NULL) pal = new ogRGBA8[256];
    if (pal == NULL) return;
    srcObject.ogGetPalette(pal);
  /*  for (count = 0; count < 256; count++)
      srcObject.Unpack(count, 
                       pal[count].red, 
                       pal[count].green, 
                       pal[count].blue,
                       pal[count].alpha); */

//    memcpy(pal, srcObject.pal, sizeof(ogRGBA8)*256);
  } // if

  if (x1 > x2) {
    xx = x1;
    x1 = x2;
    x2 = xx;
  } // if

  if (y1 > y2) {
    yy = y1;
    y1 = y2;
    y2 = yy;
  } // if

  xCount = abs(x2-x1)+1;
  yCount = abs(y2-y1)+1;
  width  = xCount;
  height = yCount;
  imageSize = xCount*yCount*bytesPerPixel;

  image = malloc(imageSize);
  p = image;

  if ( ((uInt32)x1 > maxX) || ((uInt32)y1 > maxY) ||
       ((uInt32)x2 > maxX) || ((uInt32)y2 > maxY) ) {

    for (count = 0; count < (xCount*yCount); count++) {
      SetPixel(p, tColour);
      (uInt8 *)p += bytesPerPixel;
    } // for
    p = image;  // reset the pointer;
  } // if

  xOfs = 0;
  yOfs = 0;

  if (y1 < 0) {
    yCount += y1;
    ry1 = 0;
    yOfs = xCount*abs(y1);
  } else ry1 = y1;

  if (x1 < 0) {
    xCount += x1;
    rx1 = 0;
    xOfs = abs(x1);
  } else rx1 = x1;

  if (x2 > (int32)maxX) {
    xCount -= maxX-x2+1;
    rx2 = maxX;
  } else rx2 = x2;

  if (y2 > (int32)maxY) {
    yCount -= maxY-y2+1;
    ry2 = maxY;
  } else ry2 = y2;

  xCount *= bytesPerPixel;

  for (yy = 0; yy < yCount; yy++) {
    ( (uInt8 *)p ) += xOfs;
    srcObject.ogCopyLineFrom(rx1, ry1+yy, p, xCount);    
    ( (uInt8 *)p ) += xCount;
  }
  return;
} // ogSprite::Get

uInt32
ogSprite::GetPixel(void * p) {
  uInt32 result;
  switch (bytesPerPixel) {
   case 4:
    return *(uInt32 *)p;
    break;
   case 3:
    asm(
       "  xor  %%eax, %%eax   \n"  // xor     eax, eax
       "  mov  2(%%edi),%%al  \n"  // mov     al, [edi+2]
       "  shl  $16, %%eax     \n"  // shl     eax, 16
       "  mov  (%%edi), %%ax  \n"  // mov     ax, [edi]
       "  mov  %%eax, %1      \n"  // mov     result, eax
       :
       : "D" (p), "m" (result)
       );
     return result;
     break;
  case 2:
    return *(uInt16 *)p;
    break;
  case 1:
    return *(uInt8 *)p;
    break;
  default:
    return 0;
    break;
  } // switch
} // ogSprite::GetPixel

void
ogSprite::SetPixel(void * p, uInt32 colour) {

  switch (bytesPerPixel) {
  case 4:
    *(uInt32 *)p = colour;
    break;
  case 3:
    asm(
        "  mov  %%ax, (%%edi) \n"  // mov     [edi], ax
        "  shr  $16, %%eax    \n"  // shr     eax, 16
        "  mov  %%al, 2(%%edi)\n"  // mov     [edi+2],al
       :
       : "D" (p), "a" (colour)
       );
    break;
  case 2:
    *(uInt16 *)p = (uInt16)colour;
    break;
  case 1:
    *(uInt8 *)p = (uInt8)colour;
    break;
  } // switch
  return;
} // ogSprite::SetPixel

uInt32
ogSprite::GetSize(void) {
 /*
  * getSize
  *
  * returns the size of the image as it would take on disk.  This includes
  * all header related information (width/height, bitdepth, pixel format,
  * etc) along with an extra sizeof(uInt32)
  * for storing the complete size. This allows easy indexing of images,
  * since you can figure out exactly how much the image will take up on
  * disk.  This function computes the size in the exact order it is on disk.
  * If the image is 8bpp, then there is a 1024 byte palette stored after the
  * image data along with an extra sizeof(uInt32) for the palette size in
  * bytes.  Currently we store the entire palette, but later I expect we
  * will add the ability to optimize the palette so only used entries are
  * stored.
  *
  * If you were to store a single sprite in a file, getSize would equal the
  * filesize.
  */
  uInt32 tmpsize;
  char headerIdent[4];

  tmpsize = sizeof(headerIdent)+
            sizeof(uInt32)+               // total size
            sizeof(width)+sizeof(height)+ // width/height
            sizeof(bitDepth)+             // bitDepth
            sizeof(RFP)+sizeof(GFP)+sizeof(BFP)+sizeof(AFP)+ // field positions
            sizeof(rShift)+sizeof(gShift)+sizeof(bShift)+sizeof(aShift)+ // shifters
            sizeof(tColour)+              // tColour
            sizeof(bytesPerPixel)+        // bytes per pixel
            sizeof(pixelFmtID)+           // pixel format ID
            sizeof(dAlpha)+               // default alpha
            sizeof(imageSize)+            // image size in bytes
            imageSize;                    // actual image area in bytes
  if (bytesPerPixel == 1) tmpsize += sizeof(uInt32)+sizeof(ogRGBA8)*256;
  return tmpsize;
} // ogSprite::GetSize

bool
ogSprite::Load(const char * filename) {
  return LoadFrom(filename,0);
} // ogSprite::Load

bool
ogSprite::LoadFrom(const char * filename, uInt32 offset) {
  FILE * infile;
  uInt32 lresult, tresult, totSize;
  uInt32 tmpSize;
  char headerIdent[4];

  if (!fileExists(filename)) return false;
  if ((infile = fopen(filename,"rb")) == NULL) return false;
  fseek(infile, offset, SEEK_SET);

  // for now just free up the previous image.  This will be changed
  // later so it doesn't affect the current image (if any) if there
  // is a failure loading

  free(image);
  free(pal);

  image = NULL;
  imageSize = 0;
  pal = NULL;
  
  tresult = 0;   // total bytes we've read in so far
  
  lresult = fread(&headerIdent, sizeof(headerIdent), 1, infile);
  tresult += lresult*sizeof(headerIdent);
  if ((headerIdent[0] != 'S') ||
      (headerIdent[1] != 'P') ||
      (headerIdent[2] != 'R') ||
      (headerIdent[3] != (char)0x1A)) {
    fclose(infile);
    return false;
  }
  
  lresult = fread(&totSize, sizeof(totSize), 1, infile);
  tresult += lresult*sizeof(totSize);
  
  lresult = fread(&width, sizeof(width), 1, infile);
  tresult += lresult*sizeof(width);

  lresult = fread(&height, sizeof(height), 1, infile);
  tresult += lresult*sizeof(height);

  lresult = fread(&bitDepth, sizeof(bitDepth), 1, infile);
  tresult += lresult*sizeof(bitDepth);

  lresult = fread(&RFP, sizeof(RFP), 1, infile);
  tresult += lresult*sizeof(RFP);

  lresult = fread(&GFP, sizeof(GFP), 1, infile);
  tresult += lresult*sizeof(GFP);

  lresult = fread(&BFP, sizeof(BFP), 1, infile);
  tresult += lresult*sizeof(BFP);

  lresult = fread(&AFP, sizeof(AFP), 1, infile);
  tresult += lresult*sizeof(AFP);

  lresult = fread(&rShift, sizeof(rShift), 1, infile);
  tresult += lresult*sizeof(rShift);
  
  lresult = fread(&gShift, sizeof(gShift), 1, infile);
  tresult += lresult*sizeof(gShift);
  
  lresult = fread(&bShift, sizeof(bShift), 1, infile);
  tresult += lresult*sizeof(bShift);
  
  lresult = fread(&aShift, sizeof(aShift), 1, infile);
  tresult += lresult*sizeof(aShift);

  lresult = fread(&tColour, sizeof(tColour), 1, infile);
  tresult += lresult*sizeof(tColour);

  lresult = fread(&pixelFmtID, sizeof(pixelFmtID), 1, infile);
  tresult += lresult*sizeof(pixelFmtID);

  lresult = fread(&bytesPerPixel, sizeof(bytesPerPixel), 1, infile);
  tresult += lresult*sizeof(bytesPerPixel);

  lresult = fread(&dAlpha, sizeof(dAlpha), 1, infile);
  tresult += lresult*sizeof(dAlpha);

  lresult = fread(&imageSize, sizeof(imageSize), 1, infile);
  tresult += lresult*sizeof(imageSize);
  
  image = malloc(imageSize);
  if (image == NULL) {
    fclose(infile);
    return false;
  }

  // I suppose we could interchange the imageSize and record count to produce
  // the number of bytes we read it... we'll try it this way for now.
  lresult = fread(image, imageSize, 1, infile);
  tresult += lresult*imageSize;
  
  if (bytesPerPixel == 1) {
    // 8bpp sprites have palettes
    if (pal == NULL) pal = new ogRGBA8[256];
    if (pal == NULL) {
      fclose(infile);
      return false;
    } // if pal==NULL

    lresult = fread(&tmpSize, sizeof(tmpSize), 1, infile);
    tresult += lresult*sizeof(tmpSize);

    if (tmpSize > sizeof(ogRGBA8)*256) {
      fclose(infile);
      return false;
    } // if

    lresult = fread(pal, tmpSize, 1, infile);
    tresult += lresult*tmpSize;
  } // if bytesPerPixel == 1

  fclose(infile);
  return (tresult == totSize);
} // ogSprite::LoadFrom;

void
ogSprite::Put(ogSurface& destObject, int32 x, int32 y) {
  uInt32 xx, yy;
  int32 xCount, yCount;
  uInt32 yOfs;
  uInt32 xLeft, xRight;
  int32 maxX, maxY;
  void * p;
  uInt8  r, g, b, a;
  ogPixelFmt pixfmt;

  if (image == NULL) return;
  if (!destObject.ogAvail()) return;

  maxX = destObject.ogGetMaxX();
  maxY = destObject.ogGetMaxY();

  xCount = width;
  yCount = height;

  // check to see if the image is totally off the screen
  if ((x+xCount < 0) || (y+yCount < 0) || 
      (x > (int32)maxX) || (y > (int32)maxY)) return;

  p = image;

  if (y < 0) {
    yOfs = abs(y)*xCount*bytesPerPixel;
    yCount += y;
    y = 0;
  } else yOfs = 0;

  if (x < 0) {
    xLeft = abs(x)*bytesPerPixel;
    xCount += x;
    x = 0;
  } else xLeft = 0;

  if (x+xCount > maxX) {
    xRight = (xCount - (maxX-x+1))*bytesPerPixel;
    xCount = (maxX-x)+1;
  } else xRight = 0;

  if ((y+yCount) > maxY) yCount = (maxY-y)+1;

  destObject.ogGetPixFmt(pixfmt);
  
  (uInt8 *)p += yOfs;
  
  if ((destObject.ogGetPixFmtID() != pixelFmtID) || (destObject.ogIsBlending())) {

    for (yy = 0; yy < (uInt32)yCount; yy++) { 
      (uInt8 *)p += xLeft;
      
      for (xx = 0; xx < (uInt32)xCount; xx++) {
        Unpack(GetPixel(p), r, g, b, a);
        (uInt8 *)p += bytesPerPixel;
          // this could probably be rawSetPixelRGBA instead
        destObject.ogSetPixel(x+xx, y+yy, r, g, b, a);
      } // for

      (uInt8 *)p += xRight;
    } // for yy

  } else {                            // pixel formats match
    xCount *= bytesPerPixel;

    for (yy = 0; yy < (uInt32)yCount; yy++) {
      (uInt8 *)p += xLeft;
      destObject.ogCopyLineTo(x, y+yy, p, xCount);
      (uInt8 *)p += xCount;
      (uInt8 *)p += xRight;
    } // for

  } // else
  return;
} // ogSurface::Put

bool
ogSprite::Save(const char * filename) {
  return SaveTo(filename, 0);
} // ogSprite::Save

bool
ogSprite::SaveTo(const char * filename, int32 offset) {
  /*
   * saveTo
   *
   * saves a bitmap to disk.  If the file doesn't exit then we will create
   * a new one (doing this will ignore any specified offset).  If the file
   * exists, we will seek to the specified offset and place the bitmap there.
   * If offset is -1, then we seek to the end of the file.
   *
   * This function will fail on files larger than 2GB.
   *
   */
  FILE * outfile = NULL;
  char headerIdent[4];
  uInt32 tmpSize;
  
  if (image == NULL) return false;
  if ((bytesPerPixel == 1) && (pal == NULL)) return false;

  if (!fileExists(filename)) {        // file doesn't exist
    if ((outfile = fopen(filename,"wb")) == NULL) return false;
  } else {
    // file exists. Now we check to see where we put it
    if (offset == -1) {
      if ((outfile = fopen(filename, "ab")) == NULL) return false;
    } else {
      // we have an existing file and an offset to place the data
      if ((outfile = fopen(filename, "wb")) == NULL) return false;
      if (offset != 0) fseek(outfile, offset, SEEK_SET);
    } // else
  } // else

  headerIdent[0] = 'S';
  headerIdent[1] = 'P';
  headerIdent[2] = 'R';
  headerIdent[3] = (char)0x1A;  // EOF marker

  // we store exactly how bit this sucker is inside the header. This includes
  // the header information before it, and the size itself
  tmpSize = GetSize();
  fwrite(headerIdent, sizeof(headerIdent), 1, outfile);

  fwrite(&tmpSize, sizeof(tmpSize), 1, outfile);
  fwrite(&width, sizeof(width), 1, outfile);
  fwrite(&height, sizeof(height), 1, outfile);
  fwrite(&bitDepth, sizeof(bitDepth), 1, outfile);

  fwrite(&RFP, sizeof(RFP), 1, outfile);
  fwrite(&GFP, sizeof(GFP), 1, outfile);
  fwrite(&BFP, sizeof(BFP), 1, outfile);
  fwrite(&AFP, sizeof(AFP), 1, outfile);

  fwrite(&rShift, sizeof(rShift), 1, outfile);
  fwrite(&gShift, sizeof(gShift), 1, outfile);
  fwrite(&bShift, sizeof(bShift), 1, outfile);
  fwrite(&aShift, sizeof(aShift), 1, outfile);

  fwrite(&tColour, sizeof(tColour), 1, outfile);
  fwrite(&pixelFmtID, sizeof(pixelFmtID), 1, outfile);
  fwrite(&bytesPerPixel, sizeof(bytesPerPixel), 1, outfile);
  fwrite(&dAlpha, sizeof(dAlpha), 1, outfile);

  fwrite(&imageSize, sizeof(imageSize), 1, outfile);
  fwrite(image, imageSize, 1, outfile);

  if (bytesPerPixel == 1) {
    tmpSize = sizeof(ogRGBA8)*256;
    fwrite(&tmpSize, sizeof(tmpSize), 1, outfile);
    fwrite(pal, sizeof(ogRGBA8), 256, outfile);
  } // if bytesPerPixel == 1

  fclose(outfile);
  return true;
} // ogSprite::SaveTo

void
ogSprite::Unpack(uInt32 colour, uInt8& red, uInt8& green, uInt8& blue,
                 uInt8& alpha) {
  switch (bytesPerPixel) {
   case 4:
    red   = (uInt8)(colour >> RFP);
    green = (uInt8)(colour >> GFP);
    blue  = (uInt8)(colour >> BFP);
    alpha = (uInt8)(colour >> AFP);
    break;
   case 3:
    red   = (uInt8)(colour >> RFP);
    green = (uInt8)(colour >> GFP);
    blue  = (uInt8)(colour >> BFP);
    alpha = dAlpha;
    break;
   case 2:
    red   = (uInt8)(colour >> RFP) << rShift;
    green = (uInt8)(colour >> GFP) << gShift;
    blue  = (uInt8)(colour >> BFP) << bShift;
    if (red != 0)   red += OG_MASKS[rShift];
    if (green != 0) green += OG_MASKS[gShift];
    if (blue != 0)  blue += OG_MASKS[bShift];

    if (aShift != 8) {
      alpha = (uInt8)(colour >> AFP) << aShift;
      if (alpha != 0) alpha += OG_MASKS[aShift];
    } else alpha = dAlpha;

    break;
  case 1:

    if (pal == NULL) {
      red = green = blue = alpha = 0;
      return;
    } // if

    if (colour > 255) colour &= 255;
    red   = pal[colour].red;
    green = pal[colour].green;
    blue  = pal[colour].blue;
    alpha = pal[colour].alpha;
    break;
   default:
    red = green = blue = alpha = 0;
    break;
  } // switch
  return;
} // ogSprite::Unpack

ogSprite::~ogSprite(void) {
  free(image);
  delete [] pal;
  image     = NULL;
  pal       = NULL;
  imageSize = 0;
  width     = 0;
  height    = 0;
  bitDepth  = 0;
  RFP       = 0;
  GFP       = 0;
  BFP       = 0;
  AFP       = 0;
  rShift    = 0;
  gShift    = 0;
  bShift    = 0;
  aShift    = 0;
  tColour   = 0;
  pixelFmtID= 0;
  bytesPerPixel = 0;
  dAlpha    = 0;
  return;
} // ogSprite::~ogSprite


