#include <objgfx/objgfx.h>
#include <objgfx/ogPixCon.h>

// ogPixCon constructor
ogPixCon::ogPixCon(ogPixelFmt srcPixFmt, ogPixelFmt dstPixFmt) {
  uInt8 channelIdx[4];
  uInt8 srcFieldSize[4];
  uInt8 srcFieldPos[4];
  uInt8 dstShifters[4];

  uInt8 tmpb;
  int32 i, j;

  channelIdx[0] = 0;
  channelIdx[1] = 1;
  channelIdx[2] = 2;
  channelIdx[3] = 3;

  srcFieldSize[0] = srcPixFmt.alphaMaskSize;
  srcFieldSize[1] = srcPixFmt.redMaskSize;
  srcFieldSize[2] = srcPixFmt.greenMaskSize;
  srcFieldSize[3] = srcPixFmt.blueMaskSize;
 
  srcFieldPos[0] = srcPixFmt.alphaFieldPosition;
  srcFieldPos[1] = srcPixFmt.redFieldPosition;
  srcFieldPos[2] = srcPixFmt.greenFieldPosition;
  srcFieldPos[3] = srcPixFmt.blueFieldPosition;

  /*
   * The dest shifters are 32-(fieldPosition+fieldSize).  For things like
   * 24bpp where there is no alpha, the field position will be 0, and the
   * field size will be 0. 32-(0+0) is 32.. and when the shift takes place
   * the 32 will turn into a 0 and the shift will do nothing
   */

  dstShifters[0] = 32-(dstPixFmt.alphaFieldPosition+dstPixFmt.alphaMaskSize);
  dstShifters[1] = 32-(dstPixFmt.redFieldPosition+dstPixFmt.redMaskSize);
  dstShifters[2] = 32-(dstPixFmt.greenFieldPosition+dstPixFmt.greenMaskSize);
  dstShifters[3] = 32-(dstPixFmt.blueFieldPosition+dstPixFmt.blueMaskSize);

  i = srcPixFmt.redMaskSize - dstPixFmt.redMaskSize;
  if (i>0) 
    srcMasker = ogPixelFmt::OG_MASKS[dstPixFmt.redMaskSize] << (srcPixFmt.redFieldPosition+i);
  else
    srcMasker = ogPixelFmt::OG_MASKS[srcPixFmt.redMaskSize] << srcPixFmt.redFieldPosition;

  i = srcPixFmt.greenMaskSize - dstPixFmt.greenMaskSize;
  if (i>0) 
    srcMasker += ogPixelFmt::OG_MASKS[dstPixFmt.greenMaskSize] << (srcPixFmt.greenFieldPosition+i);
  else
    srcMasker += ogPixelFmt::OG_MASKS[srcPixFmt.greenMaskSize] << srcPixFmt.greenFieldPosition;

  i = srcPixFmt.blueMaskSize - dstPixFmt.blueMaskSize;
  if (i>0) 
    srcMasker += ogPixelFmt::OG_MASKS[dstPixFmt.blueMaskSize] << (srcPixFmt.blueFieldPosition+i);
  else
    srcMasker += ogPixelFmt::OG_MASKS[srcPixFmt.blueMaskSize] << srcPixFmt.blueFieldPosition;

  i = srcPixFmt.alphaMaskSize - dstPixFmt.alphaMaskSize;
  if (i>0) 
    srcMasker += ogPixelFmt::OG_MASKS[dstPixFmt.alphaMaskSize] << (srcPixFmt.alphaFieldPosition+i);
  else
    srcMasker += ogPixelFmt::OG_MASKS[srcPixFmt.alphaMaskSize] << srcPixFmt.alphaFieldPosition;

  /*
   * sort in descending order based on srcFieldPos (oth field will hold
   * highest position value)
   */
 
  for (i = 1; i < 4; i++ ) 
    for (j = 0; j < i; j++) {
      if (srcFieldPos[j] < srcFieldPos[i]) {
        tmpb = srcFieldPos[j];
        srcFieldPos[j] = srcFieldPos[i];
        srcFieldPos[i] = tmpb;
        
        tmpb = srcFieldSize[j];
        srcFieldSize[j] = srcFieldSize[i];
        srcFieldSize[i] = tmpb;
        
        tmpb = channelIdx[j];
        channelIdx[j] = channelIdx[i];
        channelIdx[i] = tmpb;
      } // if
    } // for j

  srcShifter = ((srcFieldSize[0] << 24) |
                (srcFieldSize[1] << 16) |
                (srcFieldSize[2] << 8) |
                (srcFieldSize[3]));

  dstShifter = ((dstShifters[channelIdx[0]] << 24) |
                (dstShifters[channelIdx[1]] << 16) |
                (dstShifters[channelIdx[2]] << 8) |
                (dstShifters[channelIdx[3]]));
  return;
} // ogPixCon::ogPixCon()

/*
 * ConvPix — remap a pixel from the source format to the destination format.
 *
 * srcMasker keeps only the meaningful source channel bits.  srcShifter packs
 * the four source field SIZES (byte3=field0 .. byte0=field3, in srcFieldPos
 * descending order) and dstShifter packs the matching destination shift amounts
 * (dstShifters[channelIdx[i]]).  Each field is extracted low-to-high, left-
 * aligned into a 32-bit lane, then shifted down into its destination position.
 *
 * The i386 build keeps the original hand-written x86 assembly (proven, and the
 * shrd/shr sequence is exactly this algorithm); every other arch uses the
 * portable C reimplementation below, which is bit-for-bit equivalent.
 */
#if defined(__i386__)
uint32_t
ogPixCon::ConvPix(uint32_t pixel) {
  __asm__ __volatile__(
       "  xor   %%ebx, %%ebx       \n"    // xor     ebx, ebx
       "  xor   %%edi, %%edi       \n"    // xor     edi, edi
       "                           \n"
       "  push  %%eax              \n"    // push    eax
       "                           \n"
       "  and   %%edx, %%esi       \n"    // and     esi, edx
       "  xor   %%eax, %%eax       \n"    // xor     eax, eax
       "  xor   %%edx, %%edx       \n"    // xor     edx, edx
       "                           \n"
       "  shrdl %%cl, %%esi, %%eax \n"    // shrd    eax, esi, cl
       "  shr   %%cl, %%esi        \n"    // shr     esi, cl
       "  mov   %%ch, %%cl         \n"    // mov     cl, ch
       "  shrdl %%cl, %%esi, %%ebx \n"    // shrd    ebx, esi, cl
       "  shr   %%cl, %%esi        \n"    // shr     esi, cl
       "  shr   $16, %%ecx         \n"    // shr     ecx, 16
       "  shrdl %%cl, %%esi, %%edx \n"    // shrd    edx, esi, cl
       "  shr   %%cl, %%esi        \n"    // shr     esi, cl
       "  mov   %%ch, %%cl         \n"    // mov     cl, ch
       "  shrdl %%cl, %%esi, %%edi \n"    // shrd    edi, esi, cl
       "                           \n"
       "  pop   %%ecx              \n"    // pop     ecx
       "                           \n"
       "  shr   %%cl, %%eax        \n"    // shr     eax, cl
       "  shr   $8, %%ecx          \n"    // shr     ecx, 8
       "  shr   %%cl, %%ebx        \n"    // shr     ebx, cl
       "  shr   $8, %%ecx          \n"    // shr     ecx, 8
       "  shr   %%cl, %%edx        \n"    // shr     edx, cl
       "  shr   $8, %%ecx          \n"    // shr     ecx, 8
       "  shr   %%cl, %%edi        \n"    // shr     edi, cl
       "                           \n"
       "  or    %%ebx, %%eax       \n"    // or      eax, ebx
       "  or    %%edi, %%edx       \n"    // or      edx, edi
       "  nop                      \n"    // nop
       "  or    %%edx, %%eax       \n"    // or      eax, edx

  : "=a" (pixel)                        // %0
  : "S" (pixel), "d" (srcMasker),       // %1, %2
    "c" (srcShifter), "a" (dstShifter)  // %3, %4
 //   "ecx" (srcShifter), "eax" (dstShifter)    // %2, %3
  );
  return pixel;
}; // ogPixCon::ConvPix
#else
uint32_t
ogPixCon::ConvPix(uint32_t pixel) {
  uint32_t src = pixel & srcMasker;
  uint8_t  sz0 = (uint8_t)(srcShifter >> 24);
  uint8_t  sz1 = (uint8_t)(srcShifter >> 16);
  uint8_t  sz2 = (uint8_t)(srcShifter >> 8);
  uint8_t  sz3 = (uint8_t)(srcShifter);
  uint8_t  d0  = (uint8_t)(dstShifter >> 24);
  uint8_t  d1  = (uint8_t)(dstShifter >> 16);
  uint8_t  d2  = (uint8_t)(dstShifter >> 8);
  uint8_t  d3  = (uint8_t)(dstShifter);

  /* Extract each field from the low bits of src, left-aligned into its lane
   * (a size-0 field yields 0, matching the x86 shrd-by-0 -> dest-unchanged). */
  uint32_t f3 = sz3 ? (src << (32 - sz3)) : 0u; src >>= sz3;
  uint32_t f2 = sz2 ? (src << (32 - sz2)) : 0u; src >>= sz2;
  uint32_t f1 = sz1 ? (src << (32 - sz1)) : 0u; src >>= sz1;
  uint32_t f0 = sz0 ? (src << (32 - sz0)) : 0u;

  return (f3 >> d3) | (f2 >> d2) | (f1 >> d1) | (f0 >> d0);
}; // ogPixCon::ConvPix
#endif

