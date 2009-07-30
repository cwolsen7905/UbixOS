/*****************************************************************************************
 Copyright (c) 2002-2004, 2009 The UbixOS Project
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
#include <sys/io.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/vitals.h>
#include <ubixos/spinlock.h>
#include <assert.h>

static u_int32_t   freePages      = 0x0;
static spinLock_t  vmmSpinLock    = SPIN_LOCK_INITIALIZER;
static spinLock_t  vmmCowSpinLock = SPIN_LOCK_INITIALIZER;
int                numPages       = 0x0;
mMap              *vmmMemoryMap   = (mMap *) 0x101000;


/************************************************************************

 Function: void vmmMemMapInit();
 Description: This Function Initializes The Memory Map For the System
 Notes:

  02/20/2004 - Made It Report Real And Available Memory

************************************************************************/
int vmm_memMapInit() {
  int i        = 0x0;
  int memStart = 0x0;

  /* Count System Memory */
  numPages = vmm_countMemory();

  /* Set Memory Map To Point To First Physical Page That We Will Use */
  vmmMemoryMap = (mMap *) 0x101000;

  /* Initialize Map Make All Pages Not Available */
  for (i = 0x0; i < numPages; i++) {
    vmmMemoryMap[i].cowCounter = 0x0;
    vmmMemoryMap[i].status     = PAGE_UNAVAILABLE;
    vmmMemoryMap[i].pid        = vmmID;
    vmmMemoryMap[i].pageAddr   = i * 0x1000;
    }

  /* Calculate Start Of Free Memory */
  memStart  = (0x101000 / 0x1000);
  memStart += (((sizeof(mMap) * numPages) + (sizeof(mMap) - 1)) / 0x1000);

  /* Initialize All Free Pages To Available */
  vmmMemoryMap[(0x100000 / 0x1000)].status = PAGE_AVAILABLE;
  freePages++;
  for (i = memStart; i < numPages; i++) {
    vmmMemoryMap[i].status = PAGE_AVAILABLE;
    freePages++;
    }

  /* Print Out Amount Of Memory */
  kprintf("Real Memory:      %iKB\n", numPages * 4);
  kprintf("Available Memory: %iKB\n", freePages * 4);

  /* Return */
  return (0);
  }

/************************************************************************

 Function: int vmm_countMemory();
 Description: This Function Counts The Systems Physical Memory
 Notes:

  02/20/2004 - Inspect For Quality And Approved
  07/09/2009 - Renamed Function

************************************************************************/
int vmm_countMemory() {
  register u_int32_t *mem        = 0x0;
  unsigned long       memCount   = -1;
  unsigned long       tempMemory = 0x0;
  unsigned long       cr0        = 0x0;
  unsigned short      memKb      = 0x0;
  unsigned char       irq1State;
  unsigned char       irq2State;

  /*
   * Save The States Of Both IRQ 1 And 2 So We Can Turn Them Off And Restore
   * Them Later
   */
  irq1State = inportByte(0x21);
  irq2State = inportByte(0xA1);

  /* Turn Off IRQ 1 And 2 To Prevent Chances Of Faults While Examining Memory */
  outportByte(0x21, 0xFF);
  outportByte(0xA1, 0xFF);

  /* Save The State Of Register CR0 */
  asm volatile (
    "movl %%cr0, %%ebx\n"
    : "=a" (cr0)
    :
    : "ebx"
    );

  asm volatile ("wbinvd");

  asm volatile (
    "movl %%ebx, %%cr0\n"
    :
    : "a" (cr0 | 0x00000001 | 0x40000000 | 0x20000000)
    : "ebx"
    );

  while (memKb < 4096 && memCount != 0) {
    memKb++;
    if (memCount == -1)
      memCount = 0;
    memCount += 1024 * 1024;
    mem = (u_int32_t *)memCount;
    tempMemory = *mem;
    *mem = 0x55AA55AA;
    asm("": : :"memory");
    if (*mem != 0x55AA55AA) {
      memCount = 0;
      }
    else {
      *mem = 0xAA55AA55;
      asm("": : :"memory");
      if (*mem != 0xAA55AA55) {
	memCount = 0;
        }
      }
    asm("": : :"memory");
    *mem = tempMemory;
    }

  asm volatile (
    "movl %%ebx, %%cr0\n"
    :
    :               "a" (cr0)
    :               "ebx"
    );

  /* Restore States For Both IRQ 1 And 2 */
  outportByte(0x21, irq1State);
  outportByte(0xA1, irq2State);

  /* Return Amount Of Memory In Pages */
  return ((memKb * 1024 * 1024) / 4096);
  }

/************************************************************************

 Function: u_int32_t vmmFindFreePage(pid_t pid);

 Description: This Returns A Free  Physical Page Address Then Marks It
              Not Available As Well As Setting The PID To The Proccess
             Allocating This Page
 Notes:

************************************************************************/
u_int32_t vmm_findFreePage(pidType pid) {
  int i = 0x0;

  /* Lets Look For A Free Page */
  if (pid < sysID)
    kpanic("Error: invalid PID %i\n",pid);
    
  spinLock(&vmmSpinLock);

  for (i = 0; i <= numPages; i++) {

    /*
     * If We Found A Free Page Set It To Not Available After That Set Its Own
     * And Return The Address
     */
    if ((vmmMemoryMap[i].status == PAGE_AVAILABLE) && (vmmMemoryMap[i].cowCounter == 0)) {
      vmmMemoryMap[i].status = PAGE_UNAVAILABLE;
      vmmMemoryMap[i].pid = pid;
      freePages--;
      if (systemVitals)
        systemVitals->freePages = freePages;

      spinUnlock(&vmmSpinLock);
      return (vmmMemoryMap[i].pageAddr);
      }
    }

  /* If No Free Memory Is Found Return NULL */
  K_PANIC("Out Of Memory!!!!");
  spinUnlock(&vmmSpinLock);
  return (0x0);
  }


/************************************************************************

 Function: int vmm_freePage(u_int32_t pageAddr);

 Description: This Function Marks The Page As Free

 Notes:

************************************************************************/
void vmm_freePage(u_int32_t pageAddr) {
  int pageIndex = 0x0;

  assert((pageAddr & 0xFFF) == 0x0);

  spinLock(&vmmSpinLock);

  /* Find The Page Index To The Memory Map */
  pageIndex = (pageAddr / 4096);

  /* Check If Page COW Is Greater Then 0 If It Is Dec It If Not Free It */
  if (vmmMemoryMap[pageIndex].cowCounter == 0) {
    /* Set Page As Avail So It Can Be Used Again */
    vmmMemoryMap[pageIndex].status = PAGE_AVAILABLE;
    vmmMemoryMap[pageIndex].cowCounter = 0x0;
    vmmMemoryMap[pageIndex].pid = -2;
    freePages++;
    spinUnlock(&vmmSpinLock);
    systemVitals->freePages = freePages;
    }
  else {
    /* Prevent Dead Lock */
    spinUnlock(&vmmSpinLock);

    /* Adjust The COW Counter */
    vmm_adjustCowCounter(((u_int32_t) vmmMemoryMap[pageIndex].pageAddr), -1);
    }
  }

/************************************************************************

 Function: int adjustCowCounter(u_int32_t baseAddr,int adjustment);

 Description: This Adjust The COW Counter For Page At baseAddr It Will
              Error If The Count Goes Below 0

 Notes:

  08/01/02 - I Think If Counter Gets To 0 I Should Free The Page

************************************************************************/
void vmm_adjustCowCounter(u_int32_t baseAddr, int adjustment) {
  int vmmMemoryMapIndex = (baseAddr / 4096);

  assert((baseAddr & 0xFFF) == 0x0);

  spinLock(&vmmSpinLock);

  /* Adjust COW Counter */
  vmmMemoryMap[vmmMemoryMapIndex].cowCounter += adjustment;

  assert(vmmMemoryMap[vmmMemoryMapIndex].cowCounter >= 0x0);

  if (vmmMemoryMap[vmmMemoryMapIndex].cowCounter == 0x0) {
    vmmMemoryMap[vmmMemoryMapIndex].cowCounter = 0x0;
    vmmMemoryMap[vmmMemoryMapIndex].pid = vmmID;
    vmmMemoryMap[vmmMemoryMapIndex].status = PAGE_AVAILABLE;
    freePages++;
    systemVitals->freePages = freePages;
    }

  spinUnlock(&vmmSpinLock);
  }

/************************************************************************

 Function: void vmm_freeProcessPages(kTask_t tmpTask);

 Description: This Function Will Free Up Memory For The Exiting Process

 Notes:

  08/04/02 - Added Checking For COW Pages First

************************************************************************/
void vmm_freeProcessPages(kTask_t *tmpTask) {
  int i=0;
  int x=0;
  u_int32_t *tmpPageTable = 0x0;
  u_int32_t *tmpPageDir   = (uInt32 *)PARENT_PAGEDIR_ADDR;

/*
 This is where a Memory Leak Came From 
  u_int32_t *tmpPageDir   = (uInt32 *)PARENT_PAGEDIR_ADDR;
*/

  assert(tmpTask);

  spinLock(&vmmSpinLock);

 #ifdef BOOB

  tmpPageDir = (u_int32_t *)0x7A00000;

  if (vmm_remapPage(tmpTask->tss.cr3,0x7A00000,KERNEL_PAGE_DEFAULT) == 0x0)
    K_PANIC("vmmFailed");

  for (i=0;i<0x1000;i++) {
    kprintf("tmpPageDir[%i]: 0x%X",i,tmpPageDir[i] & PAGE_UNMASK);
    if (vmm_remapPage(tmpPageDir[i] & PAGE_UNMASK,0x7A01000 + (i * 0x1000),KERNEL_PAGE_DEFAULT) == 0x0)
      K_PANIC("Returned NULL");
    }


  /* Check Page Directory For An Avail Page Table */
  for (i=0;i<=0x300;i++) {
    kprintf("tPD: 0x%X\n",tmpPageDir[i]);
    if (tmpPageDir[i] != 0) {
      kprintf("PE");
      /* Set Up Page Table Pointer */
      tmpPageTable = (u_int32_t *)(0x7A01000 + (i * 0x1000));
      /* Check The Page Table For COW Pages */
      for (x = 0;x < PAGE_ENTRIES;x++) {
        /* If The Page Is COW Adjust COW Counter */
        if (((u_int32_t)tmpPageTable[x] & PAGE_COW) == PAGE_COW) {
          kprintf("COW!");
          vmm_adjustCowCounter(((u_int32_t)tmpPageTable[x] & 0xFFFFF000),-1);
          }
        }
      }
    }
          //Return The Address Of The Mapped In Memory
          vmm_unmapPages(0x7A00000,1,1);
          for (i=0;i<0x1000;i++) {
            vmm_unmapPages((0x7A01000 + (i*0x1000)),1,1);
            }

  #endif

  /* Loop Through Pages To Find Pages Owned By Process */
  for (i=0;i<numPages;i++) {
    if (vmmMemoryMap[i].pid == tmpTask->id) {
      /* Check To See If The cowCounter Is Zero If So We Can Ree It */
      if (vmmMemoryMap[i].cowCounter == 0) {
        vmmMemoryMap[i].status = PAGE_AVAILABLE;
        vmmMemoryMap[i].cowCounter = 0x0;
        vmmMemoryMap[i].pid = vmmID;
        freePages++;
        systemVitals->freePages = freePages;
        }
      else {
        K_PANIC("COW!NULL");
        }
      }
    }
  /* Return */
  spinUnlock(&vmmSpinLock);
  return;
  }

/***
 $Log$
 Revision 1.4  2009/07/09 04:01:15  reddawg
 More Sanity Checks

 Revision 1.3  2009/07/08 21:20:13  reddawg
 Getting There

 Revision 1.2  2009/07/08 16:05:56  reddawg
 Sync

 Revision 1.1.1.1  2007/01/17 03:31:51  reddawg
 UbixOS

 Revision 1.1  2006/12/01 18:46:19  reddawg
 renaming files

 Revision 1.2  2006/12/01 05:12:35  reddawg
 We're almost there... :)

 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.5  2006/06/01 12:42:09  reddawg
 Getting back to the basics

 Revision 1.4  2006/06/01 04:15:32  reddawg
 Woot

 Revision 1.3  2006/06/01 03:58:33  reddawg
 wondering about this stuff here

 Revision 1.2  2005/10/12 00:13:38  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:51  reddawg
 no message

 Revision 1.15  2004/09/11 23:39:31  reddawg
 ok time for bed

 Revision 1.14  2004/09/11 16:39:19  apwillia
 Fix order in adjustCowCounter to prevent potential race condition

 Revision 1.13  2004/08/14 11:23:03  reddawg
 Changes

 Revision 1.12  2004/08/01 20:51:33  reddawg
 adjustCowCounter: we no longer need to debug unhandled adjustments they are normal situations now

 Revision 1.11  2004/07/28 00:17:05  reddawg
 Major:
   Disconnected page 0x0 from the system... Unfortunately this broke many things
   all of which have been fixed. This was good because nothing deferences NULL
   any more.

 Things affected:
   malloc,kmalloc,getfreepage,getfreevirtualpage,pagefault,fork,exec,ld,ld.so,exec,file

 Revision 1.10  2004/07/26 19:15:49  reddawg
 test code, fixes and the like

 Revision 1.9  2004/07/24 23:04:44  reddawg
 Changes... mark let me know if you fault at pid 185 when you type stress

 Revision 1.8  2004/07/24 17:47:28  reddawg
 vmm_pageFault: deadlock resolved thanks to a propper solution suggested by geist

 Revision 1.7  2004/07/19 02:04:32  reddawg
 memory.c: added spinlocks to vmmFindFreePage and vmmFreePage to prevent two tasks from possibly allocating the same page

 Revision 1.6  2004/06/14 12:20:54  reddawg
 notes: many bugs repaired and ld works 100% now.

 Revision 1.5  2004/05/21 15:34:23  reddawg
 Fixed a couple of typo

 Revision 1.4  2004/05/21 14:50:10  reddawg
 Cleaned up

 Revision 1.3  2004/05/19 17:28:28  reddawg
 Added the correct endTask Procedure

 Revision 1.2  2004/04/30 14:16:04  reddawg
 Fixed all the datatypes to be consistant uInt8,uInt16,u_int32_t,Int8,Int16,Int32

 Revision 1.1.1.1  2004/04/15 12:06:52  reddawg
 UbixOS v1.0

 Revision 1.27  2004/04/13 16:36:34  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
