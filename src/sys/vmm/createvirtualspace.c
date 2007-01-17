/*****************************************************************************************
 Copyright (c) 2002 The UbixOS Project
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

 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:38  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:50  reddawg
 no message

 Revision 1.2  2004/07/28 15:05:43  reddawg
 Major:
   Pages now have strict security enforcement.
   Many null dereferences have been resolved.
   When apps loaded permissions set for pages rw and ro

 Revision 1.1.1.1  2004/04/15 12:06:51  reddawg
 UbixOS v1.0

 Revision 1.8  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license



 $Id$

*****************************************************************************************/

#include <vmm/vmm.h>


/************************************************************************

Function: void *vmmCreateVirtualSpace(pid_t);
Description: Creates A Virtual Space For A New Task
Notes:

07/30/02 - This Is Going To Create A New VM Space However Its Going To
           Share The Same Top 1GB Space With The Kernels VM And Lower
           1MB Of VM Space With The Kernel

07/30/02 - Note This Is Going To Get The Top 1Gig And Lower 1MB Region
           From The Currently Loaded Page Directory This Is Safe Because
           All VM Spaces Will Share These Regions

07/30/02 - Note I Realized A Mistake The First Page Table Will Need To Be
           A Copy But The Page Tables For The Top 1GB Will Not Reason For
           This Is That We Just Share The First 1MB In The First Page Table
           So We Will Just Share Physical Pages.

08/02/02 - Added Passing Of pid_t pid For Better Tracking Of Who Has Which
           Set Of Pages

************************************************************************/
void           *
vmmCreateVirtualSpace(pid_t pid)
{
  void           *newPageDirectoryAddress = 0x0;
  uInt32         *parentPageDirectory = 0x0, *newPageDirectory = 0x0;
  uInt32         *parentPageTable = 0x0, *newPageTable = 0x0;
  int             x = 0;

  /* Set Address Of Parent Page Directory */
  parentPageDirectory = (uInt32 *) parentPageDirAddr;
  /* Allocate A New Page For The New Page Directory */
  newPageDirectory = (uInt32 *) vmmGetFreePage(pid);
  /* Set newPageDirectoryAddress To The Newly Created Page Directories Page */
  newPageDirectoryAddress = (void *)vmm_getPhysicalAddr((uInt32) newPageDirectory);
  /* First Set Up A Flushed Page Directory */
  for (x = 0; x < pageEntries; x++) {
    (uInt32) newPageDirectory[x] = (uInt32) 0x0;
  }
  /* Map The Top 1GB Region Of The VM Space */
  for (x = 768; x < pageEntries; x++) {
    newPageDirectory[x] = parentPageDirectory[x];
  }
  /*
   * Allocate A New Page For The The First Page Table Where We Will Map The
   * Lower Region
   */
  newPageTable = (uInt32 *) vmmGetFreePage(pid);
  /* Flush The Page From Garbage In Memory */
  for (x = 0; x < pageEntries; x++) {
    (uInt32) newPageTable[x] = (uInt32) 0x0;
  }
  /* Map This Into The Page Directory */
  newPageDirectory[0] = (vmm_getPhysicalAddr((uInt32) newPageTable) | PAGE_DEFAULT);
  /* Set Address Of Parents Page Table */
  parentPageTable = (uInt32 *) tablesBaseAddress;
  /* Map The First 1MB Worth Of Pages */
  for (x = 0; x < (pageEntries / 4); x++) {
    newPageTable[x] = parentPageTable[x];
  }
  /* Set Virtual Mapping For Page Directory */
  newPageTable[256] = (vmm_getPhysicalAddr((uInt32) newPageDirectory) | PAGE_DEFAULT);

  /*
   * Now The Fun Stuff Build The Initial Virtual Page Space So We Don't Have
   * To Worry About Mapping Them In Later How Ever I'm Concerned This May
   * Become A Security Issue
   */
  /* First Lets Unmap The Previously Allocated Page Table */
  vmmUnmapPage((uInt32) newPageTable, 1);
  /* Allocate A New Page Table */
  newPageTable = (uInt32 *) vmmGetFreePage(pid);
  /* First Set Our Page Directory To Contain This */
  newPageDirectory[767] = vmm_getPhysicalAddr((uInt32) newPageTable) | PAGE_DEFAULT;
  /* Now Lets Build The Page Table */
  for (x = 0; x < pageEntries; x++) {
    newPageTable[x] = newPageDirectory[x];
  }
  /* Now We Are Done So Lets Unmap This Page */
  vmmUnmapPage((uInt32) newPageTable, 1);
  /* Now We Are Done With The Page Directory So Lets Unmap That Too */
  vmmUnmapPage((uInt32) newPageDirectory, 1);
  /* Return Physical Address Of Page Directory */
  return (newPageDirectoryAddress);
}

/***
 END
 ***/

