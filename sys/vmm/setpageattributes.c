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
#include <ubixos/kpanic.h>

/************************************************************************

 Function: void vmmSetPageAttributes(uInt32 pageAddr,int attributes;
 Description: This Function Will Set The Page Attributes Such As
 A Read Only Page, Stack Page, COW Page, ETC.
 Notes:

 ************************************************************************/
int vmm_setPageAttributes(uInt32 memAddr, uInt16 attributes) {
  uInt16 directoryIndex = 0x0, tableIndex = 0x0;
  uInt32 *pageTable = 0x0;

  /* Calculate The Page Directory Index */
  directoryIndex = (memAddr >> 22);

  /* Calculate The Page Table Index */
  tableIndex = ((memAddr >> 12) & 0x3FF);

  /* Set Table Pointer */
  if ((pageTable = (uInt32 *) (PT_BASE_ADDR + (0x1000 * directoryIndex))) == 0x0)
    kpanic("Error: pageTable == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);

  /* Set Attribute If Page Is Mapped */
  if (pageTable[tableIndex] != 0x0)
    pageTable[tableIndex] = ((pageTable[tableIndex] & 0xFFFFF000) | attributes);

  /* Reload The Page Table; */
  asm volatile(
    "push %eax     \n"
    "movl %cr3,%eax\n"
    "movl %eax,%cr3\n"
    "pop  %eax     \n"
  );
  /* Return */
  return (0x0);
}

/***
 $Log: setpageattributes.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:38  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:53  reddawg
 no message

 Revision 1.4  2004/07/28 15:05:43  reddawg
 Major:
 Pages now have strict security enforcement.
 Many null dereferences have been resolved.
 When apps loaded permissions set for pages rw and ro

 Revision 1.3  2004/07/20 22:29:55  reddawg
 assert: remade assert

 Revision 1.2  2004/06/10 22:23:56  reddawg
 Volatiles

 Revision 1.1.1.1  2004/04/15 12:06:53  reddawg
 UbixOS v1.0

 Revision 1.6  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/

