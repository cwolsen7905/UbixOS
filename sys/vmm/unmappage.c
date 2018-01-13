/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <vmm/vmm.h>

/************************************************************************

 Function: void vmm_unmapPage(uInt32 pageAddr,int flags);
 Description: This Function Will Unmap A Page From The Kernel VM Space
 The Flags Variable Decides If Its To Free The Page Or Not
 A Flag Of 0 Will Free It And A Flag Of 1 Will Keep It
 Notes:

 07/30/02 - I Have Decided That This Should Free The Physical Page There
 Is No Reason To Keep It Marked As Not Available

 07/30/02 - Ok A Found A Reason To Keep It Marked As Available I Admit
 Even I Am Not Perfect Ok The Case Where You Wouldn't Want To
 Free It Would Be On Something Like Where I Allocated A Page
 To Create A New Virtual Space So Now It Has A Flag

 ************************************************************************/
void vmm_unmapPage(uInt32 pageAddr, int flags) {
  int pageDirectoryIndex = 0, pageTableIndex = 0;
  uInt32 *pageTable = 0x0;

  /* Get The Index To The Page Directory */
  pageDirectoryIndex = (pageAddr >> 22);

  //Calculate The Page Table Index
  pageTableIndex = ((pageAddr >> 12) & 0x3FF);

  /* Set pageTable To The Virtual Address Of Table */
  pageTable = (uInt32 *) (PT_BASE_ADDR + (0x1000 * pageDirectoryIndex));
  /* Free The Physical Page If Flags Is 0 */
  if (flags == 0) {

    /*
     * This is temp i think its still an issue clearVirtualPage(pageAddr);
     * freePage((uInt32)(pageTable[pageTableIndex] & 0xFFFFF000));
     */
  }
  /* Unmap The Page */
  pageTable[pageTableIndex] = 0x0;
  /* Rehash The Page Directory */
  asm volatile(
    "movl %cr3,%eax\n"
    "movl %eax,%cr3\n"
  );
  /* Return */
  return;
}

/************************************************************************

 Function: void vmm_unmapPages(uInt32 pageAddr,int flags);
 Description: This Function Will Unmap A Page From The Kernel VM Space
 The Flags Variable Decides If Its To Free The Page Or Not
 A Flag Of 0 Will Free It And A Flag Of 1 Will Keep It
 Notes:

 07/30/02 - I Have Decided That This Should Free The Physical Page There
 Is No Reason To Keep It Marked As Not Available

 07/30/02 - Ok A Found A Reason To Keep It Marked As Available I Admit
 Even I Am Not Perfect Ok The Case Where You Wouldn't Want To
 Free It Would Be On Something Like Where I Allocated A Page
 To Create A New Virtual Space So Now It Has A Flag

 ************************************************************************/
void vmm_unmapPages(void *ptr, uint32_t size) {
  uInt32 baseAddr = (uInt32) ptr & 0xFFFFF000;
  uInt32 dI = 0x0, tI = 0x0;
  uInt32 y = 0x0;
  uInt32 *pageTable = 0x0;

  dI = (baseAddr / (1024 * 4096));
  tI = ((baseAddr - (dI * (1024 * 4096))) / 4096);
  pageTable = (uInt32 *) (PT_BASE_ADDR + (4096 * dI));
  for (y = tI; y < (tI + ((size + 4095) / 4096)); y++) {
    pageTable[y] = 0x0;
  }
  return;
}

/***
 $Log: unmappage.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:38  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:54  reddawg
 no message

 Revision 1.4  2004/07/26 19:15:49  reddawg
 test code, fixes and the like

 Revision 1.3  2004/06/15 12:35:05  reddawg
 Cleaned Up

 Revision 1.2  2004/06/10 22:23:56  reddawg
 Volatiles

 Revision 1.1.1.1  2004/04/15 12:06:53  reddawg
 UbixOS v1.0

 Revision 1.7  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
