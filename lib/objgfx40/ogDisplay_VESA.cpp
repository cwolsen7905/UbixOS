#include "objgfx40.h"
#include "defpal.inc"
#include "ogDisplay_VESA.h"
#include <go32.h>  // for __tb
#include <dpmi.h>
#include <sys/movedata.h>
#include <sys/farptr.h>
#include <pc.h>
#include <string.h>

/*
 *
 *              ogDisplay methods
 *
 */

void 
initVESAMode(uInt16 mode) {
   __dpmi_regs regs;
   regs.x.ax = 0x4f02;
   regs.x.bx = mode;
   __dpmi_int(0x10, &regs);
   return;
}

ogDisplay_VESA::ogDisplay_VESA(void) : ogSurface() {
  pages[0] = pages[1] = NULL;
  activePage = visualPage = 0;
  inGraphics = false;
  VESAInfo = new ogVESAInfo;
  modeInfo = new ogModeInfo;
  pal = new ogRGBA8[256];
  attributes = new ogAttributes();

  GetVESAInfo();
  
  screenSelector = __dpmi_allocate_ldt_descriptors(1);
  
  return;
} // ogDisplay_VESA::ogDisplay_VESA

uInt32 
ogDisplay_VESA::RawGetPixel(uInt32 x, uInt32 y) {
  uInt32 result;
  switch (bytesPerPix) {
   case 4:
    __asm__ __volatile__(
        "  push %%ds            \n"  // push    ds
        "  mov  %%ax, %%ds      \n"  // mov     ds, ax (screenSelector)
        "  shl  $2, %%ecx       \n"  // shl     ecx, 2 {adjust for pixel size}
     //  "  add  (%%esi,%%ebx,4), %%edi \n" //  add     edi, [esi + ebx*4]
        "  add  %%esi, %%edi    \n"  // add     edi, esi
        "  add  %%ecx, %%edi    \n"  // add     edi, ecx
        "  mov  (%%edi),%%eax   \n"  // eax,word ptr [edi]
        "  mov  %%eax, %4       \n"  // mov     result, eax
        "  pop  %%ds            \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),               // %0, %1
          "a" (screenSelector), "c" (x), "m" (result)  // %2, %3, %4
       );
   break;
  case 3:
    __asm__ __volatile__(
        "  push %%ds            \n"  // push    ds
        "  mov  %%ax, %%ds      \n"  // mov     ds, ax (screenSelector)
        "  mov  %%ecx, %%eax    \n"  // mov     eax, ecx  - adjust for pixel size
        "  add  %%ecx, %%ecx    \n"  // add     ecx, ecx  - adjust for pixel size
        "  add  %%eax, %%ecx    \n"  // add     ecx, eax  - adjust for pixel size
        "  add  %%esi, %%edi    \n"  // add     edi, esi
    //   "  add  (%%esi,%%ebx,4), %%edi \n" //  add     edi, [esi + ebx*4]
        "  add  %%ecx, %%edi    \n"  // add     edi, ecx
        "  movzwl (%%edi),%%eax \n"  // movzx   edx,word ptr [edi]
        "  xor  %%eax, %%eax    \n"
        "  mov  2(%%edi), %%al  \n"  // mov     al, [edi+2]
        "  shl  $16, %%eax      \n"  // shl     eax, 16
        "  mov  (%%edi), %%ax   \n"  // mov     ax, [edi]
        "  mov  %%eax, %4       \n"  // mov     result, eax
        "  pop  %%ds            \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),                // %0, %1
          "a" (screenSelector), "c" (x), "m" (result)   // %2, %3, %4
       );
    break;
  case 2:
    __asm__ __volatile__(
        "  push %%ds            \n"  // push    ds
        "  mov  %%ax, %%ds      \n"  // mov     ds, ax (screenSelector)
        "  add  %%esi, %%edi    \n"  // add     edi, esi
        "  add  %%ecx, %%ecx    \n"  // add     ecx, ecx {adjust for pixel size}
        "  add  %%ecx, %%edi    \n"  // add     edi, ecx
        "  movzwl (%%edi),%%eax \n"  // movzx   edx,word ptr [edi]
        "  mov  %%eax, %4       \n"  // mov     result, eax
        "  pop  %%ds            \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),                // %0, %1
          "a" (screenSelector), "c" (x), "m" (result)   // %2, %3, %4
       );
    break;
  case 1:
    __asm__ __volatile__(
        "  push %%ds            \n"  // push    ds
        "  mov  %%ax, %%ds      \n"  // mov     ds, ax (screenSelector)
        "  add  %%esi, %%edi    \n"  // add     edi, esi
        "  add  %%ecx, %%edi    \n"  // add     edi, ecx
        "  movzbl (%%edi),%%eax \n"  // movzx   edx,byte ptr [edi]
        "  mov  %%eax, %4       \n"  // mov     result, eax
        "  pop  %%ds            \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),                // %0, %1
          "a" (screenSelector), "c" (x), "m" (result)   // %2, %3, %4
       );
    break;
  } // switch
  return result;
} // ogDisplay_VESA::RawGetPixel

void
ogDisplay_VESA::RawSetPixel(uInt32 x, uInt32 y, uInt32 colour) {
  uInt32 newR, newG, newB, inverseA;
  uInt8 sR, sG, sB, sA;
  uInt8 dR, dG, dB;

  do {
    if (ogIsBlending()) {
      ogUnpack(colour, sR, sG, sB, sA);
      if (sA == 0) return;
      if (sA == 255) break;
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
        "  push %%ds          \n"          // push    ds
        "  mov  %%dx, %%ds    \n"          // mov     ds, dx
    
        "  shl  $2, %%ecx     \n"  // shl     eax, 2 {adjust for pixel size}
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%eax, (%%edi) \n" // mov     [edi], eax
        "  pop  %%ds          \n"          // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "c" (x), "a" (colour),                    // %2, %3
          "d" (screenSelector)                     // %4
    );
   break;
  case 3:
     __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
 //       "  add  (%%esi,%%ebx,4),%%edi \n" // add     edi, [esi + ebx * 4]
        "  push %%ds          \n"          // push    ds
        "  mov  %%dx, %%ds    \n"          // mov     ds, dx
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx {adjust for pixel size}
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx {adjust for pixel size}
    // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"  // mov     [edi], ax
        "  shr  $16, %%eax    \n"  // shr     eax, 16
        "  mov  %%al, 2(%%edi)\n"  // mov     [edi+2],al
        "  pop  %%ds          \n"          // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "c" (x), "a" (colour),                    // %2, %3
          "d" (screenSelector)                     // %4
    );
    break;
  case 2:
     __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
        "  push %%ds          \n"          // push    ds
        "  mov  %%dx, %%ds    \n"          // mov     ds, dx
        "  add  %%ecx, %%ecx  \n"  // add     ecx, ecx {adjust for pixel size}
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"  // mov     [edi], al
        "  pop  %%ds          \n"          // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "c" (x), "a" (colour),                    // %2, %3
          "d" (screenSelector)                     // %4
     );
     break;
  case 1:
    __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
    //    "  add  (%%esi,%%ebx,4), %%edi \n" // add     edi, [esi + ebx * 4]
        "  push %%ds          \n"          // push    ds
        "  mov  %%dx, %%ds    \n"          // mov     ds, dx
        "  add  %%esi, %%edi  \n"          // add     edi, esi
        "  add  %%ecx, %%edi  \n"          // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%al, (%%edi) \n"          // mov     [edi], al
        "  pop  %%ds          \n"          // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "c" (x), "a" (colour),                    // %2, %3
          "d" (screenSelector)                     // %4
   );
   break;
  } // switch
  return;
} // ogDisplay_VESA::RawSetPixel

void 
ogDisplay_VESA::RawSetPixel(uInt32 x, uInt32 y, uInt8 r, uInt8 g, uInt8 b, uInt8 a) {
  uInt32 newR, newG, newB, inverseA;
  uInt8 dR, dG, dB;
  uInt32 colour;

  do {
    if (ogIsBlending()) {
      if (a == 0) return;
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
    } else colour = ogPack(r, g, b, a);
  } while (false);
  switch (bytesPerPix) {
   case 4:
     __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
        "  push %%ds          \n"          // push    ds
        "  mov  %%dx, %%ds    \n"          // mov     ds, dx
    
        "  shl  $2, %%ecx     \n"  // shl     eax, 2 {adjust for pixel size}
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%eax, (%%edi) \n" // mov     [edi], eax
        "  pop  %%ds          \n"          // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "c" (x), "a" (colour),                    // %2, %3
          "d" (screenSelector)                     // %4
    );
   break;
  case 3:
     __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
 //       "  add  (%%esi,%%ebx,4),%%edi \n" // add     edi, [esi + ebx * 4]
        "  push %%ds          \n"          // push    ds
        "  mov  %%dx, %%ds    \n"          // mov     ds, dx
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx {adjust for pixel size}
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx {adjust for pixel size}
    // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"  // mov     [edi], ax
        "  shr  $16, %%eax    \n"  // shr     eax, 16
        "  mov  %%al, 2(%%edi)\n"  // mov     [edi+2],al
        "  pop  %%ds          \n"          // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "c" (x), "a" (colour),                    // %2, %3
          "d" (screenSelector)                     // %4
    );
    break;
  case 2:
     __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
        "  push %%ds          \n"          // push    ds
        "  mov  %%dx, %%ds    \n"          // mov     ds, dx
        "  add  %%ecx, %%ecx  \n"  // add     ecx, ecx {adjust for pixel size}
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"  // mov     [edi], al
        "  pop  %%ds          \n"          // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "c" (x), "a" (colour),                    // %2, %3
          "d" (screenSelector)                     // %4
     );
     break;
  case 1:
    __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
    //    "  add  (%%esi,%%ebx,4), %%edi \n" // add     edi, [esi + ebx * 4]
        "  push %%ds          \n"          // push    ds
        "  mov  %%dx, %%ds    \n"          // mov     ds, dx
        "  add  %%esi, %%edi  \n"          // add     edi, esi
        "  add  %%ecx, %%edi  \n"          // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%al, (%%edi) \n"          // mov     [edi], al
        "  pop  %%ds          \n"          // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "c" (x), "a" (colour),                    // %2, %3
          "d" (screenSelector)                     // %4
   );
   break;
  } // switch
  return;
} // ogDisplay_VESA::RawSetPixel

bool
ogDisplay_VESA::ogAvail(void) {
  return ( ((screenSelector != 0) || (buffer != NULL)) && (lineOfs != NULL));
} // ogDisplay_VESA::Avail

void
ogDisplay_VESA::GetModeInfo(uInt16 mode) {
  __dpmi_regs regs;
  memset(modeInfo, 0, sizeof(struct ogModeInfo));
  memset(&regs, 0, sizeof(regs));
  dosmemput(modeInfo, sizeof(struct ogModeInfo), __tb);
  
  int ofs, seg;

  seg = __tb;   // __tb is a buffer located in "dos memory". This buffer
  ofs = __tb;   // is used because the vesa driver cannot acces memory
                // above 1 MB (your program is most likely to be located above 1MB).

  ofs &= 0xffff;     // Make a real pointer (segment:offse = 16:16)
  seg &= 0xffff0000; // from the address of the buffer.
  seg >>= 4;

  regs.x.ax = 0x4f01; // Get the modeinfo of a certain vesa video mode,
                      // this is a structure which contains.
  regs.x.cx = mode;   // information needed by other functions below.
  regs.x.es = seg;
  regs.x.di = ofs;
  __dpmi_int(0x10, &regs);

  /* This info is located in dos memory, so it has to be moved to your 
   * program's address space.
   */
  dosmemget(__tb, sizeof(struct ogModeInfo), modeInfo); 
  return;

} // ogDisplay_VESA::GetModeInfo

void
ogDisplay_VESA::GetVESAInfo(void) {
  unsigned int seg, ofs;
  __dpmi_regs regs;
  if (NULL == VESAInfo) VESAInfo = new ogVESAInfo;
  if (NULL == VESAInfo) return;
  
  memset(VESAInfo, 0, sizeof(struct ogVESAInfo));
  memset(&regs, 0, sizeof(regs));
  VESAInfo->VBESignature[0] = 'V'; // First off initialize the structure.
  VESAInfo->VBESignature[1] = 'B';
  VESAInfo->VBESignature[2] = 'E';
  VESAInfo->VBESignature[3] = '2';

 /*

  Because VBE funtions operate in real mode, we first have to move the
  initialized structure to real-mode address space, so the structure can
  be filled by the vesa function in real mode.

 */
  dosmemput(VESAInfo, sizeof(struct ogVESAInfo), __tb);


  seg = __tb;     // Calculate real mode address of the buffer.
  ofs = __tb;
  ofs &= 0xffff;
  seg &= 0xffff0000;
  seg >>= 4;

  regs.x.ax = 0x4F00;
  regs.x.es = seg;
  regs.x.di = ofs;
 
  __dpmi_int(0x10, &regs); // Get vesa info.

  // Move the structure back to
  dosmemget(__tb, sizeof(struct ogVESAInfo), VESAInfo); 

  VESAInfo->OEMStringPtr = (VESAInfo->OEMStringPtr & 0xFFFF) +
    ((VESAInfo->OEMStringPtr & 0xFFFF0000) >> 12);

  VESAInfo->OEMVendorNamePtr= (VESAInfo->OEMVendorNamePtr& 0xFFFF) +
    ((VESAInfo->OEMVendorNamePtr& 0xFFFF0000) >> 12);

  VESAInfo->OEMProductNamePtr= (VESAInfo->OEMProductNamePtr& 0xFFFF) +
    ((VESAInfo->OEMProductNamePtr& 0xFFFF0000) >> 12);

  VESAInfo->OEMProductRevPtr= (VESAInfo->OEMProductRevPtr& 0xFFFF) +
    ((VESAInfo->OEMProductRevPtr& 0xFFFF0000) >> 12);

  VESAInfo->videoModePtr = ((VESAInfo->videoModePtr & 0xFFFF0000) >> 12) +
                           (VESAInfo->videoModePtr & 0xFFFF);
                             
  return;
} // ogDisplay_VESA::GetVESAInfo

uInt16
ogDisplay_VESA::FindMode(uInt32 _xRes, uInt32 _yRes, uInt32 _BPP) {
  uInt16 mode;

  if ((_xRes == 320) && (_yRes == 200) && (_BPP == 8)) return 0x13;

//  if ((VESAInfo==NULL) || (VESAInfo->videoModePtr==NULL)) return 0;
  if (modeInfo == NULL) return 0;

  for (mode = 0x100; mode < 0x1FF; mode++) {
    GetModeInfo(mode);
    if ((modeInfo->xRes >= _xRes) && (modeInfo->yRes >= _yRes) &&
        (modeInfo->bitsPerPixel == _BPP))
      return mode;
  }
  
  return 0;
} // ogDisplay_VESA::FindMode

void
ogDisplay_VESA::SetMode(uInt16 mode) {
  uInt32 size, count;
  __dpmi_meminfo mem_info;

  if (mode == 0x13) {

    xRes = 320;
    yRes = 200;
    maxX = 319;
    maxY = 199;
    BPP  = 8;
    bytesPerPix = 1;

    redFieldPosition = 0;
    greenFieldPosition = 0;
    blueFieldPosition = 0;
    alphaFieldPosition = 0;
    
    redShifter = 0;
    greenShifter = 0;
    blueShifter = 0;
    alphaFieldPosition = 0;
    
    mem_info.address = 0xA0000;
    mem_info.size = 64000;
    size = 63999;
    buffer = NULL;
    __dpmi_physical_address_mapping(&mem_info);
    __dpmi_set_segment_base_address(screenSelector, mem_info.address);
    __dpmi_set_segment_limit(screenSelector, size);
    __dpmi_set_descriptor_access_rights(screenSelector, 0x40F3);
    
  } else {
    buffer = NULL;
    mode |= 0x4000;  // attempt lfb
    GetModeInfo(mode);
    if (modeInfo->physBasePtr == 0) return;
    size = modeInfo->yRes*modeInfo->bytesPerLine;
    
    xRes = modeInfo->bytesPerLine;
    yRes = modeInfo->yRes;
    maxX = modeInfo->xRes-1;
    maxY = yRes-1;

    BPP = modeInfo->bitsPerPixel;
    bytesPerPix = (BPP + 7) >> 3;

    redFieldPosition   = modeInfo->redFieldPosition;
    greenFieldPosition = modeInfo->greenFieldPosition;
    blueFieldPosition  = modeInfo->blueFieldPosition;
    alphaFieldPosition = modeInfo->alphaFieldPosition;

    if (4 == bytesPerPix) {
      modeInfo->alphaMaskSize = 8;
      while ((alphaFieldPosition == redFieldPosition) ||
             (alphaFieldPosition == greenFieldPosition) ||
             (alphaFieldPosition == blueFieldPosition))
        alphaFieldPosition += 8;
    } // if
    
    redShifter   = 8-modeInfo->redMaskSize;
    greenShifter = 8-modeInfo->greenMaskSize;
    blueShifter  = 8-modeInfo->blueMaskSize;
    alphaShifter = 8-modeInfo->alphaMaskSize;

    if (modeInfo->alphaMaskSize != 0)
      alphaMasker = ~(OG_MASKS[modeInfo->alphaMaskSize] << alphaFieldPosition);
    else
      alphaMasker = ~0;


    mem_info.address = modeInfo->physBasePtr;
    mem_info.size = size;
    size = ((size+4095) >> 12);
    __dpmi_physical_address_mapping(&mem_info);
    __dpmi_set_segment_base_address(screenSelector, mem_info.address);
    __dpmi_set_segment_limit(screenSelector,size);
    __dpmi_set_descriptor_access_rights(screenSelector, 0xC0F3);
  } // else
  
  owner = this;
  dataState = ogAliasing;
  inGraphics = true;

  if ((lineOfs != NULL) && (lSize != 0)) delete [] lineOfs;
  lSize = yRes*sizeof(uInt32);
  lineOfs = new uInt32[yRes];;
  if (lineOfs == NULL) return;
  lineOfs[0] = 0;
  for (count=1; count<yRes; count++)
    lineOfs[count] = lineOfs[count-1]+xRes;
  initVESAMode(mode);

  if (pal == NULL) pal = new ogRGBA8[256];
  ogSetPalette(DEFAULT_PALETTE);

  if (1 == bytesPerPix) {
    pixFmtID = 0x08080808;
  } else {
    pixFmtID = (redFieldPosition) |
               (greenFieldPosition << 8) |
               (blueFieldPosition << 16) |
               (alphaFieldPosition << 24);
  } // else
  
  ogSetAntiAliasing(BPP > 8);
//  antiAlias=(BPP>8);
  if (BPP == 8) SetPal();
  ogClear(ogPack(0, 0, 0));
  return;
} // ogDisplay_VESA::SetMode

void
ogDisplay_VESA::SetPal(void) {
  if (bytesPerPix != 1) return;
  outportb(0x3c8,0);
  for (uInt32 c = 0; c < 256; c++) {
    outportb(0x3c9, pal[c].red >> 2);
    outportb(0x3c9, pal[c].green >> 2);
    outportb(0x3c9, pal[c].blue >> 2);
  } // for
  return;
} // ogDisplay_VESA::SetPal

void
ogDisplay_VESA::ogSetPalette(const ogRGBA8 newPal[256]) {
  ogSurface::ogSetPalette(newPal);
  SetPal();
  return;
} // ogDisplay_VESA::SetPalette;

bool
ogDisplay_VESA::ogAlias(ogSurface& src, uInt32 x1, uInt32 y1,
                      uInt32 x2, uInt32 y2) {

  ogSetLastError(ogNoAliasing);
  return false;
} // ogDisplay_VESA::Alias

void 
ogDisplay_VESA::ogClear(uInt32 colour) {
  uInt32 height = 0;
  uInt32 xx, yy;
  uInt8 r, g, b, a;
  
  if (!ogAvail()) return;

  do {
    if (ogIsBlending()) {
      ogUnpack(colour, r, g, b, a);
      if (a == 0) return;
      if (a == 255) break;
      for (yy = 0; yy <= maxY; yy++) 
        for (xx = 0; xx <= maxX; xx++) 
          RawSetPixel(xx, yy, r, g, b, a);
    } // if 
  } while (false);
  
  __asm__ __volatile__("cld\n");
  switch (bytesPerPix) {
   case 4:
      __asm__ __volatile__(
          "  push %%es           \n"      // push es
          "  mov %6, %%ax        \n"      // mov ax, screenSelector
          "  mov %%ax, %%es      \n"      // mov es, ax
          "  add (%%esi), %%edi  \n"      // add edi, [esi]
          "  mov %%ecx, %%esi    \n"      // mov esi, ecx
          "  inc %%edx           \n"      // inc edx (maxX)
          "  inc %%ebx           \n"      // inc ebx (maxY)
          "  mov %5, %%eax       \n"      // mov eax, colour
          "  mov %%edx, %%ecx    \n"      // mov ecx, edx
          "  shl $2, %%ecx       \n"      // shl ecx, 2
          "  sub %%ecx, %%esi    \n"      // sub esi, ecx // adjust for pix size
      "loop32:                   \n"
          "  mov %%edx, %%ecx    \n"      // mov ecx, edx
          " rep                  \n"
          " stosl                \n"      
          "  add %%esi, %%edi    \n"      // add edi, esi
          "  dec %%ebx           \n"
          "   jnz loop32         \n"
          "  pop %%es            \n"      // pop es
        :
        : "D" (buffer), "S" (lineOfs),           // %0, %1
          "b" (maxY), "c" (xRes), "d" (maxX),    // %2, %3, %4
          "m" (colour), "m" (screenSelector)    // %5, %6
     );
     break;
  case 3:
      __asm__ __volatile__(
          "  push %%es           \n"      // push es
          "  mov %7, %%ax        \n"      // mov ax, screenSelector
          "  mov %%ax, %%es      \n"      // mov es, ax
          "  add (%%esi), %%edi  \n"      // add edi, [esi]
          "  mov %%ecx, %%esi    \n"      // mov esi, ecx
          "  inc %%edx           \n"      // inc edx (maxX)
          "  inc %%ebx           \n"      // inc ebx (maxY)
          "  mov %5, %%eax       \n"      // mov eax, colour
          "  sub %%edx, %%esi    \n"      // sub esi, edx // adjust for pix size
          "  mov %%ebx, %6       \n"      // mov height, ebx
          "  sub %%edx, %%esi    \n"      // sub esi, edx // adjust for pix size
          "  mov %%eax, %%ebx    \n"      // mov ebx, eax
          "  sub %%edx, %%esi    \n"      // sub esi, edx // adjust for pix size
          "  shr $16, %%ebx      \n"      // shr ebx, 16
      "oloop24:                  \n"
          "  mov %%edx, %%ecx    \n"      // mov ecx, edx
      "iloop24:                  \n"
          "  mov %%ax,(%%edi)    \n"      // mov [edi],ax
          "  movb %%bl,2(%%edi)  \n"      // mov [edi+2],bl
          "  add $3, %%edi       \n"      // add edi, 3
          "  dec %%ecx           \n"      // dec ecx
          "   jnz iloop24        \n"
          "  add %%esi, %%edi    \n"      // add edi, esi
          "  decl %6             \n"      // dec height
          "   jnz oloop24        \n"
          "  pop %%es            \n"      // pop es
        :
        : "D" (buffer), "S" (lineOfs),           // %0, %1
          "b" (maxY), "c" (xRes), "d" (maxX),    // %2, %3, %4
          "m" (colour), "m" (height),            // %5, %6
          "m" (screenSelector)                  // %7
        );
      break;
   case 2:
      __asm__ __volatile__(
          "  push %%es           \n"      // push es
          "  mov %6, %%ax        \n"      // mov ax, screenSelector
          "  mov %%ax, %%es      \n"      // mov es, ax
          "  add (%%esi), %%edi  \n"      // add edi, [esi]
          "  mov %%ecx, %%esi    \n"      // mov esi, ecx
          "  inc %%edx           \n"      // inc edx (maxX)
          "  inc %%ebx           \n"      // inc ebx (maxY)
          "  sub %%edx, %%esi    \n"      // sub esi, edx
          "  mov %5, %%eax       \n"      // mov eax, colour
          "  sub %%edx, %%esi    \n"      // sub esi, edx // adjust for pix size
          "  mov %%ax, %%cx      \n"      // mov cx, ax
          "  shl $16, %%eax      \n"      // shl eax, 16
          "  mov %%cx, %%ax      \n"      // mov ax, cx
      "loop16:                   \n"
          "  mov %%edx, %%ecx    \n"      // mov ecx, edx
          "  shr $1, %%ecx       \n"      // shr ecx, 1
          " rep                  \n"
          " stosl                \n"      
          "  jnc noc16           \n"
          " stosw                \n"
      "noc16:                    \n"
          "  add %%esi, %%edi    \n"      // add edi, esi
          "  dec %%ebx           \n"
          "   jnz loop16         \n"
          "  pop %%es            \n"
        :
        : "D" (buffer), "S" (lineOfs),           // %0, %1
          "b" (maxY), "c" (xRes), "d" (maxX),    // %2, %3, %4
          "m" (colour), "m" (screenSelector)    // %5, %6
      );
      break;
   
    case 1:
      __asm__ __volatile__(
          "  push %%es           \n"      // push es
          "  mov %6, %%ax        \n"      // mov ax, screenSelector
          "  mov %%ax, %%es      \n"      // mov es, ax
          "  add (%%esi), %%edi  \n"      // add edi, [esi]
          "  mov %%ecx, %%esi    \n"      // mov esi, ecx
          "  inc %%edx           \n"      // inc edx (maxY)
          "  inc %%ebx           \n"      // inc ebx (maxX)
          "  mov %5, %%eax       \n"      // mov eax, colour
          "  sub %%edx, %%esi    \n"      // sub esi, edx
          "  mov %%al, %%ah      \n"      // mov ah, al
          "  mov %%ax, %%cx      \n"      // mov cx, ax
          "  shl $16, %%eax      \n"      // shl eax, 16
          "  mov %%cx, %%ax      \n"      // mov ax, cx
      "loop8:                    \n"
          "  push %%edx          \n"
          "  mov %%edx, %%ecx    \n"      // mov ecx, edx
          "  and $3, %%edx       \n"      // and edx, 3
          "  shr $2, %%ecx       \n"      // shr ecx, 2
          " rep                  \n"
          " stosl                \n"      
          "  mov %%edx, %%ecx    \n"      // mov ecx, edx
          " rep                  \n"
          " stosb                \n"      
          "  pop %%edx           \n"
          "  add %%esi, %%edi    \n"      // add edi, esi
          "  dec %%ebx           \n"
          "   jnz loop8          \n"
          "  pop %%es            \n"      // pop es
        :
        : "D" (buffer), "S" (lineOfs),           // %0, %1
          "b" (maxY), "c" (xRes), "d" (maxX),    // %2, %3, %4
          "m" (colour), "m" (screenSelector)    // %5, %6
      );
      break;
  } // switch
  return;
} // ogDisplay_VESA::Clear

void
ogDisplay_VESA::ogCopyLineTo(uInt32 dx, uInt32 dy, const void * src,
                           uInt32 size) {
  /*
   * ogCopyLineTo()
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
  movedata(_my_ds(),(uInt32)src,
           screenSelector,(uInt32)((uInt8*)buffer+(lineOfs[dy]+dx*bytesPerPix) ),
           size);
  
  return;
} // ogSurface::ogCopyLineTo

void
ogDisplay_VESA::ogCopyLineFrom(uInt32 sx, uInt32 sy, void * dest, uInt32 size) {
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
  movedata(screenSelector,(uInt32)((uInt8*)buffer+(lineOfs[sy]+sx*bytesPerPix ) ),
           _my_ds(),(uInt32)dest,
           size);

  return;
} // ogDisplay_VESA::ogCopyLineFrom

bool
ogDisplay_VESA::ogCreate(uInt32 _xRes, uInt32 _yRes, ogPixelFmt _pixFormat) {
  uInt16 mode;
  mode = FindMode(_xRes, _yRes, _pixFormat.BPP);
  if ((mode == 0) && ((_pixFormat.BPP==24) || (_pixFormat.BPP==32))) {
    if (_pixFormat.BPP==24) _pixFormat.BPP=32; else _pixFormat.BPP=24;
    mode = FindMode(_xRes,_yRes,_pixFormat.BPP);
  } // if
  if (mode!=0) SetMode(mode);
  return (mode!=0);
} // ogDisplay_VESA::ogCreate

bool
ogDisplay_VESA::ogClone(ogSurface& src) {
  ogSetLastError(ogNoCloning);
  return false;
} // ogDisplay_VESA::ogClone

void
ogDisplay_VESA::ogCopyPalette(ogSurface& src) {
  ogSurface::ogCopyPalette(src);
  SetPal();
  return;
} // ogDisplay_VESA::ogCopyPalette

uInt32 
ogDisplay_VESA::ogGetPixel(int32 x, int32 y) {
  uInt32 result;
  if (!ogAvail()) return ogGetTransparentColor();
  if (((uInt32)x>maxX) || ((uInt32)y>maxY)) 
    return ogGetTransparentColor();
  switch (bytesPerPix) {
   case 4:
    __asm__ __volatile__(
        "  push %%ds            \n"  // push    ds
        "  mov  %%ax, %%ds      \n"  // mov     ds, ax (screenSelector)
        "  shl  $2, %%ecx       \n"  // shl     ecx, 2 {adjust for pixel size}
     //  "  add  (%%esi,%%ebx,4), %%edi \n" //  add     edi, [esi + ebx*4]
        "  add  %%esi, %%edi    \n"  // add     edi, esi
        "  add  %%ecx, %%edi    \n"  // add     edi, ecx
        "  mov  (%%edi),%%eax   \n"  // eax,    dword ptr [edi]
        "  mov  %%eax, %4       \n"  // mov     result, eax
        "  pop  %%ds            \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),               // %0, %1
          "a" (screenSelector), "c" (x), "m" (result)  // %2, %3, %4
       );
    break;
  case 3:
    __asm__ __volatile__(
        "  push %%ds            \n"  // push    ds
        "  mov  %%ax, %%ds      \n"  // mov     ds, ax (screenSelector)
        "  mov  %%ecx, %%eax    \n"  // mov     eax, ecx  - adjust for pixel size
        "  add  %%ecx, %%ecx    \n"  // add     ecx, ecx  - adjust for pixel size
        "  add  %%eax, %%ecx    \n"  // add     ecx, eax  - adjust for pixel size
        "  add  %%esi, %%edi    \n"  // add     edi, esi
    //   "  add  (%%esi,%%ebx,4), %%edi \n" //  add     edi, [esi + ebx*4]
        "  add  %%ecx, %%edi    \n"  // add     edi, ecx
        "  movzwl (%%edi),%%eax \n"  // movzx   edx,word ptr [edi]
        "  xor  %%eax, %%eax    \n"
        "  mov  2(%%edi), %%al  \n"  // mov     al, [edi+2]
        "  shl  $16, %%eax      \n"  // shl     eax, 16
        "  mov  (%%edi), %%ax   \n"  // mov     ax, [edi]
        "  mov  %%eax, %4       \n"  // mov     result, eax
        "  pop  %%ds            \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),                // %0, %1
          "a" (screenSelector), "c" (x), "m" (result)   // %2, %3, %4
       );
    break;
   case 2:
    __asm__ __volatile__(
        "  push %%ds            \n"  // push    ds
        "  mov  %%ax, %%ds      \n"  // mov     ds, ax (screenSelector)
        "  add  %%esi, %%edi    \n"  // add     edi, esi
        "  add  %%ecx, %%ecx    \n"  // add     ecx, ecx {adjust for pixel size}
        "  add  %%ecx, %%edi    \n"  // add     edi, ecx
        "  movzwl (%%edi),%%eax \n"  // movzx   edx,word ptr [edi]
        "  mov  %%eax, %4       \n"  // mov     result, eax
        "  pop  %%ds            \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),                // %0, %1
          "a" (screenSelector), "c" (x), "m" (result)   // %2, %3, %4
       );
    break;
  case 1:
    __asm__ __volatile__(
        "  push %%ds            \n"  // push    ds
        "  mov  %%ax, %%ds      \n"  // mov     ds, ax (screenSelector)
        "  add  %%esi, %%edi    \n"  // add     edi, esi
        "  add  %%ecx, %%edi    \n"  // add     edi, ecx
        "  movzbl (%%edi),%%eax \n"  // movzx   edx,byte ptr [edi]
        "  mov  %%eax, %4       \n"  // mov     result, eax
        "  pop  %%ds            \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),                // %0, %1
          "a" (screenSelector), "c" (x), "m" (result)   // %2, %3, %4
       );
    break;
  } // switch
  return result;
} // ogDisplay_VESA::ogGetPixel

void *
ogDisplay_VESA::ogGetPtr(uInt32 x, uInt32 y) {
  return NULL;
} // ogDisplay_VESA::ogGetPtr

void
ogDisplay_VESA::ogHLine(int32 x1, int32 x2, int32 y, uInt32 colour) {
  int32 tmp;
  uInt8 r, g, b, a;

  if (!ogAvail()) return;
  if ((uInt32)y>maxY) return;
  if (x1 > x2) {
    tmp= x1;
    x1 = x2;
    x2 = tmp;
  } // if
  if (x1 < 0) x1 = 0;
  if (x2 > (int32)maxX) x2 = maxX;
  if (x2 < x1) return;

  if (ogIsBlending()) {
    ogUnpack(colour, r, g, b, a);
    if (a == 0) return;
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
        "  push %%es          \n"          //  push     es
        "  mov  %%dx, %%es    \n"          //  mov      es, dx
        "  sub  %%ebx, %%ecx  \n"          //  sub      ecx, ebx
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  inc  %%ecx         \n"
        "  shl  $2, %%ebx     \n"          //  shl      ebx, 2
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  rep                \n"
        "  stosl              \n"
        "  pop  %%es          \n"          //  pop      es
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "a" (colour), "b" (x1),                   // %2, %3
          "c" (x2), "d" (screenSelector)           // %4, %5
        );
    break;
  case 3:
    __asm__ __volatile__(
        "  push %%es          \n"          //  push     es
        "  mov  %%ax, %%es    \n"          //  mov      es, ax
        "  mov  %2, %%eax     \n"
        "  sub  %%ebx, %%ecx  \n"          //  sub      ecx, ebx
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  add  %%ebx, %%ebx  \n"          //  add      ebx, ebx - pix size
        "  inc  %%ecx         \n"          //  inc      ecx
        "  add  %%edx, %%ebx  \n"          //  add      ebx, edx - pix size
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  mov  %%eax, %%ebx  \n"          //  mov      ebx, eax
        "  shr  $16, %%ebx    \n"          //  shr      ebx, 16
    "hLlop24:                 \n"          
        "  mov  %%ax, (%%edi) \n"          //  mov      [edi], ax
        "  mov  %%bl, 2(%%edi)\n"          //  mov      [edi+2], bl
        "  add  $3, %%edi     \n"          //  add      edi, 3
        "  dec  %%ecx         \n"          //  dec      ecx
        "   jnz hLlop24       \n"
        "  pop  %%es          \n"          //  pop      es
        
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "m" (colour), "b" (x1),                   // %2, %3
          "c" (x2), "d" (x1), "a" (screenSelector) // %4, %5
    );
    break;
  case 2:
    __asm__ __volatile__(
        "  push %%es          \n"
        "  mov  %%dx, %%es    \n"          //  mov      es, dx
        "  sub  %%ebx, %%ecx  \n"          //  sub      ecx, ebx
        "  add  %%ebx, %%ebx  \n"          //  add      ebx, ebx - pix size
        "  inc  %%ecx         \n"          //  inc      ecx
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  xor  %%edx, %%edx  \n"          //  xor      edx, edx
        "  mov  %%ax, %%dx    \n"          //  mov      dx, ax
        "  shl  $16, %%eax    \n"          //  shl      eax, 16
        "  add  %%edx, %%eax  \n"          //  add      eax, edx

        "  shr  $1, %%ecx     \n"          //  shr      ecx, 1
        "  rep                \n"
        "  stosl              \n"
        "   jnc hLnoc16       \n"
        "  stosw              \n"
        "hLnoc16:             \n"
        "  pop  %%es          \n"
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "a" (colour), "b" (x1),                   // %2, %3
          "c" (x2), "d" (screenSelector)
    );
    break;
  case 1:
    __asm__ __volatile__(
        "  push %%es          \n"          //  push     es
        "  mov  %%dx, %%es    \n"          //  mov      es, dx
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  and  $0xff, %%eax  \n"          //  and      eax, 0ffh
        "  sub  %%ebx, %%ecx  \n"          //  sub      ecx, ebx
        "  mov  %%al, %%ah    \n"          //  mov      ah, al
        "  inc  %%ecx         \n"          //  inc      ecx
        "  mov  %%eax, %%ebx  \n"          //  mov      ebx, eax
        "  shl  $16, %%ebx    \n"          //  shl      ebx, 16
        "  add  %%ebx, %%eax  \n"          //  add      eax, ebx

        "  mov  %%ecx, %%edx  \n"          //  mov      edx, ecx
        "  mov  $4, %%ecx     \n"          //  mov      ecx, 4
        "  sub  %%edi, %%ecx  \n"          //  sub      ecx, edi
        "  and  $3, %%ecx     \n"          //  and      ecx, 3
        "  sub  %%ecx, %%edx  \n"          //  sub      edx, ecx
        "   jle LEndBytes     \n"
        "  rep                \n"          
        "  stosb              \n"
        "  mov  %%edx, %%ecx  \n"          //  mov      ecx, edx
        "  and  $3, %%edx     \n"          //  and      edx, 3
        "  shr  $2, %%ecx     \n"          //  shr      ecx, 2
        "  rep                \n"
        "  stosl              \n"
    "LEndBytes:               \n"
        "  add  %%edx, %%ecx  \n"          //  add      ecx, edx
        "  rep                \n"
        "  stosb              \n"
        "  pop  %%es          \n"
        :
        : "D" (buffer), "S" (lineOfs[y]),           // %0, %1
          "a" (colour), "b" (x1),                   // %2, %3
          "c" (x2), "d" (screenSelector)
       );
    break;
  } // switch
  return;
} // ogDisplay_VESA::ogHLine

bool
ogDisplay_VESA::ogLoadPalette(const char *palfile) {
  bool result;
  if ((result = ogSurface::ogLoadPalette(palfile))==true) SetPal();
  return result;
} // ogDisplay_VESA::ogLoadPalette

void
ogDisplay_VESA::ogSetPixel(int32 x, int32 y, uInt32 colour) {
  uInt32 newR, newG, newB, inverseA;
  uInt8 sR, sG, sB, sA;
  uInt8 dR, dG, dB;

  if (!ogAvail()) return;
  if (((uInt32)x > maxX) || ((uInt32)y > maxY)) return;

  do {
    if (ogIsBlending()) {
      ogUnpack(colour, sR, sG, sB, sA);
      if (sA == 0) return;
      if (sA == 255) break;
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
        "  push %%ds          \n"  // push    ds
        "  mov  %4, %%dx      \n"  // mov     dx, screenSelector
        "  mov  %%dx, %%ds    \n"  // mov     ds, dx
        "  shl  $2, %%ecx     \n"  // shl     eax, 2 {adjust for pixel size}
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%eax, (%%edi)\n"  // mov     [edi], eax
        "  pop  %%ds          \n"  // pop ds
        :
        : "D" (buffer), "S" (lineOfs[y]),              // %0, %1
          "c" (x), "a" (colour), "m" (screenSelector) // %2, %3, %4
    );
   break;
  case 3:
     __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
        "  push %%ds          \n"  // push    ds
        "  mov  %4, %%dx      \n"  // mov     dx, screenSelector
        "  mov  %%dx, %%ds    \n"  // mov     ds, dx
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx {adjust for pixel size}
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx {adjust for pixel size}
    // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"  // mov     [edi], ax
        "  shr  $16, %%eax    \n"  // shr     eax, 16
        "  mov  %%al, 2(%%edi)\n"  // mov     [edi+2],al
        "  pop  %%ds          \n"  // pop     ds
        :
        : "D" (buffer), "S" (lineOfs[y]),              // %0, %1
          "c" (x), "a" (colour), "m" (screenSelector) // %2, %3, %4
    );
    break;
  case 2:
     __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
        "  push %%ds          \n"
        "  mov  %4, %%dx      \n"
        "  mov  %%dx, %%ds    \n"

        "  add  %%ecx, %%ecx  \n"  // add     ecx, ecx {adjust for pixel size}
        "  add  %%esi, %%edi  \n"  // add     edi, esi
        "  mov  %3, %%eax     \n"  // mov     eax, colour
        "  add  %%ecx, %%edi  \n"  // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%ax, (%%edi) \n"  // mov     [edi], al
        "  pop  %%ds\n"
        
        :
        : "D" (buffer), "S" (lineOfs[y]),        // %0, %1
          "c" (x), "a" (colour), "m" (screenSelector) // %2, %3, %4
     );
     break;
  
  case 1:
    __asm__ __volatile__(
    // { Calculate offset, prepare the pixel to be drawn }
        "  push %%ds          \n"
        "  mov  %4, %%dx      \n"
        "  mov  %%dx, %%ds    \n"
        "  add  %%esi, %%edi  \n"          // add     edi, esi
        "  add  %%ecx, %%edi  \n"          // add     edi, ecx
    // { Draw the pixel }
        "  mov  %%al, (%%edi) \n"          // mov     [edi], al
        "  pop  %%ds\n"
        :
        : "D" (buffer), "S" (lineOfs[y]),              // %0, %1
          "c" (x), "a" (colour), "m" (screenSelector) // %2, %3, %4
   );
   break;
  } // switch
  return;
} // ogDisplay_VESA::ogSetPixel

void
ogDisplay_VESA::ogSetPalette(uInt8 colour, uInt8 red, uInt8 green, uInt8 blue) {
  if (pal == NULL) return;
  ogSurface::ogSetPalette(colour,red,green,blue);
  outportb(0x3c8, colour);
  outportb(0x3c9, red >> 2);
  outportb(0x3c9, green >> 2);
  outportb(0x3c9, blue >> 2);
  
  return;
} // ogDisplay_VESA::ogSetPalette

void
ogDisplay_VESA::ogSetPalette(uInt8 colour, uInt8 red, uInt8 green,
                                 uInt8 blue, uInt8 alpha) {
  if (pal == NULL) return;
  ogSurface::ogSetPalette(colour, red, green, blue, alpha);
  outportb(0x3c8, colour);
  outportb(0x3c9, red >> 2);
  outportb(0x3c9, green >> 2);
  outportb(0x3c9, blue >> 2);

  return;
} // ogDisplay_VESA::ogSetPalette

void
ogDisplay_VESA::ogVFlip(void) {
  if (!ogAvail()) return;
  
  switch (bytesPerPix) {
   case 4:
    __asm__ __volatile__(
        "  push %%ds          \n"     // push ds
        "  mov  %%ax, %%ds    \n"     // mov ds, ax        
        "  add  %%edi, %%esi  \n"     // add esi, edi
        "vf32lop:             \n"
        "  push %%esi         \n"     // push esi
        "  push %%edi         \n"     // push edi
        "vf32lop2:            \n"
        "  mov  (%%edi),%%eax \n"     // mov eax, [edi]
        "  mov  (%%esi),%%ecx \n"     // mov ecx, [esi]
        "  mov  %%eax,(%%esi) \n"     // mov [esi], eax
        "  mov  %%ecx,(%%edi) \n"     // mov [edi], ecx
        "  add  $4, %%edi     \n"     // add edi, 4
        "  sub  $4, %%esi     \n"     // sub esi, 4
        "  cmp  %%esi, %%edi  \n"     // cmp edi, esi
        "   jbe vf32lop2      \n"
        "  pop  %%edi         \n"     // pop edi
        "  pop  %%esi         \n"     // pop esi
        "  add  %%ebx, %%esi  \n"     // add esi, ebx
        "  add  %%ebx, %%edi  \n"     // add edi, ebx
        "  dec  %%edx         \n"
        "   jnz vf32lop       \n"
        "  pop  %%ds          \n"     // pop ds        
        :
        : "D" ((char *)buffer+lineOfs[0]), "S" (maxX*4),      // %0, %1
          "b" (xRes), "d" (maxY+1), "a" (screenSelector)     // %2, %3, %4
       );
    break;
  case 3:
    __asm__ __volatile__(
        "  push %%ds          \n"     // push ds
        "  mov  %%ax, %%ds    \n"     // mov ds, ax        
        "  add  %%edi, %%esi   \n"     // add esi, edi
        "vf24lop:              \n"
        "  push %%esi          \n"     // push esi
        "  push %%edi          \n"     // push edi
        "vf24lop2:             \n"
        "  mov  (%%edi),%%ax   \n"     // mov ax, [edi]
        "  mov  2(%%edi),%%dl  \n"     // mov dl, [edi+2]
        "  mov  (%%esi),%%cx   \n"     // mov cx, [esi]
        "  mov  2(%%esi),%%dh  \n"     // mov dh, [esi+2]
        "  mov  %%ax,(%%esi)   \n"     // mov [esi], ax
        "  mov  %%dl,2(%%esi)  \n"     // mov [esi+2], dl
        "  mov  %%cx,(%%edi)   \n"     // mov [edi], cx
        "  mov  %%dh,2(%%edi)  \n"     // mov [edi+2], dh
        "  add  $3, %%edi      \n"     // add edi, 3
        "  sub  $3, %%esi      \n"     // sub esi, 3
        "  cmp  %%esi, %%edi   \n"     // cmp edi, esi
        "   jbe vf24lop2       \n"
        "  pop  %%edi          \n"     // pop edi
        "  pop  %%esi          \n"     // pop esi
        "  add  %%ebx, %%esi   \n"     // add esi, ebx
        "  add  %%ebx, %%edi   \n"     // add edi, ebx
        "  decl %3             \n"     // dec height
        "   jnz vf24lop        \n"
        "  pop  %%ds          \n"     // pop ds        
        :
        : "D" ((char *)buffer+lineOfs[0]), "S" (maxX*3),     // %0, %1
          "b" (xRes), "d" (maxY+1), "a" (screenSelector)     // %2, %3, %4        
       );
    break;
  case 2:
    __asm__ __volatile__(
        "  push %%ds          \n"     // push ds
        "  mov  %%ax, %%ds    \n"     // mov ds, ax        
        "  add  %%edi, %%esi  \n"     // add esi, edi
        "vf16lop:             \n"
        "  push %%esi         \n"     // push esi
        "  push %%edi         \n"     // push edi
        "vf16lop2:            \n"
        "  mov  (%%edi),%%ax  \n"     // mov ax, [edi]
        "  mov  (%%esi),%%cx  \n"     // mov cx, [esi]
        "  mov  %%ax,(%%esi)  \n"     // mov [esi], ax
        "  mov  %%cx,(%%edi)  \n"     // mov [edi], cx
        "  add  $2, %%edi     \n"     // add edi, 2
        "  sub  $2, %%esi     \n"     // sub esi, 2
        "  cmp  %%esi, %%edi  \n"     // cmp edi, esi
        "   jbe vf16lop2      \n"
        "  pop  %%edi         \n"     // pop edi
        "  pop  %%esi         \n"     // pop esi
        "  add  %%ebx, %%esi  \n"     // add esi, ebx
        "  add  %%ebx, %%edi  \n"     // add edi, ebx
        "  dec  %%edx         \n"
        "   jnz vf16lop       \n"
        "  pop  %%ds          \n"     // pop ds        
        :
        : "D" ((char *)buffer+lineOfs[0]), "S" (maxX*2),          // %0, %1
          "b" (xRes), "d" (maxY+1), "a" (screenSelector)     // %2, %3, %4        
       );
    break;    
  case 1:
    __asm__ __volatile__(
        "  push %%ds          \n"     // push ds
        "  mov  %%ax, %%ds    \n"     // mov ds, ax
        "  add  %%edi, %%esi  \n"     // add esi, edi
        "vf8lop:              \n"
        "  push %%esi         \n"     // push esi
        "  push %%edi         \n"     // push edi
        "vf8lop2:             \n"
        "  mov  (%%edi),%%al  \n"     // mov al, [edi]
        "  mov  (%%esi),%%ah  \n"     // mov ah, [esi]
        "  mov  %%al,(%%esi)  \n"     // mov [esi], al
        "  mov  %%ah,(%%edi)  \n"     // mov [edi], ah
        "  inc  %%edi         \n"     // inc edi
        "  dec  %%esi         \n"     // dec esi
        "  cmp  %%esi, %%edi  \n"     // cmp edi, esi
        "   jbe vf8lop2       \n"
        "  pop  %%edi         \n"     // pop edi
        "  pop  %%esi         \n"     // pop esi
        "  add  %%ebx, %%esi  \n"     // add esi, ebx
        "  add  %%ebx, %%edi  \n"     // add edi, ebx
        "  dec  %%edx         \n"
        "   jnz vf8lop        \n"
        "  pop  %%ds          \n"     // pop ds
        :
        : "D" ((char *)buffer+lineOfs[0]), "S" (maxX),          // %0, %1
          "b" (xRes), "d" (maxY+1), "a" (screenSelector)     // %2, %3, %4        
       );
    break;
  } // switch
  return;
} // ogDisplay_VESA::ogVFlip

void
ogDisplay_VESA::ogVLine(int32 x, int32 y1, int32 y2, uInt32 colour) {
  int32 tmp;
  uInt8 r, g, b, a;

  if (!ogAvail()) return;
  if ((uInt32)x > maxX) return;

  if (y1 > y2) {
    tmp= y1;
    y1 = y2;
    y2 = tmp;
  } // if
  
  if (y1 < 0) y1 = 0;
  if (y2 > (int32)maxY) y2 = maxY;
  if (y2 < y1) return;

  if (ogIsBlending()) {

    ogUnpack(colour, r, g, b, a);

    if (a == 0) return;

    if (a != 255) {
      for (tmp = y1; tmp <= y2; tmp++) 
        RawSetPixel(x, tmp, r, g, b, a);
      return;
    } // if 
   
  } // if blending
  
  switch (bytesPerPix) {
   case 4:
    __asm__ __volatile__(
        "  push %%ds          \n"          //  push     ds
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  shl  $2, %%ebx     \n"          //  shl      ebx, 2  - pix size
        "  mov  %7, %%si      \n"          //  mov      si, screenSelector
        "  mov  %%si, %%ds    \n"          //  mov      ds, si
        "  mov  %6, %%esi     \n"          //  mov      esi, y1
        "  sub  %%esi, %%ecx  \n"          //  sub      ecx, esi
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  mov  %2, %%eax     \n"          //  mov      eax, colour        
        "  inc  %%ecx         \n"          //  inc      ecx
        "vLlop32:             \n"
        "  mov  %%eax, (%%edi)\n"          //  mov      [edi], eax
        "  add  %%edx, %%edi  \n"          //  add      edi, edx
        "  dec  %%ecx         \n"          //  dec      ecx
        "   jnz vLlop32       \n"
        "  pop  %%ds          \n"          //  pop      ds        
        :
        : "D" (buffer), "S" (lineOfs[y1]),          // %0, %1
          "m" (colour), "b" (x),                    // %2, %3
          "c" (y2), "d" (xRes),                     // %4, %5
          "m" (y1), "m" (screenSelector)            // %6, %7
       );
    break;
   case 3:
    __asm__ __volatile__(
        "  push %%ds          \n"          //  push     ds    
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  mov  %%ebx, %%esi  \n"          //  mov      esi, ebx - pix size
        "  add  %%ebx, %%ebx  \n"          //  add      ebx, ebx - pix size
        "  add  %%esi, %%ebx  \n"          //  add      ebx, esi - pix size
        "  mov  %7, %%si      \n"          //  mov      si, screenSelector
        "  mov  %%si, %%ds    \n"          //  mov      ds, si
        "  mov  %6, %%esi     \n"          //  mov      esi, y1
        "  sub  %%esi, %%ecx  \n"          //  sub      ecx, esi
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  mov  %2, %%eax     \n"          //  mov      eax, colour        
        "  inc  %%ecx         \n"          //  inc      ecx
        "  mov  %%eax, %%ebx  \n"          //  mov      ebx, eax
        "  shr  $16, %%ebx    \n"          //  shr      ebx, 16
        "vLlop24:             \n"
        "  mov  %%ax, (%%edi) \n"          //  mov      [edi], eax
        "  mov  %%bl, 2(%%edi)\n"          //  mov      [edi+2], bl
        "  add  %%edx, %%edi  \n"          //  add      edi, edx
        "  dec  %%ecx         \n"          //  dec      ecx
        "   jnz vLlop24       \n"
        "  pop  %%ds          \n"          //  pop      ds        
        :
        : "D" (buffer), "S" (lineOfs[y1]),          // %0, %1
          "m" (colour), "b" (x),                    // %2, %3
          "c" (y2), "d" (xRes),                     // %4, %5
          "m" (y1), "m" (screenSelector)           // %6, %7          
       );
    break;
   case 2:
    __asm__ __volatile__(
        "  push %%ds          \n"          //  push     ds    
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  add  %%ebx, %%ebx  \n"          //  add      ebx, ebx - pix size
        "  mov  %7, %%si      \n"          //  mov      si, screenSelector
        "  mov  %%si, %%ds    \n"          //  mov      ds, si
        "  mov  %6, %%esi     \n"          //  mov      esi, y1
        "  sub  %%esi, %%ecx  \n"          //  sub      ecx, esi
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  mov  %2, %%eax     \n"          //  mov      eax, colour        
        "  inc  %%ecx         \n"          //  inc      ecx
        "vLlop16:             \n"
        "  mov  %%ax, (%%edi) \n"          //  mov      [edi], ax
        "  add  %%edx, %%edi  \n"          //  add      edi, edx
        "  dec  %%ecx         \n"          //  dec      ecx
        "   jnz vLlop16       \n"
        "  pop  %%ds          \n"          //  pop      ds        
        :
        : "D" (buffer), "S" (lineOfs[y1]),          // %0, %1
          "m" (colour), "b" (x),                    // %2, %3
          "c" (y2), "d" (xRes),                     // %4, %5
          "m" (y1), "m" (screenSelector)           // %6, %7
        );
    break;  
   case 1:
    __asm__ __volatile__(
        "  push %%ds          \n"          //  push     ds
        "  add  %%esi, %%edi  \n"          //  add      edi, esi
        "  mov  %7, %%si      \n"          //  mov      si, screenSelector
        "  mov  %%si, %%ds    \n"          //  mov      ds, si
        "  mov  %6, %%esi     \n"          //  mov      esi, y1
        "  sub  %%esi, %%ecx  \n"          //  sub      ecx, esi
        "  add  %%ebx, %%edi  \n"          //  add      edi, ebx
        "  mov  %2, %%eax     \n"          //  mov      eax, colour
        "  inc  %%ecx         \n"          //  inc      ecx
        "vLlop8:              \n"          
        "  mov  %%al, (%%edi) \n"          //  mov      [edi], al
        "  add  %%edx, %%edi  \n"          //  add      edi, edx
        "  dec  %%ecx         \n"          //  dec      ecx
        "   jnz vLlop8        \n"
        "  pop  %%ds          \n"          //  pop      ds
        :
        : "D" (buffer), "S" (lineOfs[y1]),          // %0, %1
          "m" (colour), "b" (x),                    // %2, %3
          "c" (y2), "d" (xRes),                     // %4, %5
          "m" (y1), "m" (screenSelector)           // %6, %7
       );
    break;

  } // switch
  return;
} // ogDisplay_VESA::ogVLine

ogDisplay_VESA::~ogDisplay_VESA(void) {
  __dpmi_regs regs;
  if (VESAInfo != NULL) delete VESAInfo;
  if (modeInfo != NULL) delete modeInfo;
  if (attributes != NULL) delete attributes;
  if (inGraphics) {
    regs.x.ax = 3;
    __dpmi_int(0x10, &regs);
  } // if inGraphics

  return;
}
