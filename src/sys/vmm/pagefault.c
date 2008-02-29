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

 $Id$

*****************************************************************************************/

#include <vmm/vmm.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>

static spinLock_t pageFaultSpinLock = SPIN_LOCK_INITIALIZER;

/*****************************************************************************************

 Function:    void vmm_pageFault(uInt32 memAddr,uInt32 eip,uInt32 esp);
 Description: This is the page fault handler, it will handle COW and trap all other
              exceptions and segfault the thread.

 Notes:
 
07/30/02 - Fixed COW However I Need To Think Of A Way To Impliment
           A Paging System Also Start To Add Security Levels
    
07/27/04 - Added spin locking to ensure that we are thread safe. I know that spining a 
           cpu is a waste of resources but for now it prevents errors.

*****************************************************************************************/
void vmm_pageFault(uInt32 memAddr,uInt32 eip,uInt32 esp) {
  uInt32 i = 0x0, pageTableIndex = 0x0,pageDirectoryIndex = 0x0;
  uInt32 *pageDir = 0x0,*pageTable = 0x0;
  uInt32 *src = 0x0,*dst = 0x0;

  /* Try to aquire lock otherwise spin till we do */
  spinLock(&pageFaultSpinLock);

  /* Set page dir pointer to the address of the visable page directory */
  pageDir = (uInt32 *)parentPageDirAddr;

  /* UBU - This is a temp panic for 0x0 read write later on I will handle this differently */
  if (memAddr == 0x0) {
    kprintf("Segfault At Address: [0x%X][0x%X][%i][0x%X]\n",memAddr,esp,_current->id,eip);
    kpanic("Error We Wrote To 0x0\n");
    }

  /* Calculate The Page Directory Index */
  pageDirectoryIndex = (memAddr >> 22);
  
  /* Calculate The Page Table Index     */
  pageTableIndex = ((memAddr >> 12) & 0x3FF);

  /* UBU - This is a temporary routine for handling access to a page of a non existant page table */
  if (pageDir[pageDirectoryIndex] == 0x0) {
    kprintf("Segfault At Address: [0x%X][0x%X][%i][0x%X], Not A Valid Page Table\n",memAddr,esp,_current->id,eip);
    spinUnlock(&pageFaultSpinLock);
    endTask(_current->id);
    }
  else {
    /* Set pageTable To Point To Virtual Address Of Page Table */
    pageTable = (uInt32 *)(tablesBaseAddress + (0x1000 * pageDirectoryIndex));
    
    /* Test if this is a COW on page */
    if (((uInt32)pageTable[pageTableIndex] & PAGE_COW) == PAGE_COW) {
      /* Set Src To Base Address Of Page To Copy */
      src = (uInt32 *)(memAddr & 0xFFFFF000);
      /* Allocate A Free Page For Destination */
      /* USE vmInfo */
      dst = (uInt32 *) vmmGetFreeVirtualPage(_current->id,1,0x1);
      /* Copy Memory */
      for (i=0;i<pageEntries;i++) {
        dst[i] = src[i];
        }
      /* Adjust The COW Counter For Physical Page */
      adjustCowCounter(((uInt32)pageTable[pageTableIndex] & 0xFFFFF000),-1);
      /* Remap In New Page */
      pageTable[pageTableIndex] = (uInt32)(vmm_getPhysicalAddr((uInt32)dst)|(memAddr & 0xFFF));
      /* Unlink From Memory Map Allocated Page */
      vmmUnmapPage((uInt32)dst,1);
      }
    else if (pageTable[pageTableIndex] != 0x0) {
      kprintf("Security failed pagetable not user permission\n");
      kprintf("pageTable: [0x%X:0x%X:0x%X:0x%X]\n",pageTable[pageTableIndex],pageTableIndex,pageDirectoryIndex,eip);
      kprintf("Segfault At Address: [0x%X], Stack Address: [0x%X], Proc: [%i],EIP: [0x%X] Non Mapped\n",memAddr,esp,_current->id,eip);
      spinUnlock(&pageFaultSpinLock);
      endTask(_current->id);
      }
    else if (memAddr < (_current->td.vm_dsize + _current->td.vm_daddr)) {
      pageTable[pageTableIndex] = (uInt32)vmmFindFreePage(_current->id) | PAGE_DEFAULT;
      }
    else {
      spinUnlock(&pageFaultSpinLock);
      /* Need To Create A Routine For Attempting To Access Non Mapped Memory */
      kprintf("Segfault At Address: [0x%X][0x%X][%i][0x%X] Non Mapped\n",memAddr,esp,_current->id,eip);
      //kprintf("Out Of Stack Space: [0x%X]\n",memAddr & 0xFF0000);
      spinUnlock(&pageFaultSpinLock);
      endTask(_current->id);
      }
    }
  asm volatile(
    "movl %cr3,%eax\n"
    "movl %eax,%cr3\n"
    );
  
  /* Release the spin lock */
  spinUnlock(&pageFaultSpinLock);
  return;
  }
  
/***
 $Log$
 Revision 1.2  2007/01/19 17:08:29  reddawg
 Lots of fixes

 Revision 1.1.1.1  2007/01/17 03:31:51  reddawg
 UbixOS

 Revision 1.6  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.5  2006/12/01 05:12:35  reddawg
 We're almost there... :)

 Revision 1.4  2006/11/21 13:25:49  reddawg
 A handful of changes ;)

 Revision 1.3  2006/11/06 19:10:12  reddawg
 Lots Of Updates... Still having issues with brk();

 Revision 1.2  2006/10/31 20:44:19  reddawg
 Lots of changes

 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:38  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:52  reddawg
 no message

 Revision 1.14  2004/08/25 22:02:41  reddawg
 task switching - We now are using software switching to be consistant with the rest of the world because all of this open source freaks gave me a hard time about something I liked. There doesn't seem to be any gain in performance but it is working fine and flawlessly

 Revision 1.13  2004/08/24 05:24:37  reddawg
 TCA Is A BONER!!!!

 Revision 1.12  2004/08/14 11:23:03  reddawg
 Changes

 Revision 1.11  2004/07/28 15:05:43  reddawg
 Major:
   Pages now have strict security enforcement.
   Many null dereferences have been resolved.
   When apps loaded permissions set for pages rw and ro

 Revision 1.10  2004/07/28 00:22:56  reddawg
 bah

 Revision 1.9  2004/07/28 00:17:05  reddawg
 Major:
   Disconnected page 0x0 from the system... Unfortunately this broke many things
   all of which have been fixed. This was good because nothing deferences NULL
   any more.

 Things affected:
   malloc,kmalloc,getfreepage,getfreevirtualpage,pagefault,fork,exec,ld,ld.so,exec,file

 Revision 1.8  2004/07/27 07:09:38  reddawg
 Put in a test for 0x0

 Revision 1.7  2004/07/26 19:15:49  reddawg
 test code, fixes and the like

 Revision 1.6  2004/07/24 23:04:44  reddawg
 Changes... mark let me know if you fault at pid 185 when you type stress

 Revision 1.5  2004/07/24 20:00:51  reddawg
 Lots of changes to the vmm subsystem.... Page faults have been adjust to now be blocking on a per thread basis not system wide. This has resulted in no more deadlocks.. also the addition of per thread locking has removed segfaults as a result of COW in which two tasks fault the same COW page and try to modify it.

 Revision 1.4  2004/07/24 17:47:28  reddawg
 vmm_pageFault: deadlock resolved thanks to a propper solution suggested by geist

 Revision 1.3  2004/07/19 02:05:26  reddawg
 vmmPageFault: had a potential memory leak here for one page it was still using sysID on certain COW scenarios

 Revision 1.2  2004/06/10 22:23:56  reddawg
 Volatiles

 Revision 1.1.1.1  2004/04/15 12:06:52  reddawg
 UbixOS v1.0

 Revision 1.4  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
