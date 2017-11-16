/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id: getphysicaladdr.c 188 2016-01-23 03:09:10Z reddawg $

*****************************************************************************************/

#include <vmm/vmm.h>

/*!

Function: void *vmmGetPhysicalAddr();
Description: Returns The Physical Address Of The Virtual Page
Notes:

*/

u_int32_t vmm_getPhysicalAddr(uInt32 pageAddr) {
  int             pageDirectoryIndex = 0x0, pageTableIndex = 0x0;
  uInt32         *pageTable = 0x0;

  //Calculate The Page Directory Index
  pageDirectoryIndex = (pageAddr >> 22);
  
  //Calculate The Page Table Index
  pageTableIndex = ((pageAddr >> 12) & 0x3FF);
  
  /* Set pageTable To The Virtual Address Of Table */
  pageTable = (uInt32 *) (PT_BASE_ADDR + (0x1000 * pageDirectoryIndex));
  /* Return The Physical Address Of The Page */
  return ((uInt32)(pageTable[pageTableIndex] & 0xFFFFF000));
  }

/***
 END
 ***/
