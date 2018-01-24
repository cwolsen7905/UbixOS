#include "objgfx.h"
#include "ogPixCon.h"

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

uInt32 ogPixCon::ConvPix(uInt32 pixel)
{
    /*
	_asm {
        xor     ebx, ebx                  ; // ebx <- 0
        xor     edi, edi                  ; // edi <- 0

		mov		eax, this

		mov     edx, [eax + ogPixCon::srcMasker]
        mov     ecx, [eax + ogPixCon::srcShifter]   ; // ecx <- src shifter
        mov     eax, [eax + ogPixCon::dstShifter]   ; // eax <- dst shifter
        mov     esi, pixel                ; // esi <- pixel to convert

        push    eax                       ; // save the dest shifter for later

        and     esi, edx                  ; // esi <- esi & srcMasker
        xor     eax, eax                  ; // eax <- 0
        xor     edx, edx                  ; // edx <- 0

        shrd    eax, esi, cl              ; // copy the 1st channel
        shr     esi, cl                   ; // shift the source
        mov     cl, ch                    ; // load next shifter
        shrd    ebx, esi, cl              ; // copy the 2nd channel
        shr     esi, cl                   ; // shift the source
        shr     ecx, 16                   ; // load the next shifter
        shrd    edx, esi, cl              ; // copy the 3rd channel
        shr     esi, cl                   ; // shift the source
        mov     cl, ch                    ; // load the next shifter
        shrd    edi, esi, cl              ; // copy the 4th channel

        pop     ecx                       ; // restore the dest shifter

        shr     eax, cl                   ; // shift 1st src chan to dest pos
        shr     ecx, 8                    ; // load next shifter
        shr     ebx, cl                   ; // shift 2nd src chan to dest pos
        shr     ecx, 8                    ; // load next shifter
        shr     edx, cl                   ; // shift 3rd src chan to dest pos
        shr     ecx, 8                    ; // load next shifter
        shr     edi, cl                   ; // shift 4th src chan to dest pos

        or      eax, ebx                  ; // combine 1st and 2nd channels
        or      edx, edi                  ; // combine 3rd and 4th channels
        nop
        or      eax, edx                  ; // combine all to form new pixel
	} // _asm
*/
	return 0;
} // uInt32 ogPixCon::ConvPix()

