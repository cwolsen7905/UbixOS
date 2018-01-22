/*******************************************************
 $Id: objgfx40.cpp 89 2016-01-12 00:20:40Z reddawg $
 *******************************************************/

#ifndef __UBIXOS_KERNEL__
extern "C" {
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
}
#endif

#ifdef __UBIXOS_KERNEL__
extern "C" {
#include <math.h>
#include <lib/kmalloc.h>
#include <vfs/vfs.h>
#include <string.h>
}

#define abs(a)     (((a) < 0) ? -(a) : (a))
#endif

#include <objgfx40/objgfx40.h>
#include <objgfx40/ogPixCon.h>
#include <objgfx40/ogPixelFmt.h>
#include <objgfx40/defpal.inc>

#ifdef __UBIXOS_KERNEL__
#include <lib/libcpp.h>
#endif

const
uint32_t OG_MASKS[32] = {
  0,
  1,
  3,
  7,
  15,
  31,
  63,
  127,
  255,
  511,
  1023,
  2047,
  4095,
  8191,
  16383,
  32767,
  65535,
  131071,
  262143,
  524287,
  1048575,
  2097151,
  4194303,
  8388607,
  16777215,
  33554431,
  67108863,
  134217727,
  268435455,
  536870911,
  1073741823,
  2147483647
}; // OG_MASKS[]

const
float INTENSITIES[32] = {
  1.0,             // 0
  0.984250984251,// 1
  0.968245836552,// 2
  0.951971638233,// 3
  0.935414346693,// 4
  0.938558653544,// 5
  0.901387818866,// 6
  0.883883476483,// 7
  0.866025403784,// 8
  0.847791247891,// 9
  0.829156197589,// 10
  0.810092587301,// 11
  0.790569415042,// 12
  0.770551750371,// 13
  0.75,// 14
  0.728868986856,// 15
  0.707106781187,// 16
  0.684653196881,// 17
  0.661437827766,// 18
  0.637377439199,// 19
  0.612372435696,// 20
  0.586301969978,// 21
  0.559016994375,// 22
  0.53033008589,// 23
  0.5 ,// 24
  0.467707173347,// 25
  0.433012701892,// 26
  0.395284707521,// 27
  0.353553390593,// 28
  0.306186217848,// 29
  0.25,// 30
  0.176776695297// 31
}; // INTENSITIES[]

// #include "../ubixos-home/src/sys/include/ubixos/types.h"

// #define ROUND(f) (int)((f) + ((f) > 0 ? 0.5 : -0.5))

struct ogHLine {
    int32 xStart;
    int32 xEnd;
};

struct ogHLineList {
    int32 length;
    int32 yStart;
    int32 * xLeft;
    int32 * xRight;
};

struct ogPointListHeader {
    int32 length;
    ogPoint2d * PointPtr;
};

struct ogEdgeState {
    ogEdgeState* nextEdge;
    int32 x;
    int32 startY;
    int32 wholePixelXMove;
    int32 xDirection;
    int32 errorTerm;
    int32 errorTermAdjUp;
    int32 errorTermAdjDown;
    int32 count;
    ogRGBA8 colour;
    int32 rStepY;
    int32 gStepY;
    int32 bStepY;
    int32 aStepY;
    int32 rIncY;
    int32 gIncY;
    int32 bIncY;
    int32 aIncY;
};

class ogEdgeTable {
  public:
    ogEdgeState * globalEdges;
    ogEdgeState * activeEdges;
    ogEdgeTable(void) {
      globalEdges = activeEdges = NULL;
      return;
    }
    void AdvanceAET(void);
    void BuildGET(uInt32 numPoints, ogPoint2d * polyPoints);
    void BuildGET_G(uInt32 numPoints, ogPoint2d * polyPoints, ogRGBA8 * colours);
    void MoveXSortedToAET(int32 yToMove);
    void ScanOutAET(ogSurface & destObject, int32 yToScan, uInt32 colour);
    void ScanOutAET_G(ogSurface & destObject, int32 yToScan);
    void XSortAET(void);
    ~ogEdgeTable(void);
};
// ogEdgeState

void ogEdgeTable::AdvanceAET(void) {
  ogEdgeState * currentEdge;
  ogEdgeState ** currentEdgePtr;

  currentEdgePtr = &activeEdges;
  currentEdge = activeEdges;
  while (currentEdge != NULL) {
    --currentEdge->count;
    if (currentEdge->count == 0) {
      // this edge is finished, so remove it from the AET
      *currentEdgePtr = currentEdge->nextEdge;
      // I'm thinking I should dispose currentEdge here!?
    }
    else {
      // advance the edge's x coord by minimum move
      currentEdge->x += currentEdge->wholePixelXMove;
      // determine whether it's time for X to advance one extra
      currentEdge->errorTerm += currentEdge->errorTermAdjUp;
      if (currentEdge->errorTerm > 0) {
        currentEdge->x += currentEdge->xDirection;
        currentEdge->errorTerm -= currentEdge->errorTermAdjDown;
      } // if
      currentEdgePtr = &currentEdge->nextEdge;
    } // else
    currentEdge = *currentEdgePtr;
  } // while
  return;
} // ogEdgeTable::AdvanceAET

void ogEdgeTable::BuildGET(uInt32 numPoints, ogPoint2d * polyPoints) {
  int32 i, x1, y1, x2, y2, deltaX, deltaY, width, tmp;
  ogEdgeState * newEdgePtr;
  ogEdgeState * followingEdge;
  ogEdgeState ** followingEdgeLink;

  /*
   * Creates a GET in the buffer pointed to by NextFreeEdgeStruc from
   * the vertex list. Edge endpoints are flipped, if necessary, to
   * guarantee all edges go top to bottom. The GET is sorted primarily
   * by ascending Y start coordinate, and secondarily by ascending X
   * start coordinate within edges with common Y coordinates }
   */

  // Scan through the vertex list and put all non-0-height edges into
  // the GET, sorted by increasing Y start coordinate}
  for (i = 0; i < (int32) numPoints; i++) {
    // calculate the edge height and width
    x1 = polyPoints[i].x;
    y1 = polyPoints[i].y;

    if (0 == i) {
      // wrap back around to the end of the list
      x2 = polyPoints[numPoints - 1].x;
      y2 = polyPoints[numPoints - 1].y;
    }
    else {
      x2 = polyPoints[i - 1].x;
      y2 = polyPoints[i - 1].y;
    } // else i!=0

    if (y1 > y2) {
      tmp = x1;
      x1 = x2;
      x2 = tmp;

      tmp = y1;
      y1 = y2;
      y2 = tmp;
    } // if y1>y2

    // skip if this can't ever be an active edge (has 0 height)
    deltaY = y2 - y1;
    if (deltaY != 0) {
      newEdgePtr = new ogEdgeState;
      newEdgePtr->xDirection = ((deltaX = x2 - x1) > 0) ? 1 : -1;
      width = abs(deltaX);
      newEdgePtr->x = x1;
      newEdgePtr->startY = y1;
      newEdgePtr->count = newEdgePtr->errorTermAdjDown = deltaY;
      newEdgePtr->errorTerm = (deltaX >= 0) ? 0 : 1 - deltaY;

      if (deltaY >= width) {
        newEdgePtr->wholePixelXMove = 0;
        newEdgePtr->errorTermAdjUp = width;
      }
      else {
        newEdgePtr->wholePixelXMove = (width / deltaY) * newEdgePtr->xDirection;
        newEdgePtr->errorTermAdjUp = width % deltaY;
      } // else

      followingEdgeLink = &globalEdges;
      while (true) {
        followingEdge = *followingEdgeLink;
        if ((followingEdge == NULL) || (followingEdge->startY > y1) || ((followingEdge->startY == y1) && (followingEdge->x >= x1))) {
          newEdgePtr->nextEdge = followingEdge;
          *followingEdgeLink = newEdgePtr;
          break;
        } // if
        followingEdgeLink = &followingEdge->nextEdge;
      } // while
    } // if deltaY!=0
  } // for
  return;
} // ogEdgeTable::BuildGET

void ogEdgeTable::BuildGET_G(uInt32 numPoints, ogPoint2d * polyPoints, ogRGBA8 * colours) {

  int32 i, x1, y1, x2, y2, deltaX, deltaY, width, tmp;
  ogEdgeState * newEdgePtr;
  ogEdgeState * followingEdge;
  ogEdgeState ** followingEdgeLink;
  ogRGBA8 c1, c2, cTmp;

  /*
   * Creates a GET in the buffer pointed to by NextFreeEdgeStruc from
   * the vertex list. Edge endpoints are flipped, if necessary, to
   * guarantee all edges go top to bottom. The GET is sorted primarily
   * by ascending Y start coordinate, and secondarily by ascending X
   * start coordinate within edges with common Y coordinates }
   */

  // Scan through the vertex list and put all non-0-height edges into
  // the GET, sorted by increasing Y start coordinate}
  for (i = 0; i < (int32) numPoints; i++) {
    // calculate the edge height and width
    x1 = polyPoints[i].x;
    y1 = polyPoints[i].y;
    c1 = colours[i];

    if (0 == i) {
      // wrap back around to the end of the list
      x2 = polyPoints[numPoints - 1].x;
      y2 = polyPoints[numPoints - 1].y;
      c2 = colours[numPoints - 1];
    }
    else {
      x2 = polyPoints[i - 1].x;
      y2 = polyPoints[i - 1].y;
      c2 = colours[i - 1];
    } // else i!=0

    if (y1 > y2) {
      tmp = x1;
      x1 = x2;
      x2 = tmp;

      tmp = y1;
      y1 = y2;
      y2 = tmp;

      cTmp = c1;
      c1 = c2;
      c2 = cTmp;
    } // if y1>y2

    // skip if this can't ever be an active edge (has 0 height)
    deltaY = y2 - y1;
    if (deltaY != 0) {
      newEdgePtr = new ogEdgeState;
      newEdgePtr->colour = c1;
      newEdgePtr->xDirection = ((deltaX = x2 - x1) > 0) ? 1 : -1;

      newEdgePtr->rStepY = ((c2.red - c1.red + 1) << 16) / deltaY;
      newEdgePtr->gStepY = ((c2.green - c1.green + 1) << 16) / deltaY;
      newEdgePtr->bStepY = ((c2.blue - c1.blue + 1) << 16) / deltaY;
      newEdgePtr->aStepY = ((c2.alpha - c1.alpha + 1) << 16) / deltaY;

      newEdgePtr->rIncY = newEdgePtr->gIncY = 0;
      newEdgePtr->bIncY = newEdgePtr->aIncY = 0;

      width = abs(deltaX);
      newEdgePtr->x = x1;
      newEdgePtr->startY = y1;
      newEdgePtr->count = newEdgePtr->errorTermAdjDown = deltaY;
      newEdgePtr->errorTerm = (deltaX >= 0) ? 0 : 1 - deltaY;

      if (deltaY >= width) {
        newEdgePtr->wholePixelXMove = 0;
        newEdgePtr->errorTermAdjUp = width;
      }
      else {
        newEdgePtr->wholePixelXMove = (width / deltaY) * newEdgePtr->xDirection;
        newEdgePtr->errorTermAdjUp = width % deltaY;
      } // else

      followingEdgeLink = &globalEdges;
      while (true) {
        followingEdge = *followingEdgeLink;
        if ((followingEdge == NULL) || (followingEdge->startY > y1) || ((followingEdge->startY == y1) && (followingEdge->x >= x1))) {
          newEdgePtr->nextEdge = followingEdge;
          *followingEdgeLink = newEdgePtr;
          break;
        } // if
        followingEdgeLink = &followingEdge->nextEdge;
      } // while
    } // if deltaY!=0
  } // for
  return;
} // ogEdgeTable::BuildGET_G

void ogEdgeTable::MoveXSortedToAET(int32 yToMove) {
  ogEdgeState * AETEdge;
  ogEdgeState * tempEdge;
  ogEdgeState ** AETEdgePtr;
  int32 currentX;

  /* The GET is Y sorted. Any edges that start at the desired Y
   * coordinate will be first in the GET, so we'll move edges from
   * the GET to AET until the first edge left in the GET is no
   * longer at the desired Y coordinate. Also, the GET is X sorted
   * within each Y cordinate, so each successive edge we add to the
   * AET is guaranteed to belong later in the AET than the one just
   * added.
   */
  AETEdgePtr = &activeEdges;
  while ((globalEdges != NULL) && (globalEdges->startY == yToMove)) {
    currentX = globalEdges->x;
    // link the new edge into the AET so that the AET is still
    // sorted by X coordinate
    while (true) {
      AETEdge = *AETEdgePtr;
      if ((AETEdge == NULL) || (AETEdge->x >= currentX)) {
        tempEdge = globalEdges->nextEdge;
        *AETEdgePtr = globalEdges;
        globalEdges->nextEdge = AETEdge;
        AETEdgePtr = &globalEdges->nextEdge;
        globalEdges = tempEdge;
        break;
      }
      else
        AETEdgePtr = &AETEdge->nextEdge;
    } // while true
  } // while globalEdges!=NULL and globalEdges->startY==yToMove
  return;
} // ogEdgeTable::MoveXSortedToAET

void ogEdgeTable::ScanOutAET(ogSurface & destObject, int32 yToScan, uInt32 colour) {
  ogEdgeState * currentEdge;
  int32 leftX;

  /*  Scan through the AET, drawing line segments as each pair of edge
   *  crossings is encountered. The nearest pixel on or to the right
   *  of the left edges is drawn, and the nearest pixel to the left
   *  of but not on right edges is drawn
   */
  currentEdge = activeEdges;

  while (currentEdge != NULL) {
    leftX = currentEdge->x;
    currentEdge = currentEdge->nextEdge;

    if (currentEdge != NULL) {
      if (currentEdge->x > leftX)
        destObject.ogHLine(leftX, currentEdge->x - 1, yToScan, colour);
      currentEdge = currentEdge->nextEdge;
    } // if currentEdge != NULL
  } // while

  return;
} // ogEdgeTable::ScanOutAET

void ogEdgeTable::ScanOutAET_G(ogSurface & destObject, int32 yToScan) {
  ogEdgeState * currentEdge;
  int32 leftX, count;
  int32 rStepX, gStepX, bStepX, aStepX;
  int32 rIncX, gIncX, bIncX, aIncX;
  int32 lR, lG, lB, lA;
  int32 rR, rG, rB, rA;
  int32 dR, dG, dB, dA;
  int32 dist;

  /*  Scan through the AET, drawing line segments as each pair of edge
   *  crossings is encountered. The nearest pixel on or to the right
   *  of the left edges is drawn, and the nearest pixel to the left
   *  of but not on right edges is drawn
   */
  currentEdge = activeEdges;

  while (currentEdge != NULL) {
    leftX = currentEdge->x;

    lR = currentEdge->colour.red;
    lG = currentEdge->colour.green;
    lB = currentEdge->colour.blue;
    lA = currentEdge->colour.alpha;

    lR += currentEdge->rIncY >> 16;
    lG += currentEdge->gIncY >> 16;
    lB += currentEdge->bIncY >> 16;
    lA += currentEdge->aIncY >> 16;

    currentEdge->rIncY += currentEdge->rStepY;
    currentEdge->gIncY += currentEdge->gStepY;
    currentEdge->bIncY += currentEdge->bStepY;
    currentEdge->aIncY += currentEdge->aStepY;

    currentEdge = currentEdge->nextEdge;

    if (currentEdge != NULL) {
      if (leftX != currentEdge->x) {
        rR = currentEdge->colour.red;
        rG = currentEdge->colour.green;
        rB = currentEdge->colour.blue;
        rA = currentEdge->colour.alpha;

        rR += currentEdge->rIncY >> 16;
        rG += currentEdge->gIncY >> 16;
        rB += currentEdge->bIncY >> 16;
        rA += currentEdge->aIncY >> 16;

        currentEdge->rIncY += currentEdge->rStepY;
        currentEdge->gIncY += currentEdge->gStepY;
        currentEdge->bIncY += currentEdge->bStepY;
        currentEdge->aIncY += currentEdge->aStepY;

        dR = rR - lR;
        dG = rG - lG;
        dB = rB - lB;
        dA = rA - lA;

        dist = currentEdge->x - leftX;

        rStepX = (dR << 16) / dist;
        gStepX = (dG << 16) / dist;
        bStepX = (dB << 16) / dist;
        aStepX = (dA << 16) / dist;
        rIncX = gIncX = bIncX = aIncX = 0;

        for (count = leftX; count < currentEdge->x; count++) {
          destObject.ogSetPixel(count, yToScan, lR + (rIncX >> 16), lG + (gIncX >> 16), lB + (bIncX >> 16), lA + (aIncX >> 16));
          rIncX += rStepX;
          gIncX += gStepX;
          bIncX += bStepX;
          aIncX += aStepX;
        } // for
      }
      currentEdge = currentEdge->nextEdge;
    } // if currentEdge != NULL
  } // while

  return;
} // ogEdgeTable::ScanOutAET_G

void ogEdgeTable::XSortAET(void) {
  ogEdgeState * currentEdge;
  ogEdgeState * tempEdge;
  ogEdgeState ** currentEdgePtr;
  bool swapOccurred;

  if (activeEdges == NULL)
    return;

  do {
    swapOccurred = false;
    currentEdgePtr = &activeEdges;
    currentEdge = activeEdges;
    while (currentEdge->nextEdge != NULL) {
      if (currentEdge->x > currentEdge->nextEdge->x) {
        // the second edge has a lower x than the first
        // swap them in the AET
        tempEdge = currentEdge->nextEdge->nextEdge;
        *currentEdgePtr = currentEdge->nextEdge;
        currentEdge->nextEdge->nextEdge = currentEdge;
        currentEdge->nextEdge = tempEdge;
        swapOccurred = true;
      } // if
      currentEdgePtr = &((*currentEdgePtr)->nextEdge);
      currentEdge = *currentEdgePtr;
    } // while
  } while (swapOccurred);
  return;
} // ogEdgeTable::XSortAET

ogEdgeTable::~ogEdgeTable(void) {
  ogEdgeState * edge;
  ogEdgeState * tmpEdge;
  tmpEdge = globalEdges;
  // first walk the global edges and delete any non-null nodes
  while (tmpEdge != NULL) {
    edge = tmpEdge;
    tmpEdge = edge->nextEdge;
    delete edge;
  } // while
  tmpEdge = activeEdges;
  // next walk the activeEdges and delete any non-null nodes.  Note that this should
  // always be null
  while (tmpEdge != NULL) {
    edge = tmpEdge;
    tmpEdge = edge->nextEdge;
    delete edge;
  } // while
  return;
} // ogEdgeTable::~ogEdgeTable

static bool fileExists(const char *file) {
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

static int32 calculate(float mu, int32 p0, int32 p1, int32 p2, int32 p3);

// ogSurface constructor
ogSurface::ogSurface(void) {
  version = ogVERSION
  ;

  dataState = ogNone;
  buffer = NULL;
  lineOfs = NULL;
  pal = NULL;
  attributes = NULL;
  xRes = 0;
  yRes = 0;
  maxX = 0;
  maxY = 0;
  bSize = 0;
  lSize = 0;
  BPP = 0;
  bytesPerPix = 0;
  pixFmtID = 0;
  redShifter = 0;
  greenShifter = 0;
  blueShifter = 0;
  alphaShifter = 0;
  redFieldPosition = 0;
  greenFieldPosition = 0;
  blueFieldPosition = 0;
  alphaFieldPosition = 0;
  alphaMasker = 0;
  lastError = ogOK;
  return;
} // ogSurface::ogSurface

void ogSurface::AARawLine(uInt32 x1, uInt32 y1, uInt32 x2, uInt32 y2, uInt32 colour) {
  /*
   * aaRawLine
   *
   * private method
   *
   * draws an unclipped anti-aliased line from (x1,y1) to (x2,y2) using colour
   *
   */
  uInt32 erradj, erracc;
  uInt32 erracctmp, intshift, wgt, wgtCompMask;
  int32 dx, dy, tmp, xDir, i;
  uInt8 r, g, b, a;
  uInt32 alphas[32];
  bool oldBlending;

  if (y1 > y2) {
    tmp = y1;
    y1 = y2;
    y2 = tmp;

    tmp = x1;
    x1 = x2;
    x2 = tmp;
  } // if

  dx = (x2 - x1);
  if (dx >= 0)
    xDir = 1;
  else {
    dx = -dx;
    xDir = -1;
  }
//  dx = abs(dx);
  dy = (y2 - y1);

  if (dy == 0) {
    ogHLine(x1, x2, y1, colour);
    return;
  }

  if (dx == 0) {
    ogVLine(x1, y1, y2, colour);
    return;
  }

  ogUnpack(colour, r, g, b, a);

  if (!ogIsBlending())
    a = 255;

  for (i = 0; i < 32; i++) {
    alphas[i] = static_cast<uInt32>(INTENSITIES[i] * a + 0.5f);
  } // for

  oldBlending = ogSetBlending(true);

  RawSetPixel(x1, y1, r, g, b, a);

  // this is incomplete.. diagonal lines don't travel through the
  // center of pixels exactly

  do {

    if (dx == dy) {
      for (; dy != 0; dy--) {
        x1 += xDir;
        ++y1;
        RawSetPixel(x1, y1, r, g, b, a);
      } // for
      break;  // pop out to the bottom and restore the old blending state
    } // if dx==dy

    erracc = 0;
    intshift = 32 - 5;
    wgt = 12;
    wgtCompMask = 31;

    if (dy > dx) {
      /* y-major.  Calculate 32-bit fixed point fractional part of a pixel that
       * X advances every time Y advances 1 pixel, truncating the result so that
       * we won't overrun the endpoint along the X axis
       */
//      erradj = ((uInt64) dx << 32) / (uInt64)dy;
      __asm__ __volatile__ (
        " xor %%eax, %%eax   \n"
        " div %1             \n"
        " mov %%eax, %2      \n"
        :
        : "d" (dx), "b" (dy), "m" (erradj)
      );

      while (--dy) {
        erracctmp = erracc;
        erracc += erradj;
        if (erracc <= erracctmp)
          x1 += xDir;
        y1++;     // y-major so always advance Y
        /* the nbits most significant bits of erracc give us the intensity
         *  weighting for this pixel, and the complement of the weighting for
         *  the paired pixel.
         */
        wgt = erracc >> intshift;
        ogSetPixel(x1, y1, r, g, b, alphas[wgt]);
        ogSetPixel(x1 + xDir, y1, r, g, b, alphas[wgt ^ wgtCompMask]);
      } // while

    }
    else {

      /* x-major line.  Calculate 32-bit fixed-point fractional part of a pixel
       * that Y advances each time X advances 1 pixel, truncating the result so
       * that we won't overrun the endpoint along the X axis.
       */
//      erradj = ((uInt64)dy << 32) / (uInt64)dx;
      __asm__ __volatile__ (
        " xor %%eax, %%eax   \n"
        " div %1             \n"
        " mov %%eax, %2      \n"
        :
        : "d" (dy), "b" (dx), "m" (erradj)
      );

      // draw all pixels other than the first and last
      while (--dx) {
        erracctmp = erracc;
        erracc += erradj;
        if (erracc <= erracctmp)
          y1++;
        x1 += xDir;                 // x-major so always advance X
        /* the nbits most significant bits of erracc give us the intensity
         * weighting for this pixel, and the complement of the weighting for
         * the paired pixel.
         */
        wgt = erracc >> intshift;
        ogSetPixel(x1, y1, r, g, b, alphas[wgt]);
        ogSetPixel(x1, y1 + 1, r, g, b, alphas[wgt ^ wgtCompMask]);
      } // while
    } // else
    RawSetPixel(x2, y2, r, g, b, alphas[wgt]);

  } while (false);

  ogSetBlending(oldBlending);
  return;
} // ogSurface::AARawLine

uInt32 ogSurface::RawGetPixel(uInt32 x, uInt32 y) {
  uInt32 result;
  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(
        "  shl  $2, %%ecx      \n"  // shl     ecx, 2 {adjust for pixel size}
        "  add  %%esi, %%edi   \n"// add     edi, esi
        "  add  %%ecx, %%edi   \n"// add     edi, ecx
        "  mov  (%%edi),%%eax  \n"// eax,word ptr [edi]
        "  mov  %%eax, %3      \n"// mov     result, eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "m" (result)// %2, %3
      );
    break;
    case 3:
      __asm__ __volatile__(
        "  mov  %%ecx, %%eax   \n"  // mov     eax, ecx  - adjust for pixel size
        "  add  %%ecx, %%ecx   \n"// add     ecx, ecx  - adjust for pixel size
        "  add  %%eax, %%ecx   \n"// add     ecx, eax  - adjust for pixel size
        "  add  %%esi, %%edi   \n"// add     edi, esi
        "  add  %%ecx, %%edi   \n"// add     edi, ecx
        "  movzwl (%%edi),%%eax \n"// edx,word ptr [edi]
        "  xor  %%eax, %%eax   \n"
        "  mov  2(%%edi), %%al \n"// mov     al, [edi+2]
        "  shl  $16, %%eax     \n"// shl     eax, 16
        "  mov  (%%edi), %%ax  \n"// mov     ax, [edi]
        "  mov  %%eax, %3      \n"// mov     result, eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "m" (result)// %2, %3
      );
    break;
    case 2:
      __asm__ __volatile__(
        "  add  %%esi, %%edi   \n"  // add     edi, esi
        "  add  %%ecx, %%ecx   \n"// add     ecx, ecx {adjust for pixel size}
        "  add  %%ecx, %%edi   \n"// add     edi, ecx
        "  movzwl (%%edi),%%eax \n"// movzx   edx,word ptr [edi]
        "  mov  %%eax, %0      \n"// mov     result, eax
        : "=m" (result)
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x)// , "m" (result)              // %2, %3
      );
    break;
    case 1:
      __asm__ __volatile__(
        "  add  %%esi, %%edi   \n"  // add     edi, esi
        "  add  %%ecx, %%edi   \n"// add     edi, ecx
        " movzbl (%%edi),%%eax \n"// movzx   edx,byte ptr [edi]
        "  mov  %%eax, %3      \n"// mov     result, eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "m" (result)// %2, %3
      );
    break;
  } // switch
  return result;
} // ogSurface::RawGetPixel

void ogSurface::RawSetPixel(uInt32 x, uInt32 y, uInt32 colour) {
  uInt32 newR, newG, newB, inverseA;
  uInt8 sR, sG, sB, sA;
  uInt8 dR, dG, dB;

  do {
    if (ogIsBlending()) {
      ogUnpack(colour, sR, sG, sB, sA);
      if (sA == 0)
        return;
      if (sA == 255)
        break;
      inverseA = 255 - sA;
      ogUnpack(RawGetPixel(x, y), dR, dG, dB);
      newR = (dR * inverseA + sR * sA) >> 8;
      newG = (dG * inverseA + sG * sA) >> 8;
      newB = (dB * inverseA + sB * sA) >> 8;
      colour = ogPack(newR, newG, newB, inverseA);
    } // if
  } while (false);

  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        "  shl  $2, %%ecx     \n"// shl     eax, 2 {adjust for pixel size}
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%eax, (%%edi) \n"// mov     [edi], eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 3:
      __asm__ __volatile__(
        //  Calculate offset, prepare the pixel to be drawn
        "  leal (%%ecx, %%ecx, 2), %%ecx \n"// lea ecx, [ecx + ecx*2]
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
        "  shr  $16, %%eax    \n"// shr     eax, 16
        "  mov  %%al, 2(%%edi)\n"// mov     [edi+2],al
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 2:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        "  add  %%ecx, %%ecx  \n"// add     ecx, ecx {adjust for pixel size}
        "  add  %%esi, %%edi  \n"// add     edi, esi
        //    "  mov  %3, %%eax     \n"  // mov     eax, colour
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 1:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        //    "  add  (%%esi,%%ebx,4), %%edi \n" // add     edi, [esi + ebx * 4]
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%al, (%%edi) \n"// mov     [edi], al
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
  } // switch
  return;
} // ogSurface::RawSetPixel

void ogSurface::RawSetPixel(uInt32 x, uInt32 y, uInt8 r, uInt8 g, uInt8 b, uInt8 a) {
  uInt32 newR, newG, newB, inverseA;
  uInt8 dR, dG, dB;
  uInt32 colour;

  do {
    if (ogIsBlending()) {
      if (a == 0)
        return;
      if (a == 255) {
        colour = ogPack(r, g, b, a);
        break;
      } // if a == 255

      inverseA = 255 - a;
      ogUnpack(RawGetPixel(x, y), dR, dG, dB);
      newR = (dR * inverseA + r * a) >> 8;
      newG = (dG * inverseA + g * a) >> 8;
      newB = (dB * inverseA + b * a) >> 8;
      colour = ogPack(newR, newG, newB, inverseA);
    }
    else
      colour = ogPack(r, g, b, a);
  } while (false);

  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        "  shl  $2, %%ecx     \n"// shl     eax, 2 {adjust for pixel size}
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%eax, (%%edi) \n"// mov     [edi], eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 3:
      __asm__ __volatile__(
        //  Calculate offset, prepare the pixel to be drawn
        "  leal (%%ecx, %%ecx, 2), %%ecx \n"// lea ecx, [ecx + ecx*2]
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
        "  shr  $16, %%eax    \n"// shr     eax, 16
        "  mov  %%al, 2(%%edi)\n"// mov     [edi+2],al
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 2:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        "  add  %%ecx, %%ecx  \n"// add     ecx, ecx {adjust for pixel size}
        "  add  %%esi, %%edi  \n"// add     edi, esi
        //    "  mov  %3, %%eax     \n"  // mov     eax, colour
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 1:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        //    "  add  (%%esi,%%ebx,4), %%edi \n" // add     edi, [esi + ebx * 4]
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%al, (%%edi) \n"// mov     [edi], al
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
  } // switch
  return;
} // ogSurface::RawSetPixel

bool ogSurface::ClipLine(int32& x1, int32& y1, int32& x2, int32& y2) {
  /*
   *  clipLine()
   *
   *  private method
   *
   *  clips a line to (0,0),(maxX,maxY); returns true if
   *  the line segment is in bounds, false if none of the line segment is
   *  on the screen.  Uses HJI's line clipping algorithm.
   */

  int32 tx1, ty1, tx2, ty2;
  int32 OutCode;
  uInt32 AndResult, OrResult;
  AndResult = 15;
  OrResult = 0;
  OutCode = 0;
  if (x1 < 0)
    OutCode += 8;
  if (x1 > (int32) maxX)
    OutCode += 4;
  if (y1 < 0)
    OutCode += 2;
  if (y1 > (int32) maxY)
    OutCode++;

  AndResult &= OutCode;
  OrResult |= OutCode;
  OutCode = 0;

  if (x2 < 0)
    OutCode += 8;
  if (x2 > (int32) maxX)
    OutCode += 4;
  if (y2 < 0)
    OutCode += 2;
  if (y2 > (int32) maxY)
    OutCode++;

  AndResult &= OutCode;
  OrResult |= OutCode;

  if (AndResult > 0)
    return false;
  if (OrResult == 0)
    return true;

  // some clipping is required here.

  tx1 = x1;
  ty1 = y1;
  tx2 = x2;
  ty2 = y2;

  if (x1 < 0) {
    ty1 = (x2 * y1 - x1 * y2) / (x2 - x1);
    tx1 = 0;
  } // if
  else if (x2 < 0) {
    ty2 = (x2 * y1 - x1 * y2) / (x2 - x1);
    tx2 = 0;
  } // elseif

  if (x1 > (int32) maxX) {
    ty1 = (y1 * (x2 - maxX) + y2 * (maxX - x1)) / (x2 - x1);
    tx1 = maxX;
  } // if
  else if (x2 > (int32) maxX) {
    ty2 = (y1 * (x2 - maxX) + y2 * (maxX - x1)) / (x2 - x1);
    tx2 = maxX;
  } // elseif

  if (((ty1 < 0) && (ty2 < 0)) || ((ty1 > (int32) maxY) && (ty2 > (int32) maxY)))
    return false;

  if (ty1 < 0) {
    tx1 = (x1 * y2 - x2 * y1) / (y2 - y1);
    ty1 = 0;
  } // if
  else if (ty2 < 0) {
    tx2 = (x1 * y2 - x2 * y1) / (y2 - y1);
    ty2 = 0;
  } // elseif

  if (ty1 > (int32) maxY) {
    tx1 = (x1 * (y2 - maxY) + x2 * (maxY - y1)) / (y2 - y1);
    ty1 = maxY;
  } // if
  else if (ty2 > (int32) maxY) {
    tx2 = (x1 * (y2 - maxY) + x2 * (maxY - y1)) / (y2 - y1);
    ty2 = maxY;
  } // elseif

  if (((uInt32) tx1 > maxX) || ((uInt32) tx2 > maxX))
    return false;

  x1 = tx1;
  y1 = ty1;
  x2 = tx2;
  y2 = ty2;

  return true;
} // ogSurface::ClipLine

// wu's double step line algorithm blatently borrowed from:
// http://www.edepot.com/linewu.html

void ogSurface::RawLine(uInt32 x1, uInt32 y1, uInt32 x2, uInt32 y2, uInt32 colour) {
  int32 dy = y2 - y1;
  int32 dx = x2 - x1;
  int32 stepx, stepy;

  if (dy < 0) {
    dy = -dy;
    stepy = -1;
  }
  else {
    stepy = 1;
  }
  if (dx < 0) {
    dx = -dx;
    stepx = -1;
  }
  else {
    stepx = 1;
  }

  RawSetPixel(x1, y1, colour);
  RawSetPixel(x2, y2, colour);

  if (dx > dy) {
    int32 length = (dx - 1) >> 2;
    int32 extras = (dx - 1) & 3;
    int32 incr2 = (dy << 2) - (dx << 1);

    if (incr2 < 0) {
      int32 c = dy << 1;
      int32 incr1 = c << 1;
      int32 d = incr1 - dx;

      for (int32 i = 0; i < length; i++) {
        x1 += stepx;
        x2 -= stepx;

        if (d < 0) {                                      // Pattern:
          RawSetPixel(x1, y1, colour);                    //
          RawSetPixel(x1 += stepx, y1, colour);           //  x o o
          RawSetPixel(x2, y2, colour);                    //
          RawSetPixel(x2 -= stepx, y2, colour);

          d += incr1;
        }
        else {

          if (d < c) {                                     // Pattern:
            RawSetPixel(x1, y1, colour);                   //      o
            RawSetPixel(x1 += stepx, y1 += stepy, colour); //  x o
            RawSetPixel(x2, y2, colour);                   //
            RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
          }
          else {
            RawSetPixel(x1, y1 += stepy, colour);             // Pattern:
            RawSetPixel(x1 += stepx, y1, colour);             //    o o
            RawSetPixel(x2, y2 -= stepy, colour);             //  x
            RawSetPixel(x2 -= stepx, y2, colour);             //
          } // else

          d += incr2;
        } // else
      } // for i

      if (extras > 0) {

        if (d < 0) {
          RawSetPixel(x1 += stepx, y1, colour);
          if (extras > 1)
            RawSetPixel(x1 += stepx, y1, colour);
          if (extras > 2)
            RawSetPixel(x2 -= stepx, y2, colour);
        }
        else if (d < c) {
          RawSetPixel(x1 += stepx, y1, colour);
          if (extras > 1)
            RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 2)
            RawSetPixel(x2 -= stepx, y2, colour);
        }
        else {
          RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 1)
            RawSetPixel(x1 += stepx, y1, colour);
          if (extras > 2)
            RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
        }
      } // if extras > 0
    }
    else {
      int32 c = (dy - dx) << 1;
      int32 incr1 = c << 1;
      int32 d = incr1 + dx;

      for (int32 i = 0; i < length; i++) {
        x1 += stepx;
        x2 -= stepx;

        if (d > 0) {
          RawSetPixel(x1, y1 += stepy, colour);                   // Pattern:
          RawSetPixel(x1 += stepx, y1 += stepy, colour);          //      o
          RawSetPixel(x2, y2 -= stepy, colour);                   //    o
          RawSetPixel(x2 -= stepx, y2 -= stepy, colour);	  //  x
          d += incr1;
        }
        else {
          if (d < c) {
            RawSetPixel(x1, y1, colour);                          // Pattern:
            RawSetPixel(x1 += stepx, y1 += stepy, colour);        //      o
            RawSetPixel(x2, y2, colour);                          //  x o
            RawSetPixel(x2 -= stepx, y2 -= stepy, colour);        //
          }
          else {
            RawSetPixel(x1, y1 += stepy, colour);                 // Pattern:
            RawSetPixel(x1 += stepx, y1, colour);                 //    o o
            RawSetPixel(x2, y2 -= stepy, colour);                 //  x
            RawSetPixel(x2 -= stepx, y2, colour);                 //
          }

          d += incr2;
        } // else
      } // for i

      if (extras > 0) {
        if (d > 0) {
          RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 1)
            RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 2)
            RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
        }
        else if (d < c) {
          RawSetPixel(x1 += stepx, y1, colour);
          if (extras > 1)
            RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 2)
            RawSetPixel(x2 -= stepx, y2, colour);
        }
        else {

          RawSetPixel(x1 += stepx, y1 += stepy, colour);

          if (extras > 1)
            RawSetPixel(x1 += stepx, y1, colour);
          if (extras > 2) {
            if (d > c)
              RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
            else
              RawSetPixel(x2 -= stepx, y2, colour);
          } // if extras > 2

        } // else
      } // if extras > 0
    } // else

  }
  else {

    int32 length = (dy - 1) >> 2;
    int32 extras = (dy - 1) & 3;
    int32 incr2 = (dx << 2) - (dy << 1);

    if (incr2 < 0) {
      int32 c = dx << 1;
      int32 incr1 = c << 1;
      int32 d = incr1 - dy;

      for (int32 i = 0; i < length; i++) {
        y1 += stepy;
        y2 -= stepy;

        if (d < 0) {
          RawSetPixel(x1, y1, colour);
          RawSetPixel(x1, y1 += stepy, colour);
          RawSetPixel(x2, y2, colour);
          RawSetPixel(x2, y2 -= stepy, colour);

          d += incr1;
        }
        else {

          if (d < c) {
            RawSetPixel(x1, y1, colour);
            RawSetPixel(x1 += stepx, y1 += stepy, colour);
            RawSetPixel(x2, y2, colour);
            RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
          }
          else {
            RawSetPixel(x1 += stepx, y1, colour);
            RawSetPixel(x1, y1 += stepy, colour);
            RawSetPixel(x2 -= stepx, y2, colour);
            RawSetPixel(x2, y2 -= stepy, colour);
          } // else

          d += incr2;
        } // else
      } // for i

      if (extras > 0) {
        if (d < 0) {
          RawSetPixel(x1, y1 += stepy, colour);
          if (extras > 1)
            RawSetPixel(x1, y1 += stepy, colour);
          if (extras > 2)
            RawSetPixel(x2, y2 -= stepy, colour);
        }
        else if (d < c) {
          RawSetPixel(x1, y1 += stepy, colour);
          if (extras > 1)
            RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 2)
            RawSetPixel(x2, y2 -= stepy, colour);
        }
        else {
          RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 1)
            RawSetPixel(x1, y1 += stepy, colour);
          if (extras > 2)
            RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
        } // else
      } // if extras > 0
    }
    else {
      int32 c = (dx - dy) << 1;
      int32 incr1 = c << 1;
      int32 d = incr1 + dy;

      for (int32 i = 0; i < length; i++) {
        y1 += stepy;
        y2 -= stepy;

        if (d > 0) {
          RawSetPixel(x1 += stepx, y1, colour);
          RawSetPixel(x1 += stepx, y1 += stepy, colour);
          RawSetPixel(x2 -= stepx, y2, colour);
          RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
          d += incr1;
        }
        else {
          if (d < c) {
            RawSetPixel(x1, y1, colour);
            RawSetPixel(x1 += stepx, y1 += stepy, colour);
            RawSetPixel(x2, y2, colour);
            RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
          }
          else {
            RawSetPixel(x1 += stepx, y1, colour);
            RawSetPixel(x1, y1 += stepy, colour);
            RawSetPixel(x2 -= stepx, y2, colour);
            RawSetPixel(x2, y2 -= stepy, colour);
          } // else
          d += incr2;
        } // else
      } // for

      if (extras > 0) {
        if (d > 0) {
          RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 1)
            RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 2)
            RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
        }
        else if (d < c) {
          RawSetPixel(x1, y1 += stepy, colour);
          if (extras > 1)
            RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 2)
            RawSetPixel(x2, y2 -= stepy, colour);
        }
        else {
          RawSetPixel(x1 += stepx, y1 += stepy, colour);
          if (extras > 1)
            RawSetPixel(x1, y1 += stepy, colour);
          if (extras > 2) {
            if (d > c)
              RawSetPixel(x2 -= stepx, y2 -= stepy, colour);
            else
              RawSetPixel(x2, y2 -= stepy, colour);
          } // if extras > 2
        } // else
      } // if extras > 0
    } // else
  } // else
} // ogSurface::RawLine

#if 0
void
ogSurface::RawLine(uInt32 x1, uInt32 y1, uInt32 x2, uInt32 y2, uInt32 colour) {
  /*
   *  ogSurface::RawLine()
   *
   *  private method; draws an unclipped line from (x1,y1) to (x2,y2)
   *
   */
  int32 tc;
  if (!ogAvail()) return;
  switch (BPP) {
    case 8:
    __asm__ __volatile__(
      "  mov  $1, %%ecx     \n"  // mov     ecx, 1
      "  bt   $15, %%eax    \n"// bt      eax, 15
      "   jnc rlxPositive8  \n"
      "  or   $-1, %%ecx    \n"// or      ecx, -1
      "  neg  %%eax         \n"// neg     eax
      "rlxPositive8:                \n"
      "  add  %%eax, %%eax  \n"// add     eax, eax
      "  bt   $15, %%ebx    \n"// bt      ebx, 15
      "   jnc rlyPositive8  \n"
      "  neg  %%edx         \n"// neg     edx
      "  neg  %%ebx         \n"// neg     ebx
      "rlyPositive8:                \n"
      "  add  %%ebx, %%ebx  \n"// add     ebx, ebx

      "  cmp  %%ebx, %%eax  \n"// cmp     eax, ebx
      "   jle rlyGreater8   \n"
      "  push %%ecx         \n"// push    ecx
      "  mov  %%eax, %%ecx  \n"// mov     ecx, eax
      "  mov  %%ebx, %6     \n"// mov     tc, ebx
      "  shr  $1, %%ecx     \n"// shr     ecx, 1
      "  sub  %%ecx, %6     \n"// sub     tc, ecx
      "  pop  %%ecx         \n"// pop     ecx
      "rlxTop8:                     \n"
      "  push %%eax         \n"// push    eax
      "  mov  %5, %%eax     \n"// mov     eax, colour
      "  mov  %%al, (%%edi) \n"// mov     [edi], al
      "  pop  %%eax         \n"// pop     eax
      "  cmp  %%edi, %%esi  \n"// cmp     esi, edi
      "   je  rlDone8       \n"
      "  cmp  $0, %6        \n"// cmp     tc, 0
      "   jl  rlNoAddY8     \n"
      "  add  %%edx, %%edi  \n"// add     edi, edx
      "  sub  %%eax, %6     \n"// sub     tc, eax
      "rlNoAddY8:                   \n"
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ebx, %6     \n"// add     tc, ebx
      "   jmp rlxTop8       \n"

      "rlyGreater8:                 \n"
      "  push %%ecx         \n"// push    ecx
      "  mov  %%ebx, %%ecx  \n"// mov     ecx, ebx
      "  mov  %%eax, %6     \n"// mov     tc, eax
      "  shr  $1, %%ecx     \n"// shr     ecx, 1
      "  sub  %%ecx, %6     \n"// sub     tc, ecx
      "  pop  %%ecx         \n"
      "rlyTop8:                     \n"
      "  push %%eax         \n"// push    eax
      "  mov  %5, %%eax     \n"// mov     eax, colour
      "  mov  %%al, (%%edi) \n"// mov     [edi], al
      "  pop  %%eax         \n"// pop     eax
      "  cmp  %%edi, %%esi  \n"// cmp     esi, edi
      "   je  rlDone8       \n"
      "  cmp  $0, %6        \n"// cmp     tc, 0
      "   jl  rlNoAddX8     \n"
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  sub  %%ebx, %6     \n"// sub     tc, ebx
      "rlNoAddX8:                   \n"
      "  add  %%edx, %%edi  \n"// add     edi, edx
      "  add  %%eax, %6     \n"// add     tc, eax
      "   jmp rlyTop8       \n"
      "rlDone8:                     \n"
      :
      : "D" ((uInt8 *)buffer+lineOfs[y1]+x1),// %0
      "S" ((uInt8 *)buffer+lineOfs[y2]+x2),// %1
      "a" (x2-x1), "b" (y2-y1),// %2, %3
      "d" (xRes), "m" (colour),// %4, %5
      "m" (tc)// %6
    );
    break;
    case 15:
    case 16:
    __asm__ __volatile__(
      "  mov  $1, %%ecx     \n"// mov     ecx, 1
      "  bt   $15, %%eax    \n"// bt      eax, 15
      "   jnc rlxPositive16 \n"
      "  or   $-1, %%ecx    \n"// or      ecx, -1
      "  neg  %%eax         \n"// neg     eax
      "rlxPositive16:               \n"
      "  add  %%eax, %%eax  \n"// add     eax, eax
      "  bt   $15, %%ebx    \n"// bt      ebx, 15
      "   jnc rlyPositive16 \n"
      "  neg  %%edx         \n"// neg     edx
      "  neg  %%ebx         \n"// neg     ebx
      "rlyPositive16:               \n"
      "  add  %%ebx, %%ebx  \n"// add     ebx, ebx

      "  cmp  %%ebx, %%eax  \n"// cmp     eax, ebx
      "   jle rlyGreater16  \n"
      "  push %%ecx         \n"// push    ecx
      "  mov  %%eax, %%ecx  \n"// mov     ecx, eax
      "  mov  %%ebx, %6     \n"// mov     tc, ebx
      "  shr  $1, %%ecx     \n"// shr     ecx, 1
      "  sub  %%ecx, %6     \n"// sub     tc, ecx
      "  pop  %%ecx         \n"// pop     ecx
      "rlxTop16:                    \n"
      "  push %%eax         \n"// push    eax
      "  mov  %5, %%eax     \n"// mov     eax, colour
      "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
      "  pop  %%eax         \n"// pop     eax
      "  cmp  %%edi, %%esi  \n"// cmp     esi, edi
      "   je  rlDone16      \n"
      "  cmp  $0, %6        \n"// cmp     tc, 0
      "   jl  rlNoAddY16    \n"
      "  add  %%edx, %%edi  \n"// add     edi, edx
      "  sub  %%eax, %6     \n"// sub     tc, eax
      "rlNoAddY16:                  \n"
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ecx, %%edi  \n"// add     edi, ecx  - pix size
      "  add  %%ebx, %6     \n"// add     tc, ebx
      "   jmp rlxTop16      \n"

      "rlyGreater16:                \n"
      "  push %%ecx         \n"// push    ecx
      "  mov  %%ebx, %%ecx  \n"// mov     ecx, ebx
      "  mov  %%eax, %6     \n"// mov     tc, eax
      "  shr  $1, %%ecx     \n"// shr     ecx, 1
      "  sub  %%ecx, %6     \n"// sub     tc, ecx
      "  pop  %%ecx         \n"
      "rlyTop16:                    \n"
      "  push %%eax         \n"// push    eax
      "  mov  %5, %%eax     \n"// mov     eax, colour
      "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
      "  pop  %%eax         \n"// pop     eax
      "  cmp  %%edi, %%esi  \n"// cmp     esi, edi
      "   je  rlDone16      \n"
      "  cmp  $0, %6        \n"// cmp     tc, 0
      "   jl  rlNoAddX16    \n"
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ecx, %%edi  \n"// add     edi, ecx  - pix size
      "  sub  %%ebx, %6     \n"// sub     tc, ebx
      "rlNoAddX16:                  \n"
      "  add  %%edx, %%edi  \n"// add     edi, edx
      "  add  %%eax, %6     \n"// add     tc, eax
      "   jmp rlyTop16      \n"
      "rlDone16:                    \n"
      :
      : "D" ((uInt8 *)buffer+lineOfs[y1]+(x1 << 1)),// %0
      "S" ((uInt8 *)buffer+lineOfs[y2]+(x2 << 1)),// %1
      "a" (x2-x1), "b" (y2-y1),// %2, %3
      "d" (xRes), "m" (colour),// %4, %5
      "m" (tc)// %6
    );
    break;
    case 24:
    __asm__ __volatile__(
      "  mov  $1, %%ecx     \n"// mov     ecx, 1
      "  bt   $15, %%eax    \n"// bt      eax, 15
      "   jnc rlxPositive24 \n"
      "  or   $-1, %%ecx    \n"// or      ecx, -1
      "  neg  %%eax         \n"// neg     eax
      "rlxPositive24:               \n"
      "  add  %%eax, %%eax  \n"// add     eax, eax
      "  bt   $15, %%ebx    \n"// bt      ebx, 15
      "   jnc rlyPositive24 \n"
      "  neg  %%edx         \n"// neg     edx
      "  neg  %%ebx         \n"// neg     ebx
      "rlyPositive24:               \n"
      "  add  %%ebx, %%ebx  \n"// add     ebx, ebx

      "  cmp  %%ebx, %%eax  \n"// cmp     eax, ebx
      "   jle rlyGreater24  \n"
      "  push %%ecx         \n"// push    ecx
      "  mov  %%eax, %%ecx  \n"// mov     ecx, eax
      "  mov  %%ebx, %6     \n"// mov     tc, ebx
      "  shr  $1, %%ecx     \n"// shr     ecx, 1
      "  sub  %%ecx, %6     \n"// sub     tc, ecx
      "  pop  %%ecx         \n"// pop     ecx
      "rlxTop24:                    \n"
      "  push %%eax         \n"// push    eax
      "  mov  %5, %%eax     \n"// mov     eax, colour
      "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
      "  shr  $16, %%eax    \n"// shr     eax, 16
      "  mov  %%al, 2(%%edi)\n"// mov     [edi+2],al
      "  pop  %%eax         \n"// pop     eax
      "  cmp  %%edi, %%esi  \n"// cmp     esi, edi
      "   je  rlDone24      \n"
      "  cmp  $0, %6        \n"// cmp     tc, 0
      "   jl  rlNoAddY24    \n"
      "  add  %%edx, %%edi  \n"// add     edi, edx
      "  sub  %%eax, %6     \n"// sub     tc, eax
      "rlNoAddY24:                  \n"
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ecx, %%edi  \n"// add     edi, ecx  - pix size
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ebx, %6     \n"// add     tc, ebx
      "   jmp rlxTop24      \n"

      "rlyGreater24:                \n"
      "  push %%ecx         \n"// push    ecx
      "  mov  %%ebx, %%ecx  \n"// mov     ecx, ebx
      "  mov  %%eax, %6     \n"// mov     tc, eax
      "  shr  $1, %%ecx     \n"// shr     ecx, 1
      "  sub  %%ecx, %6     \n"// sub     tc, ecx
      "  pop  %%ecx         \n"
      "rlyTop24:                    \n"
      "  push %%eax         \n"// push    eax
      "  mov  %5, %%eax     \n"// mov     eax, colour
      "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
      "  shr  $16, %%eax    \n"// shr     eax, 16
      "  mov  %%al, 2(%%edi)\n"// mov     [edi+2],al
      "  pop  %%eax         \n"// pop     eax
      "  cmp  %%edi, %%esi  \n"// cmp     esi, edi
      "   je  rlDone24      \n"
      "  cmp  $0, %6        \n"// cmp     tc, 0
      "   jl  rlNoAddX24    \n"
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ecx, %%edi  \n"// add     edi, ecx  - pix size
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  sub  %%ebx, %6     \n"// sub     tc, ebx
      "rlNoAddX24:                  \n"
      "  add  %%edx, %%edi  \n"// add     edi, edx
      "  add  %%eax, %6     \n"// add     tc, eax
      "   jmp rlyTop24      \n"
      "rlDone24:                    \n"
      :
      : "D" ((uInt8 *)buffer+lineOfs[y1]+(x1*3)),// %0
      "S" ((uInt8 *)buffer+lineOfs[y2]+(x2*3)),// %1
      "a" (x2-x1), "b" (y2-y1),// %2, %3
      "d" (xRes), "m" (colour),// %4, %5
      "m" (tc)// %6
    );
    break;
    case 32:
    __asm__ __volatile__(
      "  mov  $1, %%ecx     \n"// mov     ecx, 1
      "  bt   $15, %%eax    \n"// bt      eax, 15
      "   jnc rlxPositive32 \n"
      "  or   $-1, %%ecx    \n"// or      ecx, -1
      "  neg  %%eax         \n"// neg     eax
      "rlxPositive32:               \n"
      "  add  %%eax, %%eax  \n"// add     eax, eax
      "  bt   $15, %%ebx    \n"// bt      ebx, 15
      "   jnc rlyPositive32 \n"
      "  neg  %%edx         \n"// neg     edx
      "  neg  %%ebx         \n"// neg     ebx
      "rlyPositive32:               \n"
      "  add  %%ebx, %%ebx  \n"// add     ebx, ebx

      "  cmp  %%ebx, %%eax  \n"// cmp     eax, ebx
      "   jle rlyGreater32  \n"
      "  push %%ecx         \n"// push    ecx
      "  mov  %%eax, %%ecx  \n"// mov     ecx, eax
      "  mov  %%ebx, %6     \n"// mov     tc, ebx
      "  shr  $1, %%ecx     \n"// shr     ecx, 1
      "  sub  %%ecx, %6     \n"// sub     tc, ecx
      "  pop  %%ecx         \n"// pop     ecx
      "rlxTop32:                    \n"
      "  push %%eax         \n"// push    eax
      "  mov  %5, %%eax     \n"// mov     eax, colour
      "  mov  %%eax, (%%edi)\n"// mov     [edi], eax
      "  pop  %%eax         \n"// pop     eax
      "  cmp  %%edi, %%esi  \n"// cmp     esi, edi
      "   je  rlDone32      \n"
      "  cmp  $0, %6        \n"// cmp     tc, 0
      "   jl  rlNoAddY32    \n"
      "  add  %%edx, %%edi  \n"// add     edi, edx
      "  sub  %%eax, %6     \n"// sub     tc, eax
      "rlNoAddY32:                  \n"
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ecx, %%edi  \n"// add     edi, ecx  - pix size
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ecx, %%edi  \n"// add     edi, ecx  - pix size
      "  add  %%ebx, %6     \n"// add     tc, ebx
      "   jmp rlxTop32      \n"

      "rlyGreater32:                \n"
      "  push %%ecx         \n"// push    ecx
      "  mov  %%ebx, %%ecx  \n"// mov     ecx, ebx
      "  mov  %%eax, %6     \n"// mov     tc, eax
      "  shr  $1, %%ecx     \n"// shr     ecx, 1
      "  sub  %%ecx, %6     \n"// sub     tc, ecx
      "  pop  %%ecx         \n"
      "rlyTop32:                    \n"
      "  push %%eax         \n"// push    eax
      "  mov  %5, %%eax     \n"// mov     eax, colour
      "  mov  %%eax, (%%edi)\n"// mov     [edi], eax
      "  pop  %%eax         \n"// pop     eax
      "  cmp  %%edi, %%esi  \n"// cmp     esi, edi
      "   je  rlDone32      \n"
      "  cmp  $0, %6        \n"// cmp     tc, 0
      "   jl  rlNoAddX32    \n"
      "  add  %%ecx, %%edi  \n"// add     edi, ecx  - pix size
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  add  %%ecx, %%edi  \n"// add     edi, ecx  - pix size
      "  add  %%ecx, %%edi  \n"// add     edi, ecx
      "  sub  %%ebx, %6     \n"// sub     tc, ebx
      "rlNoAddX32:                  \n"
      "  add  %%edx, %%edi  \n"// add     edi, edx
      "  add  %%eax, %6     \n"// add     tc, eax
      "   jmp rlyTop32      \n"
      "rlDone32:                    \n"
      :
      : "D" ((uInt8 *)buffer+lineOfs[y1]+(x1 << 2)),// %0
      "S" ((uInt8 *)buffer+lineOfs[y2]+(x2 << 2)),// %1
      "a" (x2-x1), "b" (y2-y1),// %2, %3
      "d" (xRes), "m" (colour),// %4, %5
      "m" (tc)// %6
    );
    break;
  } // switch
  return;
} // ogSurface::RawLine
#endif

bool ogSurface::ogAlias(ogSurface& src, uInt32 x1, uInt32 y1, uInt32 x2, uInt32 y2) {
  uInt32 tmp;

  if (dataState == ogOwner) {
    ogSetLastError(ogAlreadyOwner);
    return false;
  } // if

  if (x2 < x1) {
    tmp = x2;
    x2 = x1;
    x1 = tmp;
  } // if

  if (y2 < y1) {
    tmp = y2;
    y2 = y1;
    y1 = tmp;
  } // if

  maxX = (x2 - x1);
  maxY = (y2 - y1);

  dataState = ogAliasing;

  bSize = 0;
  lSize = 0;

  owner = &src;
  buffer = ((unsigned char *) (src.buffer) + x1 * (src.bytesPerPix));
  lineOfs = ((uInt32 *) src.lineOfs) + y1;
  attributes = src.attributes;

  pal = src.pal;
  xRes = src.xRes;
  yRes = src.yRes;
  BPP = src.BPP;
  bytesPerPix = src.bytesPerPix;
  pixFmtID = src.pixFmtID;

  // For 8bpp modes the next part doesn't matter
  redFieldPosition = src.redFieldPosition;
  greenFieldPosition = src.greenFieldPosition;
  blueFieldPosition = src.blueFieldPosition;
  alphaFieldPosition = src.alphaFieldPosition;
  // The next part is only used by 15/16bpp
  redShifter = src.redShifter;
  greenShifter = src.greenShifter;
  blueShifter = src.blueShifter;
  alphaShifter = src.alphaShifter;

  alphaMasker = src.alphaMasker;

  return true;
} // ogSurface::ogAlias

void ogSurface::ogArc(int32 xCenter, int32 yCenter, uInt32 radius, uInt32 sAngle, uInt32 eAngle, uInt32 colour) {
  int32 p;
  uInt32 x, y, tmp;
  double alpha;

  if (radius == 0) {
    ogSetPixel(xCenter, yCenter, colour);
    return;
  } // if

  sAngle %= 361;
  eAngle %= 361;

  if (sAngle > eAngle) {
    tmp = sAngle;
    sAngle = eAngle;
    eAngle = tmp;
  } // if

  x = 0;
  y = radius;
  p = 3 - 2 * radius;

  while (x <= y) {
    alpha = (180.0 / 3.14159265358979) * atan((double) x / (double) y);

    if ((alpha >= sAngle) && (alpha <= eAngle))
      ogSetPixel(xCenter - x, yCenter - y, colour);

    if ((90 - alpha >= sAngle) && (90 - alpha <= eAngle))
      ogSetPixel(xCenter - y, yCenter - x, colour);

    if ((90 + alpha >= sAngle) && (90 + alpha <= eAngle))
      ogSetPixel(xCenter - y, yCenter + x, colour);

    if ((180 - alpha >= sAngle) && (180 - alpha <= eAngle))
      ogSetPixel(xCenter - x, yCenter + y, colour);

    if ((180 + alpha >= sAngle) && (180 + alpha <= eAngle))
      ogSetPixel(xCenter + x, yCenter + y, colour);

    if ((270 - alpha >= sAngle) && (270 - alpha <= eAngle))
      ogSetPixel(xCenter + y, yCenter + x, colour);

    if ((270 + alpha >= sAngle) && (270 + alpha <= eAngle))
      ogSetPixel(xCenter + y, yCenter - x, colour);

    if ((360 - alpha >= sAngle) && (360 - alpha <= eAngle))
      ogSetPixel(xCenter + x, yCenter - y, colour);

    if (p < 0)
      p += 4 * x + 6;
    else {
      p += 4 * (x - y) + 10;
      --y;
    } // else
    ++x;
  } // while
  return;
} // ogSurface::ogArc

bool ogSurface::ogAvail(void) {
  return ((buffer != NULL) && (lineOfs != NULL));
} // ogSurface::ogAvail

static int32 calculate(float mu, int32 p0, int32 p1, int32 p2, int32 p3) {
  float mu2, mu3;
  mu2 = mu * mu;
  mu3 = mu2 * mu;
  return (int32) (0.5f + (1.0 / 6.0) * (mu3 * (-p0 + 3.0 * p1 - 3.0 * p2 + p3) + mu2 * (3.0 * p0 - 6.0 * p1 + 3.0 * p2) + mu * (-3.0 * p0 + 3.0 * p2) + (p0 + 4.0 * p1 + p2)));
} // calculate

void ogSurface::ogBSpline(uInt32 numPoints, ogPoint2d* points, uInt32 segments, uInt32 colour) {
  float mu, mudelta;
  int32 x1, y1, x2, y2;
  uInt32 n, h;

  if (points == NULL)
    return;

  if ((numPoints < 4) || (numPoints > 255) || (segments == 0))
    return;

  mudelta = 1.0 / segments;
  for (n = 3; n < numPoints; n++) {
    mu = 0.0;
    x1 = calculate(mu, points[n - 3].x, points[n - 2].x, points[n - 1].x, points[n].x);
    y1 = calculate(mu, points[n - 3].y, points[n - 2].y, points[n - 1].y, points[n].y);
    mu += mudelta;

    for (h = 0; h < segments; h++) {
      x2 = calculate(mu, points[n - 3].x, points[n - 2].x, points[n - 1].x, points[n].x);
      y2 = calculate(mu, points[n - 3].y, points[n - 2].y, points[n - 1].y, points[n].y);
      ogLine(x1, y1, x2, y2, colour);
      mu += mudelta;
      x1 = x2;
      y1 = y2;
    } // for h

  } // for n
  return;
} // ogSurface::ogBSpline

void ogSurface::ogCircle(int32 xCenter, int32 yCenter, uInt32 radius, uInt32 colour) {
  int32 x, y, d;

  x = 0;
  y = radius;
  d = 2 * (1 - radius);
  while (y >= 0) {
    ogSetPixel(xCenter + x, yCenter + y, colour);
    ogSetPixel(xCenter + x, yCenter - y, colour);
    ogSetPixel(xCenter - x, yCenter + y, colour);
    ogSetPixel(xCenter - x, yCenter - y, colour);

    if (d + y > 0) {
      --y;
      d -= 2 * y + 1;
    } // if

    if (x > d) {
      ++x;
      d += 2 * x + 1;
    } // if
  } // while
  return;
} // ogSurface::ogCircle

void ogSurface::ogClear(uInt32 colour) {
  uInt32 height = 0;
  uInt32 xx, yy;
  uInt8 r, g, b, a;
  if (!ogAvail())
    return;

  do {
    if (ogIsBlending()) {
      ogUnpack(colour, r, g, b, a);
      if (a == 0)
        return;
      if (a == 255)
        break;
      for (yy = 0; yy <= maxY; yy++)
        for (xx = 0; xx <= maxX; xx++)
          RawSetPixel(xx, yy, r, g, b, a);
      return;
    } // if
  } while (false);

  __asm__ __volatile__("cld\n");
  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(

        "  add (%%esi), %%edi  \n"      // add edi, [esi]
        "  mov %%ecx, %%esi    \n"// mov esi, ecx
        "  inc %%edx           \n"// inc edx (maxX)
        "  inc %%ebx           \n"// inc ebx (maxY)
        "  mov %%edx, %%ecx    \n"// mov ecx, edx
        "  shl $2, %%ecx       \n"// shl ecx, 2
        "  sub %%ecx, %%esi    \n"// sub esi, ecx // adjust for pix size
        "loop32:                   \n"
        "  mov %%edx, %%ecx    \n"// mov ecx, edx
        " rep                  \n"
        " stosl                \n"
        "  add %%esi, %%edi    \n"// add edi, esi
        "  dec %%ebx           \n"
        "   jnz loop32         \n"

        :
        : "D" (buffer), "S" (lineOfs),// %0, %1
        "a" (colour), "b" (maxY),// %2, %3
        "c" (xRes), "d" (maxX)// %4, %5
      );
    break;
    case 3:
      __asm__ __volatile__(
        "  add (%%esi), %%edi  \n"      // add edi, [esi]
        "  mov %%ecx, %%esi    \n"// mov esi, ecx
        "  inc %%edx           \n"// inc edx (maxX)
        "  inc %%ebx           \n"// inc ebx (maxY)
        "  sub %%edx, %%esi    \n"// sub esi, edx // adjust for pix size
        "  mov %%ebx, %6       \n"// mov height, ebx
        "  sub %%edx, %%esi    \n"// sub esi, edx // adjust for pix size
        "  mov %%eax, %%ebx    \n"// mov ebx, eax
        "  sub %%edx, %%esi    \n"// sub esi, edx // adjust for pix size
        "  shr $16, %%ebx      \n"// shr ebx, 16
        "oloop24:                  \n"
        "  mov %%edx, %%ecx    \n"// mov ecx, edx
        "iloop24:                  \n"
        "  mov %%ax,(%%edi)    \n"// mov [edi],ax
        "  movb %%bl,2(%%edi)  \n"// mov [edi+2],bl
        "  add $3, %%edi       \n"// add edi, 3
        "  dec %%ecx           \n"// dec ecx
        "   jnz iloop24        \n"
        "  add %%esi, %%edi    \n"// add edi, esi
        "  decl %6             \n"// dec height
        "   jnz oloop24        \n"
        :
        : "D" (buffer), "S" (lineOfs),// %0, %1
        "a" (colour), "b" (maxY),// %2, %3
        "c" (xRes), "d" (maxX),// %4, %5
        "m" (height)// %6
      );
    break;
    case 2:
      __asm__ __volatile__(
        "  add (%%esi), %%edi  \n"      // add edi, [esi]
        "  mov %%ecx, %%esi    \n"// mov esi, ecx
        "  inc %%edx           \n"// inc edx (maxX)
        "  inc %%ebx           \n"// inc ebx (maxY)
        "  sub %%edx, %%esi    \n"// sub esi, edx
        "  sub %%edx, %%esi    \n"// sub esi, edx // adjust for pix size
        "  mov %%ax, %%cx      \n"// mov cx, ax
        "  shl $16, %%eax      \n"// shl eax, 16
        "  mov %%cx, %%ax      \n"// mov ax, cx
        "loop16:                   \n"
        "  mov %%edx, %%ecx    \n"// mov ecx, edx
        "  shr $1, %%ecx       \n"// shr ecx, 1
        " rep                  \n"
        " stosl                \n"
        "  jnc noc16           \n"
        " stosw                \n"
        "noc16:                    \n"
        "  add %%esi, %%edi    \n"// add edi, esi
        "  dec %%ebx           \n"
        "   jnz loop16         \n"
        :
        : "D" (buffer), "S" (lineOfs),// %0, %1
        "a" (colour), "b" (maxY),// %2, %3
        "c" (xRes), "d" (maxX)// %4, %5
      );
    break;
    case 1:
      __asm__ __volatile__(
        "  add (%%esi), %%edi  \n"      // add edi, [esi]
        "  mov %%ecx, %%esi    \n"// mov esi, ecx
        "  inc %%edx           \n"// inc edx (maxY)
        "  inc %%ebx           \n"// inc ebx (maxX)
        "  sub %%edx, %%esi    \n"// sub esi, edx
        "  mov %%al, %%ah      \n"// mov ah, al
        "  mov %%ax, %%cx      \n"// mov cx, ax
        "  shl $16, %%eax      \n"// shl eax, 16
        "  mov %%cx, %%ax      \n"// mov ax, cx
        "loop8:                    \n"
        "  push %%edx          \n"
        "  mov %%edx, %%ecx    \n"// mov ecx, edx
        "  and $3, %%edx       \n"// and edx, 3
        "  shr $2, %%ecx       \n"// shr ecx, 2
        " rep                  \n"
        " stosl                \n"
        "  mov %%edx, %%ecx    \n"// mov ecx, edx
        " rep                  \n"
        " stosb                \n"
        "  pop %%edx           \n"
        "  add %%esi, %%edi    \n"// add edi, esi
        "  dec %%ebx           \n"
        "   jnz loop8          \n"
        :
        : "D" (buffer), "S" (lineOfs),// %0, %1
        "a" (colour), "b" (maxY),// %2, %3
        "c" (xRes), "d" (maxX)// %4, %5
      );
    break;
  } // switch
  return;
} // ogSurface::ogClear

bool ogSurface::ogClone(ogSurface& src) {
  bool created;
  ogPixelFmt pixFmt;

  if (src.dataState == ogNone) {
    ogSetLastError(ogNoSurface);
    return false;
  } // if

  src.ogGetPixFmt(pixFmt);
  created = ogCreate(src.maxX + 1, src.maxY + 1, pixFmt);
  if (!created)
    return false;

  *attributes = *src.attributes;

  ogCopyPalette(src);
  ogCopy(src);
  return true;
} // ogSurface::ogClone

void ogSurface::ogCopy(ogSurface& src) {
  uInt32 pixMap[256];
  uInt32 count, xCount, yCount;
  uInt32 xx, yy;
  uInt8 r, g, b, a;
  void * srcPtr;

  if (!ogAvail())
    return;
  if (!src.ogAvail())
    return;

  xCount = src.maxX + 1;
  if (xCount > maxX + 1)
    xCount = maxX + 1;
  yCount = src.maxY + 1;
  if (yCount > maxY + 1)
    yCount = maxY + 1;

  if (ogIsBlending()) {

    for (yy = 0; yy < yCount; yy++)
      for (xx = 0; xx < xCount; xx++) {
        src.ogUnpack(src.RawGetPixel(xx, yy), r, g, b, a);
        RawSetPixel(xx, yy, r, g, b, a);
      } // for xx

    return;
  } // if blending

  if (pixFmtID != src.pixFmtID) {
    if (src.bytesPerPix == 1) {
      for (xx = 0; xx < 256; xx++)
        pixMap[xx] = ogPack(src.pal[xx].red, src.pal[xx].green, src.pal[xx].blue, src.pal[xx].alpha);

      for (yy = 0; yy < yCount; yy++)
        for (xx = 0; xx < xCount; xx++)
          RawSetPixel(xx, yy, pixMap[src.RawGetPixel(xx, yy)]);

    }
    else {  // if src.bytesPerPix == 1
      ogPixelFmt srcPixFmt, dstPixFmt;
      src.ogGetPixFmt(srcPixFmt);
      ogGetPixFmt(dstPixFmt);
      ogPixCon * pc = new ogPixCon(srcPixFmt, dstPixFmt);

      for (yy = 0; yy < yCount; yy++)
        for (xx = 0; xx < xCount; xx++)
          RawSetPixel(xx, yy, pc->ConvPix(src.RawGetPixel(xx, yy)));

      delete pc;
    } // else
  }
  else {

    xCount *= bytesPerPix;

    for (count = 0; count < yCount; count++)
      if ((srcPtr = src.ogGetPtr(0, count)) == NULL) {
        /*
         * if we are here then we couldn't get a direct memory pointer
         * from the source object.  This means that it is not a normal
         * "memory" buffer and we have to use the implementation inspecific
         * interface.  We let the source buffer fill a "temporary" buffer
         * and then we copy it to where it needs to go.
         */
#ifdef __UBIXOS_KERNEL__
        srcPtr = kmalloc(xCount);  // allocate space
#else
        srcPtr = malloc(xCount);  // allocate space
#endif
        if (srcPtr != NULL) {
          src.ogCopyLineFrom(0, count, srcPtr, xCount);
          ogCopyLineTo(0, count, srcPtr, xCount);
#ifdef __UBIXOS_KERNEL__
          kfree(srcPtr);
#else
          free(srcPtr);
#endif
        } // if srcPtr!=NULL
      }
      else
        ogCopyLineTo(0, count, srcPtr, xCount);
  } // else
} // ogSurface::ogCopy

void ogSurface::ogCopyBuf(int32 dX1, int32 dY1, ogSurface& src, int32 sX1, int32 sY1, int32 sX2, int32 sY2) {
  uInt32 pixMap[256];
  int32 xx, yy, count, xCount, yCount;
  uInt8 r, g, b, a;
  void *srcPtr;
  ogPixCon * pc;
  ogPixelFmt srcPixFmt, dstPixFmt;

  if (!ogAvail())
    return;
  if (!src.ogAvail())
    return;

  if ((dX1 > (int32) maxX) || (dY1 > (int32) maxY))
    return;

  // if any of the source buffer is out of bounds then do nothing
  if (((uInt32) sX1 > src.maxX) || ((uInt32) sX2 > src.maxX) || ((uInt32) sY1 > src.maxY) || ((uInt32) sY2 > src.maxY))
    return;

  if (sX1 > sX2) {
    xx = sX1;
    sX1 = sX2;
    sX2 = xx;
  } // if

  if (sY1 > sY2) {
    yy = sY1;
    sY1 = sY2;
    sY2 = yy;
  } // if

  xCount = abs(sX2 - sX1) + 1;
  yCount = abs(sY2 - sY1) + 1;

  if (dX1 + xCount > (int32) maxX + 1)
    xCount = maxX - dX1 + 1;
  if (dY1 + yCount > (int32) maxY + 1)
    yCount = maxY - dY1 + 1;

  if (dX1 < 0) {
    xCount += dX1;
    sX1 -= dX1;
    dX1 = 0;
  } // if

  if (dY1 < 0) {
    yCount += dY1;
    sY1 -= dY1;
    dY1 = 0;
  } // if

  if ((dX1 + xCount < 0) || (dY1 + yCount < 0))
    return;

  if (ogIsBlending()) {
    for (yy = 0; yy < yCount; yy++)
      for (xx = 0; xx < xCount; xx++) {
        src.ogUnpack(src.RawGetPixel(sX1 + xx, sY1 + yy), r, g, b, a);
        RawSetPixel(dX1 + xx, dY1 + yy, r, g, b, a);
      } // for xx
  } // if IsBlending

  if (pixFmtID != src.pixFmtID) {

    if (src.bytesPerPix == 1) {
      for (xx = 0; xx < 256; xx++)
        pixMap[xx] = ogPack(src.pal[xx].red, src.pal[xx].green, src.pal[xx].blue, src.pal[xx].alpha);

      for (yy = 0; yy < yCount; yy++)
        for (xx = 0; xx < xCount; xx++)
          RawSetPixel(dX1 + xx, dY1 + yy, pixMap[src.ogGetPixel(sX1 + xx, sY1 + yy)]);
    }
    else {

      src.ogGetPixFmt(srcPixFmt);
      ogGetPixFmt(dstPixFmt);
      pc = new ogPixCon(srcPixFmt, dstPixFmt);  // allocate the pixel converter
      if (pc == NULL)
        return;

      for (yy = 0; yy < yCount; yy++)
        for (xx = 0; xx < xCount; xx++) {
          RawSetPixel(dX1 + xx, dY1 + yy, pc->ConvPix(src.RawGetPixel(sX1 + xx, sY1 + yy)));
        } // for xx

      delete pc; // destroy the pixel converter

    } // else
  }
  else {
    xCount *= bytesPerPix;

    for (count = 0; count < yCount; count++)
      if ((srcPtr = src.ogGetPtr(sX1, sY1 + count)) == NULL) {
        // if we are here then we couldn't get a direct memory pointer
        // from the source object.  This means that it is not a normal
        // "memory" buffer and we have to use the implementation inspecific
        // interface.  We let the source buffer fill a "temporary" buffer
        // and then we copy it to where it needs to go.

#ifdef __UBIXOS_KERNEL__
        srcPtr = kmalloc(xCount);  // allocate space
#else
        srcPtr = malloc(xCount);  // allocate space
#endif
        if (srcPtr != NULL) {
          src.ogCopyLineFrom(sX1, sY1 + count, srcPtr, xCount);
          ogCopyLineTo(dX1, dY1 + count, srcPtr, xCount);
#ifdef __UBIXOS_KERNEL__
          kfree(srcPtr);
#else
          free(srcPtr);
#endif
        } // if srcPtr!=NULL
      }
      else
        ogCopyLineTo(dX1, dY1 + count, srcPtr, xCount);
  } // else
} // ogSurface::ogCopyBuf

void ogSurface::ogCopyLineTo(uInt32 dx, uInt32 dy, const void * src, uInt32 size) {
  /*
   * CopyLineTo()
   *
   * Inputs:
   *
   * dx   - Destination X of the target buffer
   * dy   - Destination Y of the target buffer
   * src  - buffer to copy
   * size - size in bytes *NOT* pixels
   *
   * Copies a run of pixels (of the same format) to (x,y) of a buffer
   *
   * This method is required because of the different implementations of
   * copying a run of pixels to a buffer
   *
   * WARNING!!! This does *NO* error checking. It is assumed that you've
   * done all of that.  CopyLineTo and CopyLineFrom are the only
   * methods that don't check to make sure you're hosing things.  Don't
   * use this method unless YOU KNOW WHAT YOU'RE DOING!!!!!!!!!
   */

  memcpy((uint8_t *) buffer + lineOfs[dy] + (dx * bytesPerPix), // dest
  src,                                                // src
    size);                                              // size

  return;
} // ogSurface::ogCopyLineTo

void ogSurface::ogCopyLineFrom(uInt32 sx, uInt32 sy, void * dst, uInt32 size) {
  /*
   * CopyLineFrom()
   *
   * Inputs:
   *
   * sx   - Source X of the target buffer
   * sy   - Source Y of the target buffer
   * dest - where to put it
   * size - size in bytes *NOT* pixels
   *
   * Copies a run of pixels (of the same format) to (x,y) of a buffer
   *
   * This method is required because of the different implementations of
   * copying a run of pixels to a buffer
   *
   * WARNING!!! This does *NO* error checking. It is assumed that you've
   * done all of that.  CopyLineTo and CopyLineFrom are the only
   * methods that don't check to make sure you're hosing things.  Don't
   * use this method unless YOU KNOW WHAT YOU'RE DOING!!!!!!!!!
   */

  memcpy(dst,                                               // dest
    (uInt8*) buffer + lineOfs[sy] + sx * bytesPerPix,         // src
    size);                                             // size

  return;
} // ogSurface::ogCopyLineFrom

void ogSurface::ogCopyPalette(ogSurface& src) {
  if (src.pal == NULL)
    return;
  if (pal == NULL)
    pal = new ogRGBA8[256];
  if (pal == NULL)
    return;
  src.ogGetPalette(pal);
  // memcpy(pal, src.pal, sizeof(ogRGBA8)*256);
  return;
} // ogSurface::ogCopyPalette

bool ogSurface::ogCreate(uInt32 _xRes, uInt32 _yRes, ogPixelFmt _pixFormat) {
  /*
   *  ogSurface::ogCreate()
   *  Allocates memory for a buffer of size _xRes by _yRes with
   *  the pixel format defined in _pixformat.  Allocates memory
   *  for pal and lineOfs.
   */
  void * newBuffer = NULL;
  uInt32 * newLineOfs = NULL;
  ogRGBA8 * newPal = NULL;
  ogAttributes* newAttributes = NULL;

  uInt32 newBSize;
  uInt32 newLSize;
  uInt32 yy;

  bool status = false;

  switch (_pixFormat.BPP) {
    case 8:
    case 15:
    case 16:
    case 24:
    case 32:
    break;
    default:
      ogSetLastError(ogBadBPP);
      return false;
  } // switch

  newBSize = _xRes * _yRes * ((_pixFormat.BPP + 7) >> 3);
  newLSize = _yRes * sizeof(uInt32);  // number of scan lines * sizeof(uInt32)

#ifdef __UBIXOS_KERNEL__
  newBuffer = kmalloc(newBSize);
#else
  newBuffer = malloc(newBSize);
#endif
  newLineOfs = new uInt32[_yRes];
  newPal = new ogRGBA8[256];
  newAttributes = new ogAttributes();

  do {

    if ((newBuffer == NULL) || (newLineOfs == NULL) || (newPal == NULL) || (newAttributes == NULL)) {
      ogSetLastError(ogMemAllocFail);
      break;                           // break out of do {...} while(false)
    } // if

    // check to see if we have already allocated memory .. if so, free it

    if (dataState == ogOwner) {
#ifdef __UBIXOS_KERNEL__
      kfree(buffer);
#else
      free(buffer);
#endif
      delete[] lineOfs;
      delete[] pal;
      delete attributes;
    }  // if dataState

    buffer = newBuffer;
    lineOfs = newLineOfs;
    pal = newPal;
    attributes = newAttributes;
    bSize = newBSize;
    lSize = newLSize;

    newBuffer = NULL;
    newLineOfs = NULL;
    newPal = NULL;
    newAttributes = NULL;

    BPP = _pixFormat.BPP;
    bytesPerPix = (BPP + 7) >> 3;

    ogSetPalette(DEFAULT_PALETTE);
    // memcpy(pal, DEFAULT_PALETTE, sizeof(ogRGBA8)*256);

    maxX = _xRes - 1;
    xRes = _xRes * bytesPerPix;
    maxY = _yRes - 1;
    yRes = _yRes;

    // in the pascal version we go from 1 to maxY .. here we use yy < yRes
    // (which is the same)

    lineOfs[0] = 0;
    for (yy = 1; yy < yRes; yy++)
      lineOfs[yy] = lineOfs[yy - 1] + xRes;

    dataState = ogOwner;

    // For 8bpp modes the next part doesn't matter

    redFieldPosition = _pixFormat.redFieldPosition;
    greenFieldPosition = _pixFormat.greenFieldPosition;
    blueFieldPosition = _pixFormat.blueFieldPosition;
    alphaFieldPosition = _pixFormat.alphaFieldPosition;
    // The next part is only used by 15/16hpp
    redShifter = 8 - _pixFormat.redMaskSize;
    greenShifter = 8 - _pixFormat.greenMaskSize;
    blueShifter = 8 - _pixFormat.blueMaskSize;
    alphaShifter = 8 - _pixFormat.alphaMaskSize;

    if (_pixFormat.alphaMaskSize != 0)
      alphaMasker = ~(OG_MASKS[_pixFormat.alphaMaskSize] << alphaFieldPosition);
    else
      alphaMasker = ~0;

    if (bytesPerPix == 1) {
      pixFmtID = 0x08080808;
      // turn anti aliasing off by default for 8bpp modes
      ogSetAntiAliasing(false);
    }
    else {
      pixFmtID = (redFieldPosition) | (greenFieldPosition << 8) | (blueFieldPosition << 16) | (alphaFieldPosition << 24);
      ogSetAntiAliasing(true);
    } // else

    ogClear(ogPack(0, 0, 0));

    owner = this;
    status = true;
  } while (false);

#ifdef __UBIXOS_KERNEL__
  if (newBuffer) kfree(newBuffer);
#else
  if (newBuffer)
    free(newBuffer);
#endif
  if (newLineOfs)
    delete[] newLineOfs;
  if (newPal)
    delete[] newPal;
  if (newAttributes)
    delete newAttributes;

  return status;
} // ogSurface::ogCreate

void ogSurface::ogCubicBezierCurve(int32 x1, int32 y1, int32 x2, int32 y2, int32 x3, int32 y3, int32 x4, int32 y4, uInt32 segments, uInt32 colour) {
  float tX1, tY1, tX2, tY2, tX3, tY3, mu, mu2, mu3, mudelta;
  int32 xStart, yStart, xEnd, yEnd;
  uInt32 n;
  if (segments < 1)
    return;
  if (segments > 128)
    segments = 128;

  mudelta = 1.0 / segments;
  mu = mudelta;
  tX1 = -x1 + 3 * x2 - 3 * x3 + x4;
  tY1 = -y1 + 3 * y2 - 3 * y3 + y4;
  tX2 = 3 * x1 - 6 * x2 + 3 * x3;
  tY2 = 3 * y1 - 6 * y2 + 3 * y3;
  tX3 = -3 * x1 + 3 * x2;
  tY3 = -3 * y1 + 3 * y2;

  xStart = x1;
  yStart = y1;

  for (n = 1; n < segments; n++) {
    mu2 = mu * mu;
    mu3 = mu2 * mu;
    xEnd = static_cast<int32>(mu3 * tX1 + mu2 * tX2 + mu * tX3 + x1 + 0.5f);
    yEnd = static_cast<int32>(mu3 * tY1 + mu2 * tY2 + mu * tY3 + y1 + 0.5f);
    ogLine(xStart, yStart, xEnd, yEnd, colour);
    mu += mudelta;
    xStart = xEnd;
    yStart = yEnd;
  } // for
  return;
} // ogSurface::ogCubicBezierCurve

void ogSurface::ogCurve(int32 x1, int32 y1, int32 x2, int32 y2, int32 x3, int32 y3, uInt32 segments, uInt32 colour) {
  int64 ex, ey, fx, fy;
  int64 t1, t2;

  if (segments < 2)
    segments = 2;
  else if (segments > 128)
    segments = 128;
  x2 = (x2 * 2) - ((x1 + x3) / 2);
  y2 = (y2 * 2) - ((y1 + y3) / 2);

  ex = ((int64) (x2 - x1) << 17) / segments;
  ey = ((int64) (y2 - y1) << 17) / (int64) segments;
  fx = ((int64) (x3 - (2 * x2) + x1) << 16) / (segments * segments);
  fy = ((int64) (y3 - (2 * y2) + y1) << 16) / (int64) (segments * segments);

  while (--segments > 0) {
    t1 = x3;
    t2 = y3;
    x3 = ((int64) ((fx * segments + ex) * segments) / 65536L) + x1;
    y3 = ((int64) ((fy * segments + ey) * segments) / 65536L) + y1;
    ogLine(t1, t2, x3, y3, colour);
  } // while
  ogLine(x3, y3, x1, y1, colour);
  return;

} // ogSurface::ogCurve

void ogSurface::ogFillCircle(int32 xCenter, int32 yCenter, uInt32 radius, uInt32 colour) {
  int32 x, y, d;
  x = 0;
  y = radius;
  d = 4 * (1 - radius);

  while (y >= 0) {
    if (d + y > 0) {
      ogHLine(xCenter - x, xCenter + x, yCenter - y, colour);
      if (y != 0)
        ogHLine(xCenter - x, xCenter + x, yCenter + y, colour);

      --y;
      d -= 4 * y + 1;
    } // if

    if (x > d) {
      ++x;
      d += 4 * x + 1;
    } // if
  } // while
  return;
} // ogSurface::ogFillCircle

#if 0
!-/* Scan converts an edge from (X1,Y1) to (X2,Y2), not including the
 !- * point at (X2,Y2). This avoids overlapping the end of one line with
 !- * the start of the next, and causes the bottom scan line of the
 !- * polygon not to be drawn. If SkipFirst != 0, the point at (X1,Y1)
 !- * isn't drawn. For each scan line, the pixel closest to the scanned
 !- * line without being to the left of the scanned line is chosen
 !- */
!-static void index_forward(int32 & index, uInt32 numPoints) {
  !- index = (index + 1) % numPoints;
  !- return;
  !-} // index_forward
!-
!-static void index_backward(int32 & index, uInt32 numPoints) {
  !- index = (index - 1 + numPoints) % numPoints;
  !- return;
  !-} // index_forward
!-
!-static void index_move(int32 & index, uInt32 numPoints, int32 direction) {
  !- if (direction > 0)
  !- index_forward(index, numPoints);
  !-
  else
  !- index_backward(index, numPoints);
  !- return;
  !-} // index_move
!-
!-static void scanEdge(int32 x1, int32 y1, int32 x2, int32 y2,
  !- uInt32 & eIdx, int32 * xList) {
  !- int32 y, deltaX, deltaY;
  !- float inverseSlope;
  !-
  !- deltaX = x2 - x1;
  !- deltaY = y2 - y1;
  !- if (deltaY <= 0) return;
  !- inverseSlope = deltaX / deltaY;
  !-
  !-  // Store the X coordinate of the pixel closest to but not to the
  !-// left of the line for each Y coordinate between Y1 and Y2, not
  !-// including Y2
  !- y = y1;
  !- do {
    !- xList[eIdx] = x1+ (int32)(0.5f+((y-y1)*inverseSlope));
    !- y++;
    !- eIdx++;
    !-}while (y<y2);
  !- return;
  !-} // scanEdge
!-
!-void
!-ogSurface::FillConvexPolygon(uInt32 numPoints, ogPoint2d* polyPoints, uInt32 colour) {
  !- int32 i, minIndexL, maxIndex, minIndexR, temp;
  !- int32 minPointY, maxPointY, leftEdgeDir;
  !- int32 topIsFlat, nextIndex, curIndex, prevIndex;
  !- int32 deltaXN, deltaYN, deltaXP, deltaYP;
  !- ogHLineList workingHLineList;
  !- uInt32 edgePointIdx;
  !- uInt32 vetexIdx;
  !-
  !- if (numPoints<2) return;
  !- minIndexL = maxIndex = 0;
  !- minPointY = maxPointY = polyPoints[0].y;
  !- for (i = 1; i < (int32)numPoints; i++) {
    !- if (polyPoints[i].y < minPointY) {
      !- minIndexL = i;
      !- minPointY = polyPoints[i].y; // new top
      !-}
    else if (polyPoints[i].y > maxPointY) {
      !- maxIndex = i;
      !- maxPointY = polyPoints[i].y; // new bottom
      !-} // else if
    !-} // for
  !-
  !- if (minPointY == maxPointY) return;
  !-
  !-// scan in ascending order to find the last top-edge point
  !- minIndexR = minIndexL;
  !- while (polyPoints[minIndexR].y == minPointY) index_forward(minIndexR, numPoints);
  !- index_backward(minIndexR, numPoints);// back up to last top-edge point
  !-
  !-// now scan in descending order to find the first top-edge point
  !- while (polyPoints[minIndexL].y == minPointY) index_backward(minIndexL, numPoints);
  !- index_forward(minIndexL, numPoints);
  !-
  !-// figure out which direction through the vertex list from the top
  !-// vertex is the left edge and which is the right
  !- leftEdgeDir = -1;
  !-
  !- topIsFlat = (polyPoints[minIndexL].x==polyPoints[minIndexR].x) ? 0 : 1;
  !- if (topIsFlat==1) {
    !- if (polyPoints[minIndexL].x > polyPoints[minIndexR].x) {
      !- leftEdgeDir = 1;
      !- temp = minIndexL;
      !- minIndexL = minIndexR;
      !- minIndexR = temp;
      !-}
    !-}
  else {
    !-    // Point to the downward end of the first line of each of the
    !-// two edges down from the top
    !- nextIndex = minIndexR;
    !- index_forward(nextIndex, numPoints);
    !- prevIndex = minIndexL;
    !- index_forward(prevIndex, numPoints);
    !-
    !- deltaXN = polyPoints[nextIndex].x - polyPoints[minIndexL].x;
    !- deltaYN = polyPoints[nextIndex].y - polyPoints[minIndexL].y;
    !- deltaXP = polyPoints[prevIndex].x - polyPoints[minIndexL].x;
    !- deltaYP = polyPoints[prevIndex].y - polyPoints[minIndexL].y;
    !- if (deltaXN * deltaYP - deltaYN * deltaXP < 0) {
      !- leftEdgeDir = 1;
      !- temp = minIndexL;
      !- minIndexL = minIndexR;
      !- minIndexR = temp;
      !-} // if
    !-} // else
  !-
  !- /* Set the # of scan lines in the polygon, skipping the bottom edge
   !-   * and also skipping the top vertex if the top isn't flat because
   !-   * in that case the top vertex has a right edge component, and set
   !-   * the top scan line to draw, which is likewise the second line of
   !-   * the polygon unles the top if flat
   !-   */
  !-
  !- workingHLineList.length = maxPointY - minPointY;
  !- if (workingHLineList.length <= 0) return;
  !- workingHLineList.yStart = minPointY;
  !-
  !-  // get memory in which to srote the line list we generate
  !- workingHLineList.xLeft = workingHLineList.xRight = NULL;
  !- if ((workingHLineList.xLeft = new int32[workingHLineList.length]) == NULL) return;
  !- if ((workingHLineList.xRight = new int32[workingHLineList.length]) == NULL) {
    !- delete workingHLineList.xLeft;
    !- return;
    !-}
  !- memset(workingHLineList.xLeft,0,workingHLineList.length*sizeof(int32));
  !- memset(workingHLineList.xRight,0,workingHLineList.length*sizeof(int32));
  !-
  !-  // scan the left edge and store the boundary points int he list
  !-// Initial pointer for storing scan converted left-edge coords
  !- edgePointIdx = 0;
  !-
  !-// start from the top of the left edge
  !- curIndex = prevIndex = minIndexL;
  !-
  !- do {
    !- index_move(curIndex, numPoints, leftEdgeDir);
    !- scanEdge(polyPoints[prevIndex].x,
      !- polyPoints[prevIndex].y,
      !- polyPoints[curIndex].x,
      !- polyPoints[curIndex].y,
      !- edgePointIdx,
      !- workingHLineList.xLeft);
    !- prevIndex = curIndex;
    !-}while (curIndex != maxIndex);
  !-
  !- edgePointIdx = 0;
  !- curIndex = prevIndex = minIndexR;
  !-  // Scan convert the right edge, top to bottom. X coordinates are
  !-// adjusted 1 to the left, effectively causing scan conversion of
  !-// the nearest points to the left of but not exactly on the edge }
  !- do {
    !- index_move(curIndex, numPoints, -leftEdgeDir);
    !- scanEdge(polyPoints[prevIndex].x,
      !- polyPoints[prevIndex].y,
      !- polyPoints[curIndex].x,
      !- polyPoints[curIndex].y,
      !- edgePointIdx,
      !- workingHLineList.xRight);
    !- prevIndex = curIndex;
    !-}while (curIndex != maxIndex);
  !-
  !- ogPolygon(numPoints, polyPoints, colour);
  !-
  !- for (i = 0; i < workingHLineList.length; i++) {
    !- HLine(workingHLineList.xLeft[i], workingHLineList.xRight[i],
      !- workingHLineList.yStart+i, colour);
    !-} // for
  !-
  !- ogPolygon(numPoints, polyPoints, colour);
  !-
  !- delete workingHLineList.xLeft;
  !- delete workingHLineList.xRight;
  !-
  !- return;
  !-} // ogSurface::FillConvexPolygon
#endif

void ogSurface::ogFillGouraudPolygon(uInt32 numPoints, ogPoint2d* polyPoints, ogRGBA8 * colours) {

  ogEdgeTable * edges;
  int32 currentY = ~0;

  if (numPoints < 3)
    return;

  edges = new ogEdgeTable();

  if (edges == NULL)
    return;  // sanity check

  edges->BuildGET_G(numPoints, polyPoints, colours);

  if (edges->globalEdges != NULL)
    currentY = edges->globalEdges->startY;

  while ((edges->globalEdges != NULL) || (edges->activeEdges != NULL)) {
    edges->MoveXSortedToAET(currentY);
    edges->ScanOutAET_G(*this, currentY);
    edges->AdvanceAET();
    edges->XSortAET();
    ++currentY;
    if (currentY > (int32) maxY)
      break; // if we've gone past the bottom, stop
  } // while

  delete edges;
  return;
} // ogSurface::ogFillGouraudPolygon

void ogSurface::ogFillPolygon(uInt32 numPoints, ogPoint2d* polyPoints, uInt32 colour) {
  ogEdgeTable * edges;
  int32 currentY = ~0;

  if (numPoints < 3)
    return;

  if (!ogIsBlending())
    ogPolygon(numPoints, polyPoints, colour);

  edges = new ogEdgeTable();

  if (edges == NULL)
    return;  // sanity check

  edges->BuildGET(numPoints, polyPoints);

  if (edges->globalEdges != NULL)
    currentY = edges->globalEdges->startY;

  while ((edges->globalEdges != NULL) || (edges->activeEdges != NULL)) {
    edges->MoveXSortedToAET(currentY);
    edges->ScanOutAET(*this, currentY, colour);
    edges->AdvanceAET();
    edges->XSortAET();
    ++currentY;
    if (currentY > (int32) maxY)
      break; // if we've gone past the bottom, stop
  } // while
  delete edges;
  return;
} // ogSurface::ogFillPolygon

void ogSurface::ogFillRect(int32 x1, int32 y1, int32 x2, int32 y2, uInt32 colour) {
  int32 yy, tmp;

  if (x2 < x1) {
    tmp = x2;
    x2 = x1;
    x1 = tmp;
  } // if

  if (y2 < y1) {
    tmp = y2;
    y2 = y1;
    y1 = tmp;
  } // if

  if ((y2 < 0) || (y1 > (int32) maxY))
    return;
  if (y1 < 0)
    y1 = 0;
  if (y2 > (int32) maxY)
    y2 = maxY;
  for (yy = y1; yy <= y2; yy++)
    ogHLine(x1, x2, yy, colour);
} // ogSurface::ogFillRect

void ogSurface::ogFillTriangle(int32 x1, int32 y1, int32 x2, int32 y2, int32 x3, int32 y3, uInt32 colour) {
  ogPoint2d points[3];
  points[0].x = x1;
  points[0].y = y1;
  points[1].x = x2;
  points[1].y = y2;
  points[2].x = x3;
  points[2].y = y3;

  ogFillPolygon(3, points, colour);

  return;
} // ogSurface::ogFillTriangle

uInt32 ogSurface::ogGetAlpha(void) {
  if (attributes != NULL)
    return attributes->defaultAlpha;
  else
    return 255L;
} // ogSurface::ogGetAlpha

ogErrorCode ogSurface::ogGetLastError(void) {
  ogErrorCode tmp = lastError;
  lastError = ogOK;
  return tmp;
} // ogSurface::ogGetLastError

void ogSurface::ogGetPalette(ogRGBA8 _pal[256]) {
  memcpy(_pal, pal, sizeof(_pal));
  return;
} // ogSurface::ogGetPalette

void ogSurface::ogGetPixFmt(ogPixelFmt& pixfmt) {
  pixfmt.BPP = BPP;
  pixfmt.redFieldPosition = redFieldPosition;
  pixfmt.greenFieldPosition = greenFieldPosition;
  pixfmt.blueFieldPosition = blueFieldPosition;
  pixfmt.alphaFieldPosition = alphaFieldPosition;
  pixfmt.redMaskSize = 8 - redShifter;
  pixfmt.greenMaskSize = 8 - greenShifter;
  pixfmt.blueMaskSize = 8 - blueShifter;
  pixfmt.alphaMaskSize = 8 - alphaShifter;
  return;
} // ogSurface::ogGetPixFmt

uInt32 ogSurface::ogGetPixel(int32 x, int32 y) {
  uInt32 result;
  if (!ogAvail())
    return ogGetTransparentColor();

  if (((uInt32) x > maxX) || ((uInt32) y > maxY))
    return ogGetTransparentColor();

  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(
        "  shl  $2, %%ecx      \n"  // shl     ecx, 2 {adjust for pixel size}
        "  add  %%esi, %%edi   \n"// add     edi, esi
        "  add  %%ecx, %%edi   \n"// add     edi, ecx
        "  mov  (%%edi),%%eax  \n"// eax,word ptr [edi]
        "  mov  %%eax, %3      \n"// mov     result, eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "m" (result)// %2, %3
      );
    break;
    case 3:
      __asm__ __volatile__(
        "  leal (%%ecx, %%ecx, 2), %%ecx \n"  // lea ecx, [ecx + ecx*2]
//     "  mov  %%ecx, %%eax   \n"  // mov     eax, ecx  - adjust for pixel size
//     "  add  %%ecx, %%ecx   \n"  // add     ecx, ecx  - adjust for pixel size
//     "  add  %%eax, %%ecx   \n"  // add     ecx, eax  - adjust for pixel size
        "  add  %%esi, %%edi   \n"// add     edi, esi
        "  add  %%ecx, %%edi   \n"// add     edi, ecx
        "  movzwl (%%edi),%%eax \n"// edx,word ptr [edi]
        "  xor  %%eax, %%eax   \n"
        "  mov  2(%%edi), %%al \n"// mov     al, [edi+2]
        "  shl  $16, %%eax     \n"// shl     eax, 16
        "  mov  (%%edi), %%ax  \n"// mov     ax, [edi]
        "  mov  %%eax, %3      \n"// mov     result, eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "m" (result)// %2, %3
      );
    break;
    case 2:
      __asm__ __volatile__(
        "  add  %%esi, %%edi   \n"  // add     edi, esi
        "  add  %%ecx, %%ecx   \n"// add     ecx, ecx {adjust for pixel size}
        "  add  %%ecx, %%edi   \n"// add     edi, ecx
        "  movzwl (%%edi),%%eax \n"// movzx   edx,word ptr [edi]
        "  mov  %%eax, %0      \n"// mov     result, eax
        : "=m" (result)
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x)// , "m" (result)              // %2, %3
      );
    break;
    case 1:
      __asm__ __volatile__(
        "  add  %%esi, %%edi   \n"  // add     edi, esi
        "  add  %%ecx, %%edi   \n"// add     edi, ecx
        " movzbl (%%edi),%%eax \n"// movzx   edx,byte ptr [edi]
        "  mov  %%eax, %3      \n"// mov     result, eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "m" (result)// %2, %3
      );
    break;
  } // switch
  return result;
} // ogSurface::ogGetPixel

void *
ogSurface::ogGetPtr(uInt32 x, uInt32 y) {
//  return (Avail() ? ( (uInt8*)buffer+(lineOfs[y]+x*((BPP+7) >> 3)) ) : NULL );
  return ((uInt8*) buffer + (lineOfs[y] + x * bytesPerPix));
} // ogSurface::ogGetPtr

uInt32 ogSurface::ogGetTransparentColor(void) {
  if (attributes != NULL)
    return attributes->transparentColor;
  else
    return 0;
} // ogSurface::ogGetTransparentColor

void ogSurface::ogHFlip(void) {
  void * tmpBuf1;
  void * tmpBuf2;
  uInt32 xWidth, count;

  if (!ogAvail())
    return;

  xWidth = (maxX + 1) * bytesPerPix;

#ifdef __UBIXOS_KERNEL__
  tmpBuf1 = kmalloc(xWidth);
  tmpBuf2 = kmalloc(xWidth);
#else
  tmpBuf1 = malloc(xWidth);
  tmpBuf2 = malloc(xWidth);
#endif

  if ((tmpBuf1 != NULL) && (tmpBuf2 != NULL))
    for (count = 0; count <= (maxY / 2); count++) {
      ogCopyLineFrom(0, count, tmpBuf1, xWidth);
      ogCopyLineFrom(0, maxY - count, tmpBuf2, xWidth);
      ogCopyLineTo(0, maxY - count, tmpBuf1, xWidth);
      ogCopyLineTo(0, count, tmpBuf2, xWidth);
    } // for count

#ifdef __UBIXOS_KERNEL__
    kfree(tmpBuf2);
    kfree(tmpBuf1);
#else
  free(tmpBuf2);
  free(tmpBuf1);
#endif

  return;
} // ogSurface::ogHFlip

void ogSurface::ogHLine(int32 x1, int32 x2, int32 y, uInt32 colour) {
  int32 tmp;
  uInt8 r, g, b, a;

  if (!ogAvail())
    return;
  if ((uInt32) y > maxY)
    return;

  if (x1 > x2) {
    tmp = x1;
    x1 = x2;
    x2 = tmp;
  } // if

  if (x1 < 0)
    x1 = 0;
  if (x2 > (int32) maxX)
    x2 = maxX;
  if (x2 < x1)
    return;

  if (ogIsBlending()) {
    ogUnpack(colour, r, g, b, a);
    if (a == 0)
      return;
    if (a == 255) {
      for (tmp = x1; tmp <= x2; tmp++)
        RawSetPixel(tmp, y, r, g, b, a);
      return;
    } // if a == 255
  } // if blending

  __asm__ __volatile__("cld \n");
  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(
        "  sub  %%ebx, %%ecx  \n"          //  sub      ecx, ebx
        "  add  %%esi, %%edi  \n"//  add      edi, esi
        "  inc  %%ecx         \n"
        "  shl  $2, %%ebx     \n"//  shl      ebx, 2
        "  add  %%ebx, %%edi  \n"//  add      edi, ebx
        "  rep                \n"
        "  stosl              \n"
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "a" (colour), "b" (x1),// %2, %3
        "c" (x2)// %4
      );
    break;
    case 3:
      __asm__ __volatile__(
        "  sub  %%ebx, %%ecx  \n"          //  sub      ecx, ebx
        "  add  %%esi, %%edi  \n"//  add      edi, esi
        "  add  %%ebx, %%ebx  \n"//  add      ebx, ebx - pix size
        "  inc  %%ecx         \n"//  inc      ecx
        "  add  %%edx, %%ebx  \n"//  add      ebx, edx - pix size
        "  add  %%ebx, %%edi  \n"//  add      edi, ebx
        "  mov  %%eax, %%ebx  \n"//  mov      ebx, eax
        "  shr  $16, %%ebx    \n"//  shr      ebx, 16
        "hLlop24:                 \n"
        "  mov  %%ax, (%%edi) \n"//  mov      [edi], ax
        "  mov  %%bl, 2(%%edi)\n"//  mov      [edi+2], bl
        "  add  $3, %%edi     \n"//  add      edi, 3
        "  dec  %%ecx         \n"//  dec      ecx
        "   jnz hLlop24       \n"
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "a" (colour), "b" (x1),// %2, %3
        "c" (x2), "d" (x1)// %4, %5
      );
    break;
    case 2:
      __asm__ __volatile__(
        "  sub  %%ebx, %%ecx  \n"          //  sub      ecx, ebx
        "  add  %%ebx, %%ebx  \n"//  add      ebx, ebx - pix size
        "  inc  %%ecx         \n"//  inc      ecx
        "  add  %%ebx, %%edi  \n"//  add      edi, ebx
        "  add  %%esi, %%edi  \n"//  add      edi, esi
        "  xor  %%edx, %%edx  \n"//  xor      edx, edx
        "  mov  %%ax, %%dx    \n"//  mov      dx, ax
        "  shl  $16, %%eax    \n"//  shl      eax, 16
        "  add  %%edx, %%eax  \n"//  add      eax, edx

        "  shr  $1, %%ecx     \n"//  shr      ecx, 1
        "  rep                \n"
        "  stosl              \n"
        "   jnc hLnoc16       \n"
        "  stosw              \n"
        "hLnoc16:             \n"
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "a" (colour), "b" (x1),// %2, %3
        "c" (x2)// %4
      );
    break;
    case 1:
      __asm__ __volatile__(
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  add  %%esi, %%edi  \n"//  add      edi, esi
        "  and  $0xff, %%eax  \n"//  and      eax, 0ffh
        "  sub  %%ebx, %%ecx  \n"//  sub      ecx, ebx
        "  mov  %%al, %%ah    \n"//  mov      ah, al
        "  inc  %%ecx         \n"//  inc      ecx
        "  mov  %%eax, %%ebx  \n"//  mov      ebx, eax
        "  shl  $16, %%ebx    \n"//  shl      ebx, 16
        "  add  %%ebx, %%eax  \n"//  add      eax, ebx

        "  mov  %%ecx, %%edx  \n"//  mov      edx, ecx
        "  mov  $4, %%ecx     \n"//  mov      ecx, 4
        "  sub  %%edi, %%ecx  \n"//  sub      ecx, edi
        "  and  $3, %%ecx     \n"//  and      ecx, 3
        "  sub  %%ecx, %%edx  \n"//  sub      edx, ecx
        "   jle LEndBytes     \n"
        "  rep                \n"
        "  stosb              \n"
        "  mov  %%edx, %%ecx  \n"//  mov      ecx, edx
        "  and  $3, %%edx     \n"//  and      edx, 3
        "  shr  $2, %%ecx     \n"//  shr      ecx, 2
        "  rep                \n"
        "  stosl              \n"
        "LEndBytes:               \n"
        "  add  %%edx, %%ecx  \n"//  add      ecx, edx
        "  rep                \n"
        "  stosb              \n"
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "a" (colour), "b" (x1),// %2, %3
        "c" (x2)
      );
    break;
  } // switch
  return;
} // ogSurface::ogHLine

bool ogSurface::ogIsAntiAliasing(void) {
  if (attributes != NULL)
    return attributes->antiAlias;
  else
    return false;
} // ogSurface::ogIsAntiAliasing

bool ogSurface::ogIsBlending(void) {
  if (attributes != NULL)
    return attributes->blending;
  else
    return false;
} // ogSurface::ogIsBlending

void ogSurface::ogLine(int32 x1, int32 y1, int32 x2, int32 y2, uInt32 colour) {
  if (ClipLine(x1, y1, x2, y2)) {
    if (ogIsAntiAliasing())
      AARawLine(x1, y1, x2, y2, colour);
    else
      RawLine(x1, y1, x2, y2, colour);
  } // if clipLine
  return;
} // ogSurface::ogLine

bool ogSurface::ogLoadPalette(const char *palfile) {
  ogRGBA8 oldPalette[256];
#ifdef __UBIXOS_KERNEL__
  fileDescriptor *f;
#else
  FILE *f;
#endif
  uInt32 lresult;
  bool result;

  if (!fileExists(palfile)) {
    ogSetLastError(ogFileNotFound);
    return false;
  } // if

  if (pal == NULL) {
    pal = new ogRGBA8[256];
    if (pal == NULL) {
      ogSetLastError(ogMemAllocFail);
      return false;
    } // if
    ogSetPalette(DEFAULT_PALETTE);
    //   memcpy(pal, DEFAULT_PALETTE, sizeof(ogRGBA8)*256);
  } // if

  ogGetPalette(oldPalette);
  // memcpy(&oldPalette, pal, sizeof(ogRGBA8)*256);

  if ((f = fopen(palfile, "rb")) == NULL)
    return false;

  lresult = fread(pal, sizeof(ogRGBA8), 256, f);
  result = (lresult == 256);

  if (!result) {
    ogSetLastError(ogFileReadError);
    ogSetPalette(oldPalette);
    // memcpy(pal, &oldPalette, sizeof(ogRGBA8)*256);
  } // if

  fclose(f);
  return result;
} // ogSurface::ogLoadPalette

void ogSurface::ogPolygon(uInt32 numPoints, ogPoint2d* polyPoints, uInt32 colour) {
  uInt32 count;

  if (numPoints == 1)
    ogSetPixel(polyPoints[0].x, polyPoints[0].y, colour);
  else
    for (count = 0; count < numPoints; count++)
      ogLine(polyPoints[count].x, polyPoints[count].y, polyPoints[(count + 1) % numPoints].x, polyPoints[(count + 1) % numPoints].y, colour);
  return;
} // ogSurface::ogPolygon

void ogSurface::ogRect(int32 x1, int32 y1, int32 x2, int32 y2, uInt32 colour) {
  int32 tmp;

  if ((x1 == x2) || (y1 == y2)) {

    if ((x1 == x2) && (y1 == y2))
      ogSetPixel(x1, y1, colour);
    else
      ogLine(x1, y1, x2, y2, colour);

  }
  else {

    if (y1 > y2) {
      tmp = y1;
      y1 = y2;
      y2 = tmp;
    } // if

    ogHLine(x1, x2, y1, colour);  // Horizline has built in clipping
    ogVLine(x1, y1 + 1, y2 - 1, colour);  // vertline has built in clipping too
    ogVLine(x2, y1 + 1, y2 - 1, colour);
    ogHLine(x1, x2, y2, colour);

  } // else

  return;
} // ogSurface::ogRect

uInt32 ogSurface::ogPack(uInt8 red, uInt8 green, uInt8 blue) {
  uInt32 idx, colour;
  uInt32 rd, gd, bd, dist, newdist;

  colour = 0;
  switch (bytesPerPix) {
    case 4:
      colour = ((red << redFieldPosition) | (green << greenFieldPosition) | (blue << blueFieldPosition) | (ogGetAlpha() << alphaFieldPosition));
    break;
    case 3:
      colour = ((red << redFieldPosition) | (green << greenFieldPosition) | (blue << blueFieldPosition));
    break;
    case 2:
      colour = ((red >> redShifter) << redFieldPosition) | ((green >> greenShifter) << greenFieldPosition) | ((blue >> blueShifter) << blueFieldPosition) | ((ogGetAlpha() >> alphaShifter) << alphaFieldPosition);
    break;
    case 1:
      colour = 0;
      dist = 255 + 255 + 255;
      for (idx = 0; idx <= 255; idx++) {
        rd = abs(red - pal[idx].red);
        gd = abs(green - pal[idx].green);
        bd = abs(blue - pal[idx].blue);
        newdist = rd + gd + bd;

        if (newdist < dist) {
          dist = newdist;
          colour = idx;
        } // if
      } // for
    break;
  } // switch

  return colour;

} // ogSurface::ogPack

uInt32 ogSurface::ogPack(uInt8 red, uInt8 green, uInt8 blue, uInt8 alpha) {
  uInt32 idx, colour;
  uInt32 rd, gd, bd, dist, newdist;

  colour = 0;
  switch (bytesPerPix) {
    case 4:
      colour = ((red << redFieldPosition) | (green << greenFieldPosition) | (blue << blueFieldPosition) | (alpha << alphaFieldPosition));
    break;
    case 3:
      colour = ((red << redFieldPosition) | (green << greenFieldPosition) | (blue << blueFieldPosition));
    break;
    case 2:
      colour = ((red >> redShifter) << redFieldPosition) | ((green >> greenShifter) << greenFieldPosition) | ((blue >> blueShifter) << blueFieldPosition) | ((alpha >> alphaShifter) << alphaFieldPosition);
    break;
    case 1:
      colour = 0;
      dist = 255 + 255 + 255;
      for (idx = 0; idx <= 255; idx++) {
        rd = abs(red - pal[idx].red);
        gd = abs(green - pal[idx].green);
        bd = abs(blue - pal[idx].blue);
        newdist = rd + gd + bd;

        if (newdist < dist) {
          dist = newdist;
          colour = idx;
        } // if
      } // for
    break;
  } // switch

  return colour;
} // ogSurface::ogPack

bool ogSurface::ogSavePalette(const char *palfile) {
#ifdef __UBIXOS_KERNEL__
  fileDescriptor *f;
#else
  FILE * f;
#endif
  uInt32 lresult;

  if (pal == NULL) {
    ogSetLastError(ogNoPalette);
    return false;
  }

  if ((f = fopen(palfile, "wb")) == NULL)
    return false;
  lresult = fwrite(pal, sizeof(ogRGBA8), 256, f);
  fclose(f);

  if (lresult == 256)
    return true;
  else {
    ogSetLastError(ogFileWriteError);
    return false;
  } // else

} // ogSurface::ogSavePal

void ogSurface::ogScale(ogSurface& src) {
  ogScaleBuf(0, 0, maxX, maxY, src, 0, 0, src.maxX, src.maxY);
  return;
} // ogSurface::ogScale

void ogSurface::ogScaleBuf(int32 dX1, int32 dY1, int32 dX2, int32 dY2, ogSurface& src, int32 sX1, int32 sY1, int32 sX2, int32 sY2) {

  uInt32 sWidth, dWidth;
  uInt32 sHeight, dHeight;
  int32 sx, sy, xx, yy;
  uInt32 xInc, yInc;
  uInt32 origdX1, origdY1;
  ogPixelFmt pixFmt;
  ogSurface * tmpBuf;
  ogSurface * sBuf;
  ogSurface * dBuf;
  bool doCopyBuf;

  origdX1 = origdY1 = 0; // to keep the compiler from generating a warning

  if (!ogAvail())
    return;
  if (!src.ogAvail())
    return;

  if (sX1 > sX2) {
    xx = sX1;
    sX1 = sX2;
    sX2 = xx;
  }

  if (sY1 > sY2) {
    yy = sY1;
    sY1 = sY2;
    sY2 = yy;
  }

  // if any part of the source falls outside the buffer then don't do anything

  if (((uInt32) sX1 > src.maxX) || ((uInt32) sX2 > src.maxX) || ((uInt32) sY1 > src.maxY) || ((uInt32) sY2 > src.maxY))
    return;

  if (dX1 > dX2) {
    xx = dX1;
    dX1 = dX1;
    dX2 = xx;
  }

  if (dY1 > dY2) {
    yy = dY1;
    dY1 = dY2;
    dY2 = yy;
  }

  dWidth = (dX2 - dX1) + 1;
  if (dWidth <= 0)
    return;

  dHeight = (dY2 - dY1) + 1;
  if (dHeight <= 0)
    return;

  sWidth = (sX2 - sX1) + 1;
  sHeight = (sY2 - sY1) + 1;

  // convert into 16:16 fixed point ratio
  xInc = (sWidth << 16) / dWidth;
  yInc = (sHeight << 16) / dHeight;

  if (dX2 > (int32) maxX) {
    xx = (xInc * (dX1 - maxX)) >> 16;
    sX1 -= xx;
    sWidth -= xx;
    dWidth -= (dX1 - maxX);
    dX1 = maxX;
  }

  if (dY2 > (int32) maxY) {
    yy = (yInc * (dY2 - maxY)) >> 16;
    sY2 -= yy;
    sHeight -= yy;
    dHeight -= (dY2 - maxY);
    dY2 = maxY;
  }

  if (dX1 < 0) {
    xx = (xInc * (-dX1)) >> 16;
    sX1 += xx;
    sWidth -= xx;
    dWidth += dX1;
    dX1 = 0;
  }

  if (dY1 < 0) {
    yy = (yInc * (-dY1)) >> 16;
    sY1 += yy;
    sHeight -= yy;
    dHeight += dY1;
    dY1 = 0;
  }

  if ((dWidth <= 0) || (dHeight <= 0))
    return;
  if ((sWidth <= 0) || (sHeight <= 0))
    return;

  // Do a quick check to see if the scale is 1:1 .. in that case just copy
  // the image

  if ((dWidth == sWidth) && (dHeight == sHeight)) {
    ogCopyBuf(dX1, dY1, src, sX1, sY1, sX2, sY2);
    return;
  }

  tmpBuf = NULL;

  /*
   * Alright.. this is how we're going to optimize the case of different
   * pixel formats.  We are going to use copyBuf() to automagically do
   * the conversion for us using tmpBuf.  Here's how it works:
   * If the source buffer is smaller than the dest buffer (ie, we're making
   * something bigger) we will convert the source buffer first into the dest
   * buffer's pixel format.  Then we do the scaling.
   * If the source buffer is larger than the dest buffer (ie, we're making
   * something smaller) we will scale first and then use copyBuf to do
   * the conversion.
   * This method puts the onus of conversion on the copyBuf() function which,
   * while not excessively fast, does the job.
   * The case in which the source and dest are the same size is handled above.
   *
   */
  if (pixFmtID != src.pixFmtID) {

    tmpBuf = new ogSurface();
    if (tmpBuf == NULL)
      return;
    if (sWidth * sHeight * src.bytesPerPix <= dWidth * dHeight * bytesPerPix) {
      // if the number of pixels in the source buffer is less than the
      // number of pixels in the dest buffer then...
      ogGetPixFmt(pixFmt);
      if (!tmpBuf->ogCreate(sWidth, sHeight, pixFmt))
        return;
      tmpBuf->ogCopyPalette(src);
      tmpBuf->ogCopyBuf(0, 0, src, sX1, sY1, sX2, sY2);
      sX2 -= sX1;
      sY2 -= sY1;
      sX1 = 0;
      sY1 = 0;
      sBuf = tmpBuf;
      dBuf = this;
      doCopyBuf = false; // do we do a copyBuf later?
    }
    else {
      src.ogGetPixFmt(pixFmt);
      if (!tmpBuf->ogCreate(dWidth, dHeight, pixFmt))
        return;
      tmpBuf->ogCopyPalette(*this);
      origdX1 = dX1;
      origdY1 = dY1;
      dX1 = 0;
      dY1 = 0;
      dX2 = tmpBuf->maxX;
      dY2 = tmpBuf->maxY;
      sBuf = &src;
      dBuf = tmpBuf;
      doCopyBuf = true;
    } // else
  }
  else {
    // pixel formats are identical
    sBuf = &src;
    dBuf = this;
    doCopyBuf = false;
  } // else

  sy = sY1 << 16;

  for (yy = dY1; yy <= dY2; yy++) {
    sx = 0;
    for (xx = dX1; xx <= dX2; xx++) {
      dBuf->RawSetPixel(xx, yy, sBuf->RawGetPixel(sX1 + (sx >> 16), (sy >> 16)));
      sx += xInc;
    } // for xx
    sy += yInc;
  } // for yy

  if ((doCopyBuf) && (tmpBuf != NULL))
    ogCopyBuf(origdX1, origdY1, *tmpBuf, 0, 0, tmpBuf->maxX, tmpBuf->maxY);

  delete tmpBuf;
  return;
} // ogSurface::ogScaleBuf

uInt32 ogSurface::ogSetAlpha(uInt32 _newAlpha) {
  uInt32 tmp;

  if (attributes != NULL) {
    tmp = attributes->defaultAlpha;
    attributes->defaultAlpha = _newAlpha;
    return tmp;
  }
  else
    return _newAlpha;
} // ogSurface::ogSetAlpha

bool ogSurface::ogSetAntiAliasing(bool _antiAliasing) {
  bool tmp;

  if (attributes != NULL) {
    tmp = attributes->antiAlias;
    attributes->antiAlias = _antiAliasing;
    return tmp;
  }
  else
    return _antiAliasing;
} // ogSurface::ogSetAntiAliasing

bool ogSurface::ogSetBlending(bool _blending) {
  bool tmp;

  if (attributes != NULL) {
    tmp = attributes->blending;
    attributes->blending = _blending;
    return tmp;
  }
  else
    return _blending;

} // ogSurface::ogSetBlending;

ogErrorCode ogSurface::ogSetLastError(ogErrorCode latestError) {
  ogErrorCode tmp = lastError;
  lastError = latestError;
  return tmp;
} // ogSurface::ogSetLastError

void ogSurface::ogSetPalette(const ogRGBA8 newPal[256]) {
  if (pal == NULL)
    return;
  memcpy(pal, newPal, sizeof(pal));
  return;
} // ogSurface::ogSetPalette

void ogSurface::ogSetPixel(int32 x, int32 y, uInt32 colour) {
  uInt32 newR, newG, newB, inverseA;
  uInt8 sR, sG, sB, sA;
  uInt8 dR, dG, dB;

  if (!ogAvail())
    return;

  if (((uInt32) x > maxX) || ((uInt32) y > maxY))
    return;

  do {
    if (ogIsBlending()) {
      ogUnpack(colour, sR, sG, sB, sA);
      if (sA == 0)
        return;
      if (sA == 255)
        break;
      inverseA = 255 - sA;
      ogUnpack(RawGetPixel(x, y), dR, dG, dB);
      newR = (dR * inverseA + sR * sA) >> 8;
      newG = (dG * inverseA + sG * sA) >> 8;
      newB = (dB * inverseA + sB * sA) >> 8;
      colour = ogPack(newR, newG, newB, inverseA);
    } // if
  } while (false);

  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        "  shl  $2, %%ecx     \n"// shl     eax, 2 {adjust for pixel size}
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%eax, (%%edi) \n"// mov     [edi], eax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 3:
      __asm__ __volatile__(
        //  Calculate offset, prepare the pixel to be drawn
        "  leal (%%ecx, %%ecx, 2), %%ecx \n"// lea ecx, [ecx + ecx*2]
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
        "  shr  $16, %%eax    \n"// shr     eax, 16
        "  mov  %%al, 2(%%edi)\n"// mov     [edi+2],al
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 2:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        "  add  %%ecx, %%ecx  \n"// add     ecx, ecx {adjust for pixel size}
        "  add  %%esi, %%edi  \n"// add     edi, esi
        //    "  mov  %3, %%eax     \n"  // mov     eax, colour
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"// mov     [edi], ax
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
    case 1:
      __asm__ __volatile__(
        // { Calculate offset, prepare the pixel to be drawn }
        //    "  add  (%%esi,%%ebx,4), %%edi \n" // add     edi, [esi + ebx * 4]
        "  add  %%esi, %%edi  \n"// add     edi, esi
        "  add  %%ecx, %%edi  \n"// add     edi, ecx
        // { Draw the pixel }
        "  mov  %%al, (%%edi) \n"// mov     [edi], al
        :
        : "D" (buffer), "S" (lineOfs[y]),// %0, %1
        "c" (x), "a" (colour)// %2, %3
      );
    break;
  } // switch
  return;
} // ogSurface::ogSetPixel

void ogSurface::ogSetPixel(int32 x, int32 y, uInt8 r, uInt8 g, uInt8 b, uInt8 a) {
  if (!ogAvail())
    return;
  if (((uInt32) x > maxX) || ((uInt32) y > maxY))
    return;
  RawSetPixel(x, y, r, g, b, a);
  return;
} // ogSurface::ogSetPixel

void ogSurface::ogSetPalette(uInt8 colour, uInt8 red, uInt8 green, uInt8 blue, uInt8 alpha) {
  if (pal == NULL)
    return;
  pal[colour].red = red;
  pal[colour].green = green;
  pal[colour].blue = blue;
  pal[colour].alpha = alpha;
  return;
} // ogSurface::ogSetPalette

void ogSurface::ogSetPalette(uInt8 colour, uInt8 red, uInt8 green, uInt8 blue) {
  if (pal == NULL)
    return;
  pal[colour].red = red;
  pal[colour].green = green;
  pal[colour].blue = blue;
  pal[colour].alpha = ogGetAlpha();
  return;
} // ogSurface::ogSetPalette

uInt32 ogSurface::ogSetTransparentColor(uInt32 colour) {
  uInt32 tmp = 0;

  if (attributes != NULL) {
    tmp = attributes->transparentColor & ogGetAlphaMasker();
    attributes->transparentColor = colour & ogGetAlphaMasker();
  } // if

  return tmp;
} // ogSurface::ogSetTransparentColor

static double f(double g) {
  return g * g * g - g;
}

void ogSurface::ogSpline(uInt32 numPoints, ogPoint2d* points, uInt32 segments, uInt32 colour) {
  int32 i, oldY, oldX, x, y, j;
  float part, t, xx, yy, tmp;
  float * zc;
  float * dx;
  float * dy;
  float * u;
  float * wndX1;
  float * wndY1;
  float * px;
  float * py;

  bool runOnce;

  if ((numPoints < 2) || (points == NULL))
    return;

  zc = new float[numPoints];
  dx = new float[numPoints];
  dy = new float[numPoints];
  u = new float[numPoints];
  wndX1 = new float[numPoints];
  wndY1 = new float[numPoints];
  px = new float[numPoints];
  py = new float[numPoints];

  do {
    if (zc == NULL)
      break;
    if (dx == NULL)
      break;
    if (dy == NULL)
      break;
    if (wndX1 == NULL)
      break;
    if (wndY1 == NULL)
      break;
    if (px == NULL)
      break;
    if (py == NULL)
      break;

    for (i = 0; (uInt32) i < numPoints; i++) {
      zc[i] = dx[i] = dy[i] = u[i] = wndX1[i] = wndY1[i] = px[i] = py[i] = 0.0f;
    }

    runOnce = false;
    oldX = oldY = 0;

    x = points[0].x;
    y = points[0].y;

    for (i = 1; (uInt32) i < numPoints; i++) {
      xx = points[i - 1].x - points[i].x;
      yy = points[i - 1].y - points[i].y;
      t = sqrt(xx * xx + yy * yy);
      zc[i] = zc[i - 1] + t;
    } // for

    u[0] = zc[1] - zc[0] + 1;
    for (i = 1; (uInt32) i < numPoints - 1; i++) {
      u[i] = zc[i + 1] - zc[i] + 1;
      tmp = 2 * (zc[i + 1] - zc[i - 1]);
      dx[i] = tmp;
      dy[i] = tmp;
      wndY1[i] = 6.0f * ((points[i + 1].y - points[i].y) / u[i] - (points[i].y - points[i - 1].y) / u[i - 1]);
      wndX1[i] = 6.0f * ((points[i + 1].x - points[i].x) / u[i] - (points[i].x - points[i - 1].x) / u[i - 1]);
    } // for

    for (i = 1; (uInt32) i < numPoints - 2; i++) {
      wndY1[i + 1] = wndY1[i + 1] - wndY1[i] * u[i] / dy[i];
      dy[i + 1] = dy[i + 1] - u[i] * u[i] / dy[i];
      wndX1[i + 1] = wndX1[i + 1] - wndX1[i] * u[i] / dx[i];
      dx[i + 1] = dx[i + 1] - u[i] * u[i] / dx[i];
    } // for

    for (i = numPoints - 2; i > 0; i--) {
      py[i] = (wndY1[i] - u[i] * py[i + 1]) / dy[i];
      px[i] = (wndX1[i] - u[i] * px[i + 1]) / dx[i];
    } // for

    for (i = 0; (uInt32) i < numPoints - 1; i++) {
      for (j = 0; (uInt32) j <= segments; j++) {
        part = zc[i] - (((zc[i] - zc[i + 1]) / segments) * j);
        t = (part - zc[i]) / u[i];
        part = t * points[i + 1].y + (1.0 - t) * points[i].y + u[i] * u[i] * (f(t) * py[i + 1] + f(1.0 - t) * py[i]) / 6.0;
//        y = Round(part);
        y = static_cast<int32>(part + 0.5f);
        part = zc[i] - (((zc[i] - zc[i + 1]) / segments) * j);
        t = (part - zc[i]) / u[i];
        part = t * points[i + 1].x + (1.0 - t) * points[i].x + u[i] * u[i] * (f(t) * px[i + 1] + f(1.0 - t) * px[i]) / 6.0;

//      x = Round(part);
        x = static_cast<int32>(part + 0.5f);
        if (runOnce)
          ogLine(oldX, oldY, x, y, colour);
        else
          runOnce = true;
        oldX = x;
        oldY = y;
      } // for j
    } // for i
  } while (false);

  delete[] py;
  delete[] px;
  delete[] wndY1;
  delete[] wndX1;
  delete[] u;
  delete[] dy;
  delete[] dx;
  delete[] zc;

  return;
} // ogSurface::ogSpline

void ogSurface::ogTriangle(int32 x1, int32 y1, int32 x2, int32 y2, int32 x3, int32 y3, uInt32 colour) {

  ogLine(x1, y1, x2, y2, colour);
  ogLine(x2, y2, x3, y3, colour);
  ogLine(x3, y3, x1, y1, colour);
  return;
} // ogSurface::ogTriangle

void ogSurface::ogUnpack(uInt32 colour, uInt8& red, uInt8& green, uInt8& blue) {

  switch (bytesPerPix) {
    case 4:
    case 3:
      red = colour >> redFieldPosition;
      green = colour >> greenFieldPosition;
      blue = colour >> blueFieldPosition;
    break;
    case 2:
      red = ((colour >> redFieldPosition) << redShifter);
      green = ((colour >> greenFieldPosition) << greenShifter);
      blue = ((colour >> blueFieldPosition) << blueShifter);
      if (red != 0)
        red += OG_MASKS[redShifter];
      if (green != 0)
        green += OG_MASKS[greenShifter];
      if (blue != 0)
        blue += OG_MASKS[blueShifter];
    break;
    case 1:

      if (pal == NULL) {
        red = green = blue = 0;
        return;
      } // if pal == null

      if (colour > 255)
        colour &= 255;
      red = pal[colour].red;
      green = pal[colour].green;
      blue = pal[colour].blue;
    break;
    default:
      red = 0;
      green = 0;
      blue = 0;
  } // switch

  return;
} // ogSurface::ogUnpack

void ogSurface::ogUnpack(uInt32 colour, uInt8& red, uInt8& green, uInt8& blue, uInt8& alpha) {

  switch (bytesPerPix) {
    case 4:
      red = colour >> redFieldPosition;
      green = colour >> greenFieldPosition;
      blue = colour >> blueFieldPosition;
      alpha = colour >> alphaFieldPosition;
    break;
    case 3:
      red = colour >> redFieldPosition;
      green = colour >> greenFieldPosition;
      blue = colour >> blueFieldPosition;
      alpha = ogGetAlpha();
    break;
    case 2:
      red = ((colour >> redFieldPosition) << redShifter);
      green = ((colour >> greenFieldPosition) << greenShifter);
      blue = ((colour >> blueFieldPosition) << blueShifter);
      if (red != 0)
        red += OG_MASKS[redShifter];
      if (green != 0)
        green += OG_MASKS[greenShifter];
      if (blue != 0)
        blue += OG_MASKS[blueShifter];

      if (alphaShifter != 8) {
        alpha = (colour >> alphaFieldPosition) << alphaShifter;
        if (alpha != 0)
          alpha += OG_MASKS[alphaShifter];
      }
      else
        alpha = ogGetAlpha();

    break;
    case 1:

      if (pal == NULL) {
        red = green = blue = alpha = 0;
        return;
      } // if pal == null

      if (colour > 255)
        colour &= 255;
      red = pal[colour].red;
      green = pal[colour].green;
      blue = pal[colour].blue;
      alpha = pal[colour].alpha;
    break;
    default:
      red = green = blue = alpha = 0;
  } // switch

  return;
} // ogSurface::ogUnpack

void ogSurface::ogVFlip(void) {
  uInt32 height;

  if (!ogAvail())
    return;

  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(
        "  add  %%edi, %%esi  \n"     // add esi, edi
        "vf32lop:             \n"
        "  push %%esi         \n"// push esi
        "  push %%edi         \n"// push edi
        "vf32lop2:            \n"
        "  mov  (%%edi),%%eax \n"// mov eax, [edi]
        "  mov  (%%esi),%%ecx \n"// mov ecx, [esi]
        "  mov  %%eax,(%%esi) \n"// mov [esi], eax
        "  mov  %%ecx,(%%edi) \n"// mov [edi], ecx
        "  add  $4, %%edi     \n"// add edi, 4
        "  sub  $4, %%esi     \n"// sub esi, 4
        "  cmp  %%esi, %%edi  \n"// cmp edi, esi
        "   jbe vf32lop2      \n"
        "  pop  %%edi         \n"// pop edi
        "  pop  %%esi         \n"// pop esi
        "  add  %%ebx, %%esi  \n"// add esi, ebx
        "  add  %%ebx, %%edi  \n"// add edi, ebx
        "  dec  %%edx         \n"
        "   jnz vf32lop       \n"
        :
        : "D" ((char *)buffer+lineOfs[0]), "S" (maxX*4),// %0, %1
        "b" (xRes), "d" (maxY+1)// %2, %3
      );
    break;
    case 3:
      height = maxY + 1;
      __asm__ __volatile__(
        "  add  %%edi, %%esi   \n"     // add esi, edi
        "vf24lop:              \n"
        "  push %%esi          \n"// push esi
        "  push %%edi          \n"// push edi
        "vf24lop2:             \n"
        "  mov  (%%edi),%%ax   \n"// mov ax, [edi]
        "  mov  2(%%edi),%%dl  \n"// mov dl, [edi+2]
        "  mov  (%%esi),%%cx   \n"// mov cx, [esi]
        "  mov  2(%%esi),%%dh  \n"// mov dh, [esi+2]
        "  mov  %%ax,(%%esi)   \n"// mov [esi], ax
        "  mov  %%dl,2(%%esi)  \n"// mov [esi+2], dl
        "  mov  %%cx,(%%edi)   \n"// mov [edi], cx
        "  mov  %%dh,2(%%edi)  \n"// mov [edi+2], dh
        "  add  $3, %%edi      \n"// add edi, 3
        "  sub  $3, %%esi      \n"// sub esi, 3
        "  cmp  %%esi, %%edi   \n"// cmp edi, esi
        "   jbe vf24lop2       \n"
        "  pop  %%edi          \n"// pop edi
        "  pop  %%esi          \n"// pop esi
        "  add  %%ebx, %%esi   \n"// add esi, ebx
        "  add  %%ebx, %%edi   \n"// add edi, ebx
        "  decl %3             \n"// dec height
        "   jnz vf24lop        \n"
        :
        : "D" ((char *)buffer+lineOfs[0]), "S" (maxX*3),// %0, %1
        "b" (xRes), "m" (height)// %2, %3
      );
    break;
    case 2:
      __asm__ __volatile__(
        "  add  %%edi, %%esi  \n"     // add esi, edi
        "vf16lop:             \n"
        "  push %%esi         \n"// push esi
        "  push %%edi         \n"// push edi
        "vf16lop2:            \n"
        "  mov  (%%edi),%%ax  \n"// mov ax, [edi]
        "  mov  (%%esi),%%cx  \n"// mov cx, [esi]
        "  mov  %%ax,(%%esi)  \n"// mov [esi], ax
        "  mov  %%cx,(%%edi)  \n"// mov [edi], cx
        "  add  $2, %%edi     \n"// add edi, 2
        "  sub  $2, %%esi     \n"// sub esi, 2
        "  cmp  %%esi, %%edi  \n"// cmp edi, esi
        "   jbe vf16lop2      \n"
        "  pop  %%edi         \n"// pop edi
        "  pop  %%esi         \n"// pop esi
        "  add  %%ebx, %%esi  \n"// add esi, ebx
        "  add  %%ebx, %%edi  \n"// add edi, ebx
        "  dec  %%edx         \n"
        "   jnz vf16lop       \n"
        :
        : "D" ((char *)buffer+lineOfs[0]), "S" (maxX*2),// %0, %1
        "b" (xRes), "d" (maxY+1)// %2, %3
      );
    break;
    case 1:
      __asm__ __volatile__(
        "  add  %%edi, %%esi  \n"     // add esi, edi
        "vf8lop:             \n"
        "  push %%esi         \n"// push esi
        "  push %%edi         \n"// push edi
        "vf8lop2:             \n"
        "  mov  (%%edi),%%al  \n"// mov al, [edi]
        "  mov  (%%esi),%%ah  \n"// mov ah, [esi]
        "  mov  %%al,(%%esi)  \n"// mov [esi], al
        "  mov  %%ah,(%%edi)  \n"// mov [edi], ah
        "  inc  %%edi         \n"// inc edi
        "  dec  %%esi         \n"// dec esi
        "  cmp  %%esi, %%edi  \n"// cmp edi, esi
        "   jbe vf8lop2       \n"
        "  pop  %%edi         \n"// pop edi
        "  pop  %%esi         \n"// pop esi
        "  add  %%ebx, %%esi  \n"// add esi, ebx
        "  add  %%ebx, %%edi  \n"// add edi, ebx
        "  dec  %%edx         \n"
        "   jnz vf8lop        \n"
        :
        : "D" ((char *)buffer+lineOfs[0]), "S" (maxX),// %0, %1
        "b" (xRes), "d" (maxY+1)// %2, %3
      );
    break;
  } // switch
  return;
} // ogSurface::ogVFlip

void ogSurface::ogVLine(int32 x, int32 y1, int32 y2, uInt32 colour) {
  int32 tmp;
  uInt8 r, g, b, a;

  if (!ogAvail())
    return;
  if ((uInt32) x > maxX)
    return;

  if (y1 > y2) {
    tmp = y1;
    y1 = y2;
    y2 = tmp;
  } // if

  if (y1 < 0)
    y1 = 0;
  if (y2 > (int32) maxY)
    y2 = maxY;
  if (y2 < y1)
    return;

  if (ogIsBlending()) {

    ogUnpack(colour, r, g, b, a);

    if (a == 0)
      return;

    if (a != 255) {
      for (tmp = y1; tmp <= y2; tmp++)
        RawSetPixel(x, tmp, r, g, b, a);
      return;
    } // if

  } // if blending

  switch (bytesPerPix) {
    case 4:
      __asm__ __volatile__(
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  shl  $2, %%ebx     \n"//  shl      ebx, 2  - pix size
        "  mov  %6, %%esi     \n"//  mov      esi, y1
        "  sub  %%esi, %%ecx  \n"//  sub      ecx, esi
        "  add  %%ebx, %%edi  \n"//  add      edi, ebx
        "  inc  %%ecx         \n"//  inc      ecx
        "vLlop32:             \n"
        "  mov  %%eax, (%%edi)\n"//  mov      [edi], eax
        "  add  %%edx, %%edi  \n"//  add      edi, edx
        "  dec  %%ecx         \n"//  dec      ecx
        "   jnz vLlop32       \n"
        :
        : "D" (buffer), "S" (lineOfs[y1]),// %0, %1
        "a" (colour), "b" (x),// %2, %3
        "c" (y2), "d" (xRes),// %4, %5
        "m" (y1)// %6
      );
    break;
    case 3:
      __asm__ __volatile__(
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  mov  %%ebx, %%esi  \n"//  mov      esi, ebx - pix size
        "  add  %%ebx, %%ebx  \n"//  add      ebx, ebx - pix size
        "  add  %%esi, %%ebx  \n"//  add      ebx, esi - pix size
        "  mov  %6, %%esi     \n"//  mov      esi, y1
        "  sub  %%esi, %%ecx  \n"//  sub      ecx, esi
        "  add  %%ebx, %%edi  \n"//  add      edi, ebx
        "  inc  %%ecx         \n"//  inc      ecx
        "  mov  %%eax, %%ebx  \n"//  mov      ebx, eax
        "  shr  $16, %%ebx    \n"//  shr      ebx, 16
        "vLlop24:             \n"
        "  mov  %%ax, (%%edi) \n"//  mov      [edi], eax
        "  mov  %%bl, 2(%%edi)\n"//  mov      [edi+2], bl
        "  add  %%edx, %%edi  \n"//  add      edi, edx
        "  dec  %%ecx         \n"//  dec      ecx
        "   jnz vLlop24       \n"
        :
        : "D" (buffer), "S" (lineOfs[y1]),// %0, %1
        "a" (colour), "b" (x),// %2, %3
        "c" (y2), "d" (xRes),// %4, %5
        "m" (y1)// %6
      );
    break;
    case 2:
      __asm__ __volatile__(
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  add  %%ebx, %%ebx  \n"//  add      ebx, ebx - pix size
        "  mov  %6, %%esi     \n"//  mov      esi, y1
        "  sub  %%esi, %%ecx  \n"//  sub      ecx, esi
        "  add  %%ebx, %%edi  \n"//  add      edi, ebx
        "  inc  %%ecx         \n"//  inc      ecx
        "vLlop16:             \n"
        "  mov  %%ax, (%%edi) \n"//  mov      [edi], ax
        "  add  %%edx, %%edi  \n"//  add      edi, edx
        "  dec  %%ecx         \n"//  dec      ecx
        "   jnz vLlop16       \n"
        :
        : "D" (buffer), "S" (lineOfs[y1]),// %0, %1
        "a" (colour), "b" (x),// %2, %3
        "c" (y2), "d" (xRes),// %4, %5
        "m" (y1)// %6
      );
    break;
    case 1:
      __asm__ __volatile__(
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  mov  %6, %%esi     \n"//  mov      esi, y1
        "  sub  %%esi, %%ecx  \n"//  sub      ecx, esi
        "  add  %%ebx, %%edi  \n"//  add      edi, ebx
        "  inc  %%ecx         \n"//  inc      ecx
        "vLlop8:              \n"
        "  mov  %%al, (%%edi) \n"//  mov      [edi], al
        "  add  %%edx, %%edi  \n"//  add      edi, edx
        "  dec  %%ecx         \n"//  dec      ecx
        "   jnz vLlop8        \n"
        :
        : "D" (buffer), "S" (lineOfs[y1]),// %0, %1
        "a" (colour), "b" (x),// %2, %3
        "c" (y2), "d" (xRes),// %4, %5
        "m" (y1)// %6
      );
    break;
  } // switch
  return;
} // ogSurface::ogVLine

ogSurface::~ogSurface(void) {

  if (dataState == ogOwner) {
    delete[] pal;
    delete[] lineOfs;
    delete attributes;
#ifdef __UBIXOS_KERNEL__
    kfree(buffer);
#else
    free(buffer);
#endif
  }  // if datastate

  pal = NULL;
  lineOfs = NULL;
  buffer = NULL;
  attributes = NULL;
  bSize = 0;
  lSize = 0;
  dataState = ogNone;
  return;
} // ogSurface::~ogSurface

