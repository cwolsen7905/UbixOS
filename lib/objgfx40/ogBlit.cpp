extern "C" {
  #include <string.h>
  #include <stdio.h>
  #include <stdlib.h>
  }
#include <objgfx40/objgfx40.h>
#include <objgfx40/ogBlit.h>

//using namespace std;

static bool
fileExists(const char *file)
{
  FILE *f = fopen(file, "rb");
  if (!f)
    return false;
  fclose(f);
  return true;
}

ogBlit::ogBlit(ogBlit const & srcBlit, bool doFullCopy = false) : ogSprite() {

  // horrible horrible hack. This is required because I can't have
  // two constructors with the same parameter list like I can in pascal.

  // -- begin hack --
  if (doFullCopy) ogSprite::operator=(srcBlit);
  // -- end hack --

  startX = srcBlit.startX;
  startY = srcBlit.startY;
  endX   = srcBlit.endX;
  endY   = srcBlit.endY;
 
  totalPixCount = srcBlit.totalPixCount;
  blitMaskSize = srcBlit.blitMaskSize;

  blitMask = NULL;

  if (blitMaskSize != 0) {
    blitMask = new uInt8[blitMaskSize];
    if ((blitMask != NULL) && (srcBlit.blitMask != NULL))
      memcpy(blitMask, srcBlit.blitMask, blitMaskSize);
  } // if
  return;
} // ogBlit::ogBlit

ogBlit::ogBlit(void) : ogSprite() {
  blitMask      = NULL;
  blitMaskSize  = 0;
  totalPixCount = 0;
  startX        = 0;
  startY        = 0;
  endX          = 0;
  endY          = 0;
  return;
} // ogBlit::ogBlit

void
ogBlit::BlitSize(ogSurface& src, int32 x1, int32 y1, int32 x2, int32 y2) {
  uInt32 aMask;
  int32  x,y;
  uInt8  zerocount;
  uInt8  pixcount;

  bool   inZeros;
  bool   found;

  // first free the image data or the blitMask data if we already have some
  
  free(image);
  delete [] blitMask;

  image = NULL;
  blitMask = NULL;
  imageSize = 0;
  blitMaskSize = 0;

  aMask = src.ogGetAlphaMasker();
  tColour = src.ogGetTransparentColor();

  startX = x1;
  startY = y1;
  endX = x2;
  endY = y2;

  // start by locating the left-most non-transparent pixel in the region defined
  // by (x1,y1) to (x2,y2)

  found = false;
  while ((!found) && (startX <= x2)) {
    for (y = y1; y <= y2; y++)
      found |= ((src.ogGetPixel(startX, y) & aMask) != tColour);
    if (!found) ++startX;
  } // while

  // now we look for the top-most non-transparent pixel in the regsion 
  // defined by  (startX,y1) to (x2,y2)

  found = false;
  while ((!found) && (startY <= y2)) {
    for (x = startX; x <= x2; x++)
      found |= ((src.ogGetPixel(x,startY) & aMask) != tColour);
    if (!found) ++startY;
  } // while

  found = false;
  while ((!found) && (endX >= startX)) {
    for (y = startY; y <= y2; y++)
      found |= ((src.ogGetPixel(endX,y) & aMask) != tColour);
    if (!found) --endX;
  } // while

  found = false;
  while ((!found) && (endY >= startY)) {
    for (x = startX; x <= endX; x++)
      found |= ((src.ogGetPixel(x,endY) & aMask) != tColour);
    if (!found) --endY;
  } // while

  for (y = startY; y <= endY; y++) {
    zerocount = 0;
    blitMaskSize++;  // save room for xlcount
    x = startX;
    inZeros = ((src.ogGetPixel(x,y) & aMask) == tColour);

    while (x <= endX) {
      switch (inZeros) {
        case true:
          zerocount = 0;  // How many zeros?

          while ((x <= endX) && ((src.ogGetPixel(x,y) & aMask) == tColour)) {
            ++x;

            if (zerocount == 255) {
              zerocount = 0;
              blitMaskSize += 2;
            } else ++zerocount;
          } // while

          inZeros = false;
          break; // case true

        case false:
          pixcount = 0;
          blitMaskSize += 2;
          do {
            ++x;

            if (pixcount == 255) {
              blitMaskSize += 2;
              pixcount = 0;
            } else ++pixcount;

            ++totalPixCount;
       //mjikaboom     imageSize += bm;
          } while ((x <= endX) && ((src.ogGetPixel(x,y) & aMask) != tColour));
          inZeros = true;
          break; // case false
      } // switch
    } // while
  } // for

  startX -= x1;
  startY -= y1;
  endX -= x1;
  endY -= y1;

  blitMask = new uInt8[blitMaskSize]; //(uInt8 *)malloc(blitMaskSize);
//  memset(blitMask,0,blitMaskSize);
  return;
} // ogBlit::BlitSize

void
ogBlit::GetBlitMask(ogSurface& src, int32 x1, int32 y1, 
                    int32 x2, int32 y2) {

  uInt8 * blitMaskPtr;
  uInt8 * lineCountPtr;
  int32   x, y;
  bool    inZeros;
  uInt8   pixCount, zeroCount;
  uInt32  aMask;
  uInt32  tmp;

  if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  } // if
  
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  } // if

  // calculate the width/height

  width = (x2 - x1)+1;
  height = (y2 - y1)+1;

  bytesPerPixel = src.ogGetBytesPerPix();

  if (bytesPerPixel == 1) {
    if (pal != NULL) pal = new ogRGBA8[256];
    // note that tPal will check for null, so this check may be unnecessary
    if (pal != NULL) src.ogGetPalette(pal);
/*      for (tmp = 0; tmp < 256; tmp++)
      src.Unpack(tmp,
                 pal[tmp].red, 
                 pal[tmp].green, 
                 pal[tmp].blue,
                 pal[tmp].alpha); */
  } // if

  // compute the size of the blit mask and allocate memory for it

  BlitSize(src, x1, y1, x2, y2);
  
  if (blitMask == NULL) return;
  
  blitMaskPtr = blitMask;
  aMask = src.ogGetAlphaMasker();
  tColour = src.ogGetTransparentColor();

  for (y = y1+startY; y <= y1+endY; y++) {
    zeroCount = 0;
    lineCountPtr = blitMaskPtr;
    *lineCountPtr = 0;
    ++blitMaskPtr;
    x = x1+startX;
    inZeros = ((src.ogGetPixel(x,y) & aMask) == tColour);

    while (x <= x1+endX) {
      switch (inZeros) {
        case true:
          zeroCount = 0;
          while ((x <= x1+endX) && ((src.ogGetPixel(x,y) & aMask) == tColour)) {
            ++x;
            if (zeroCount == 255) {
              ++(*lineCountPtr);
              *blitMaskPtr = 255;     // offset
              ++blitMaskPtr;          // increment to next byte
              *blitMaskPtr = 0;       // runcount
              ++blitMaskPtr;          // increment to next byte
              *blitMaskPtr = 0;       // offset
              zeroCount = 0;
            } else ++zeroCount;
          } // while

          inZeros = false;  // we are no longer in zeros
          break; // case true

        case false:
          ++(*lineCountPtr);
          *blitMaskPtr = zeroCount;
          ++blitMaskPtr;
          *blitMaskPtr = 0;
          pixCount = 0;

          do {
            ++x;

            if (pixCount == 255) {
              ++(*lineCountPtr);
              *blitMaskPtr = 255;     // runcount
              ++blitMaskPtr;          // advance pointer
              *blitMaskPtr = 0;       // offset to next run
              ++blitMaskPtr;          // advance pointer
              *blitMaskPtr = 0;       // next run count (incremented below)
              pixCount = 0;
            } else ++pixCount;

            ++(*blitMaskPtr);

          } while ((x <= (x1+endX)) &&
                   ((src.ogGetPixel(x,y) & aMask) != tColour));
          ++blitMaskPtr;
          inZeros = true;      // set inZeros to true to toggle
          break; // case false
      } // switch
    } // while
  } // for y
  return;
} // ogBlit::GetBlitMask

void
ogBlit::Get(ogSurface& src, int32 x1, int32 y1, int32 x2, int32 y2) {
  int32 tmp;
  
  if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  } // if

  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  } // if

  // get the blit mask
  GetBlitMask(src, x1, y1, x2, y2);

  // now get the actual blit using the blit mask
  GetBlitWithMask(src, x1, y1);

  return;
} // ogBlit::Get

void
ogBlit::GetBlitWithMask(ogSurface & src, int32 x, int32 y) {
 /*
  * getBlitWithMask
  *
  * Retrieves the data portion of a blit using a predefined mask and
  * stores the data in the image pointer.  If the source buffer is
  * a different pixel format, we will adjust the image pointer
  * to accommodate the new data and update the pixel format.  The put()
  * function will adjust the pixels to the dest buffer as needed.
  * Before calling this routine, you must call getBlitMask.
  */

  int32  sx, sy;
  uInt8  lineCount, offset, pixCount;
  uInt32 nsy, ney;
  uInt8 *blitMaskPtr;
  void  *imagePtr;
  uInt32 distToEdge, xRight, count;
  ogPixelFmt pixFmt;

  if (blitMask == NULL) return;

  if ( (x + startX > (int32)src.ogGetMaxX()) || (x + endX < 0) ||
       (y + startY > (int32)src.ogGetMaxY()) || (y + endY < 0)) return;

  blitMaskPtr = blitMask;

  // First check to see if the pixel format we got the blitmask from
  // is different than what we're dealing with now
  // note that the first time through pixelFmtID will be 0, so this
  // will get that information

  if (src.ogGetPixFmtID() != pixelFmtID) {
    free(image);
    image = NULL;
    imageSize = 0;

    src.ogGetPixFmt(pixFmt);

    bitDepth = pixFmt.BPP;
    RFP      = pixFmt.redFieldPosition;
    GFP      = pixFmt.greenFieldPosition;
    BFP      = pixFmt.blueFieldPosition;
    AFP      = pixFmt.alphaFieldPosition;
    rShift   = 8-pixFmt.redMaskSize;
    gShift   = 8-pixFmt.greenMaskSize;
    bShift   = 8-pixFmt.blueMaskSize;
    aShift   = 8-pixFmt.alphaMaskSize;

    bytesPerPixel = src.ogGetBytesPerPix();
    pixelFmtID = src.ogGetPixFmtID();
    dAlpha   = src.ogGetAlpha();
   //  tColour = src.ogGetTransparentColor(); // done elsewhere

  } // if

  if (image == NULL) {
    imageSize = totalPixCount * bytesPerPixel;
    image = malloc(imageSize);
  } // if

  imagePtr = image;
  // If any part of the blit data is out of bounds, we need to fill it with the
  // transparent colour
  if ( (x + startX < 0) || (x + endX > (int32)src.ogGetMaxX()) ||
       (y + startY < 0) || (y + endY > (int32)src.ogGetMaxY())) {
    for (count = 0; count < totalPixCount; count++) {
      SetPixel(imagePtr, tColour);
      (uInt8 *)imagePtr += bytesPerPixel;
    } // for count
    imagePtr = image;   // reset the image pointer
  } // if

  // first do clipping on the top edge
  nsy = startY;
  if (y+startY < 0) {
    /*
     *  If we're here then part of the blit is above the top edge of the
     *  buffer.  The distance to the top of the buffer is abs(y+startY).
     *  So, we need to loop through the blit geometry and advance the
     *  relative pointers (BlitMaskPtr and ImagePtr)
     */
    for (sy = (y+startY); sy<0; sy++) {
      ++nsy;
      lineCount = *blitMaskPtr;
      ++blitMaskPtr;
      while (lineCount > 0) {
        ++blitMaskPtr;
        pixCount = *blitMaskPtr;
        ++blitMaskPtr;
        if (pixCount > 0) (uInt8 *)imagePtr += pixCount*bytesPerPixel;
        --lineCount;
      } // while
    } // for sy
  } // if

  // Now do clipping on the bottom edge. This is easy.
  if (y+endY > (int32)src.ogGetMaxY())
    ney = (src.ogGetMaxY()-y);
  else
    ney = endY;

  for (sy = nsy; (uInt32)sy <= ney; sy++) {
    sx = x+startX;
    lineCount = *blitMaskPtr;
    ++blitMaskPtr;

    while (lineCount > 0) {
      offset = *blitMaskPtr;
      ++blitMaskPtr;
      sx += offset;
      pixCount = *blitMaskPtr;
      ++blitMaskPtr;

      if (pixCount > 0) {
        if (sx <= (int32)src.ogGetMaxX()) {
          if ((sx < 0) && (sx+pixCount > 0)) {
            pixCount += sx;                         // remember, sx is negative
            (uInt8*)imagePtr -= sx*bytesPerPixel;   // remember, sx is negative
            sx = 0;
          } // if sx<0 && sx+pixcount>0

          if (sx+pixCount > (int32)src.ogGetMaxX()+1) {
            distToEdge = (src.ogGetMaxX()-sx)+1;
            xRight = (pixCount - distToEdge)*bytesPerPixel;
            pixCount = distToEdge;
          } else xRight = 0;                     // if sx+pixCount>MaxX

          if (sx >= 0)
            src.ogCopyLineFrom(sx, y+sy, imagePtr, pixCount*bytesPerPixel);

          (uInt8*)imagePtr += xRight; // get any remainter from right edge clip
        } // if sx <= MaxX

        sx += pixCount;
        (uInt8*)imagePtr += pixCount*bytesPerPixel;

      } // if pixCount>0
      --lineCount;
    } // while
  } // for

  return;
} // ogBlit::GetBlitWithMask

uInt32
ogBlit::GetSize(void) {
  return ogSprite::GetSize() +
           sizeof(totalPixCount) +
           sizeof(blitMaskSize) +
           blitMaskSize +
           sizeof(startX) +
           sizeof(startY) +
           sizeof(endX) +
           sizeof(endY);
} // ogBlit::GetSize

bool
ogBlit::LoadFrom(const char * filename, uInt32 offset) {
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
  delete [] pal;
  delete [] blitMask;
  imageSize = 0;
  blitMaskSize = 0;

  tresult = 0;   // total bytes we've read in so far
  
  lresult = fread(headerIdent, sizeof(headerIdent), 1, infile);
  tresult += lresult*sizeof(headerIdent);
  if ((headerIdent[0] != 'B') ||
      (headerIdent[1] != 'L') ||
      (headerIdent[2] != 'T') ||
      (headerIdent[3] != (uInt8)0x1A)) {
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

  lresult = fread(&totalPixCount, sizeof(totalPixCount), 1, infile);
  tresult += lresult*sizeof(totalPixCount);
  
  lresult = fread(&startX, sizeof(startX), 1, infile);
  tresult += lresult*sizeof(startX);

  lresult = fread(&startY, sizeof(startY), 1, infile);
  tresult += lresult*sizeof(startY);

  lresult = fread(&endX, sizeof(endX), 1, infile);
  tresult += lresult*sizeof(endX);

  lresult = fread(&endY, sizeof(endY), 1, infile);
  tresult += lresult*sizeof(endY);

  lresult = fread(&blitMaskSize, sizeof(blitMaskSize), 1, infile);
  tresult += lresult*sizeof(blitMaskSize);

  lresult = fread(&imageSize, sizeof(imageSize), 1, infile);
  tresult += lresult*sizeof(imageSize);
  
  blitMask = new uInt8[blitMaskSize];
  if (blitMask == NULL) {
    fclose(infile);
    return false;
  }
  
  image = malloc(imageSize);
  if (image == NULL) {
    fclose(infile);
    return false;
  }

  // read in the blit mask
  lresult = fread(blitMask, blitMaskSize, 1, infile);
  tresult += lresult*blitMaskSize;

  // read in the image data
  // it's possible that if we start saving only blit masks this section will be
  // blank
  lresult = fread(image, 1, imageSize, infile);
  tresult += lresult;

  if (bitDepth == 8) {
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
    }
    lresult = fread(pal, tmpSize, 1, infile);
    tresult += lresult*tmpSize;
  } // if bitDepth == 8

  fclose(infile);
  return (tresult == totSize);
} // ogBlit::LoadFrom

void
ogBlit::Put(ogSurface& dest, int32 x, int32 y) {
  int32  sx, sy;
  uInt32 nsy, ney;
  uInt8  lineCount, offset, pixCount;
  uInt8* blitMaskPtr;
  void * imagePtr;
  uInt32 distToEdge, xRight, xx;
  uInt8  r, g, b, a;
  ogPixelFmt pixFmt;
  
  // can we draw anything?
  if ((blitMask == NULL) || (image == NULL)) return;

  if (!dest.ogAvail()) return;

  // see if the blit is oustide the buffer
  if ( ((x+startX) > (int32)dest.ogGetMaxX()) || ((x + endX) < 0) ||
       ((y+startY) > (int32)dest.ogGetMaxY()) || ((y + endY) < 0) ) return;

  blitMaskPtr = blitMask;
  imagePtr = image;

  // first do clipping on the top edge
  nsy = startY;
  if (y+startY < 0) {
    /*
     *  If we're here then part of the blit is above the top edge of the
     *  buffer.  The distance to the top of the buffer is abs(y+startY).
     *  So, we need to loop through the blit geometry and advance the
     *  relative pointers (blitMaskPtr and imagePtr)
     */
    for (sy = (y+startY); sy < 0; sy++) {
      ++nsy;
      lineCount = *blitMaskPtr;
      ++blitMaskPtr;

      while (lineCount > 0) {
        ++blitMaskPtr;
        pixCount = *blitMaskPtr;
        ++blitMaskPtr;
        if (pixCount > 0) (uInt8 *)imagePtr += pixCount*bytesPerPixel;
        --lineCount;
      } // while
    } // for sy
  } // if

  // Now do clipping on the bottom edge. This is easy
  // y is guaranteed to be >=0
  // I'm going to contradict myself and say that I don't think y is
  // guaranteed to be >= 0. 

  if (y+endY > (int32)dest.ogGetMaxY())
    ney = (dest.ogGetMaxY()-y);
  else
    ney = endY;

  dest.ogGetPixFmt(pixFmt);

  if ((dest.ogGetPixFmtID() != pixelFmtID) || (dest.ogIsBlending())) {
    for (sy = nsy; (uInt32)sy <= ney; sy++) {
      sx = x+startX;
      lineCount = *blitMaskPtr;
      ++blitMaskPtr;
      while (lineCount > 0) {
        offset = *blitMaskPtr;
        ++blitMaskPtr;
        sx += offset;
        pixCount = *blitMaskPtr;
        ++blitMaskPtr;

        if (pixCount > 0) {
          if (sx <= (int32)dest.ogGetMaxX()) {

            if ((sx < 0) && (sx+(int32)pixCount > 0)) {
              pixCount += sx;                        // remember, sx is negative
              (uInt8*)imagePtr -= sx*bytesPerPixel;  // remember, sx is negative
              sx = 0;
            } // if sx<0 && sx+pixCount>0

            if (sx+pixCount > (int32)dest.ogGetMaxX()) {
              distToEdge = (dest.ogGetMaxX()-sx)+1;
              xRight = (pixCount-distToEdge)*bytesPerPixel;
              pixCount = distToEdge;
            } else xRight = 0; // if sx+pixCount>MaxX

            if (sx >= 0)
              for (xx = 0; xx < pixCount; xx++) {
                Unpack(GetPixel((uInt8*)imagePtr+(xx*bytesPerPixel)),
                       r, g, b, a);
                dest.ogSetPixel(sx+xx, sy+y, r, g, b, a);
              } // for
            (uInt8*)imagePtr += xRight;
          } // if sx <= maxX
          sx += pixCount;
          (uInt8*)imagePtr += pixCount*bytesPerPixel;
        } // if pixCount != 0
        --lineCount;
      } // while
    } // for
  } else {
    for (sy = nsy; (uInt32)sy <= ney; sy++) {
      sx = x+startX;
      lineCount = *blitMaskPtr;
      ++blitMaskPtr;

      while (lineCount > 0) {
        offset = *blitMaskPtr;
        ++blitMaskPtr;
        sx += offset;
        pixCount = *blitMaskPtr;
        ++blitMaskPtr;

        if (pixCount > 0) {
          if (sx <= (int32)dest.ogGetMaxX()) {
            if ((sx < 0) && (sx+pixCount > 0)) {
              pixCount += sx;                       // remember, sx is negative
              (uInt8*)imagePtr -= sx*bytesPerPixel; // remember, sx is negative
              sx = 0;
            } // if sx<0 && sx+pixCount>0

            if (sx+pixCount > (int32)dest.ogGetMaxX()+1) {
              distToEdge = (dest.ogGetMaxX()-sx)+1;
              xRight = (pixCount - distToEdge)*bytesPerPixel;
              pixCount = distToEdge;
            } else xRight = 0; // if sx+pixCount>MaxX

            if (sx >= 0)
              dest.ogCopyLineTo(sx, y+sy, imagePtr, pixCount*bytesPerPixel);
            (uInt8*)imagePtr += xRight;
          } // if sx <= MaxX
          sx += pixCount;
          (uInt8*)imagePtr += pixCount*bytesPerPixel;
        } // if pixCount>0
        --lineCount;
      } // while
    } // for
  } // else

  return;
} // ogBlit::Put

bool
ogBlit::SaveTo(const char * filename, int32 offset) {
  /*
   * saveTo
   *
   * saves a blit to disk.  If the file doesn't exit then we will create
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
  if ((bitDepth == 8) && (pal == NULL)) return false;

  if (!fileExists(filename)) {        // file doesn't exist
    if ((outfile = fopen(filename,"wb")) == NULL) return false;
  } else {
    // file exists. Now we check to see where we put it
    if (offset==-1) {
      if ((outfile = fopen(filename, "ab")) == NULL) return false;
    } else {
      // we have an existing file and an offset to place the data
      if ((outfile = fopen(filename, "wb")) == NULL) return false;
      if (offset != 0) fseek(outfile, offset, SEEK_SET);
    } // else
  } // else

  tmpSize = GetSize();
  
  headerIdent[0] = 'B';
  headerIdent[1] = 'L';
  headerIdent[2] = 'T';
  headerIdent[3] = (uInt8)0x1A;  // EOF marker

  // we store exactly how bit this sucker is inside the header. This includes
  // the header information before it, and the size itself
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

  fwrite(&totalPixCount, sizeof(totalPixCount), 1, outfile);

  fwrite(&startX, sizeof(startX), 1, outfile);
  fwrite(&startY, sizeof(startY), 1, outfile);
  fwrite(&endX, sizeof(endX), 1, outfile);
  fwrite(&endY, sizeof(endY), 1, outfile);

  fwrite(&blitMaskSize, sizeof(blitMaskSize), 1, outfile);
  fwrite(&imageSize, sizeof(imageSize), 1, outfile);

  fwrite(blitMask, blitMaskSize, 1, outfile);
  fwrite(image, 1, imageSize, outfile);

  if (bitDepth == 8) {
    tmpSize = sizeof(ogRGBA8)*256;
    fwrite(&tmpSize, sizeof(tmpSize), 1, outfile);
    fwrite(pal, sizeof(ogRGBA8), 256, outfile);
  } // if bitDepth == 8

  fclose(outfile);
  return true;
} // ogBlit::SaveTo

ogBlit::~ogBlit(void) {
  delete [] blitMask;
  blitMask = NULL;
  blitMaskSize = 0;
  return;
} // ogBlit::~ogBlit

