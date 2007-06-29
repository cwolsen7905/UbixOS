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

 $Id$

*****************************************************************************************/

#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <ubixos/kpanic.h>
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <vmm/vmm.h>
#include <string.h>
#include <assert.h>

/*
 Set up three descriptor tables:
 
 kernDesc      - The inuse descriptor table 
 freeKernDesc  - The free descriptor table (descriptors with memory backing just not in use)
 emptyKernDesc - The empty descriptor table (descriptors with out a memory backing)
 
*/
static struct memDescriptor *usedKernDesc  = 0x0;
static struct memDescriptor *freeKernDesc  = 0x0;
static struct memDescriptor *emptyKernDesc = 0x0;

/*
 Set up our spinlocks so we do not corrupt linked lists if we have re-entrancy
*/
static spinLock_t mallocSpinLock    = SPIN_LOCK_INITIALIZER;
static spinLock_t emptyDescSpinLock = SPIN_LOCK_INITIALIZER;

/************************************************************************

Function: void *getEmptyDesc()
Description: Find An Empty Descriptor

Notes:

02/17/03 - Is This Efficient?

************************************************************************/    
static void *getEmptyDesc() {
  int i = 0x0;
  struct memDescriptor *tmpDesc = 0x0;

  spinLock(&emptyDescSpinLock);
  
  tmpDesc = emptyKernDesc;
  
  if (tmpDesc != 0x0) {
    emptyKernDesc       = tmpDesc->next;
    if (emptyKernDesc != 0x0) 
      emptyKernDesc->prev = 0x0;
      
     
    tmpDesc->next       = 0x0;
    tmpDesc->prev       = 0x0;
    spinUnlock(&emptyDescSpinLock);
    return(tmpDesc);
    }
  if ((emptyKernDesc = (struct memDescriptor *)vmm_getFreeMallocPage(4)) == 0x0)
    kpanic("Error: vmmGetFreeKernelPage returned NULL\n");
    
  /* zero out the memory so we know there is no garbage */
  memset(emptyKernDesc,0x0,0x4000);

  emptyKernDesc[0].next = &emptyKernDesc[1];

  for (i = 0x1;i < ((0x4000/sizeof(struct memDescriptor)));i++) {
    if (i+1 < (0x4000/sizeof(struct memDescriptor)))
      emptyKernDesc[i].next = &emptyKernDesc[i+1];
    else
      emptyKernDesc[i].next = 0x0;
    emptyKernDesc[i].prev = &emptyKernDesc[i-1];
    }

  tmpDesc = &emptyKernDesc[0];

  emptyKernDesc       = tmpDesc->next;
  emptyKernDesc->prev = 0x0;
  tmpDesc->next       = 0x0;
  tmpDesc->prev       = 0x0;
  spinUnlock(&emptyDescSpinLock);
  return(tmpDesc);
  }

/************************************************************************

Function: void insertFreeDesc(struct memDescriptor *freeDesc)
Description: This Function Inserts A Free Descriptor On The List Which Is
             Kept In Size Order

Notes:

02/17/03 - This Was Inspired By TCA's Great Wisdom -
           "[20:20:59] <TCA> You should just insert it in order"

************************************************************************/
static int insertFreeDesc(struct memDescriptor *freeDesc) {
  struct memDescriptor *tmpDesc = 0x0;
  assert(freeDesc);
  
  if (freeDesc->limit <= 0x0)
    kpanic("Inserting Descriptor with no limit\n");
  
  if (freeKernDesc != 0x0) {
 
    #if 0
    freeDesc->next = freeKernDesc;
    freeDesc->prev = 0x0;
    freeKernDesc->prev = freeDesc;
    freeKernDesc = freeDesc;
    #endif 
    
    for (tmpDesc = freeKernDesc;tmpDesc != 0x0;tmpDesc = tmpDesc->next) {
      if (freeDesc->limit <= tmpDesc->limit) {
      
        freeDesc->prev = tmpDesc->prev;
        if (tmpDesc->prev != 0x0)
          tmpDesc->prev->next = freeDesc;

        
        tmpDesc->prev  = freeDesc;
        freeDesc->next = tmpDesc;
        
        if (tmpDesc == freeKernDesc)
          freeKernDesc = freeDesc;
        return(0x0);
        }
       if (tmpDesc->next == 0x0) {
         tmpDesc->next = freeDesc;
         freeDesc->prev = tmpDesc;
         freeDesc->next = 0x0;
         return(0x0);
         }
      }
    kpanic("didnt Insert\n");
    return(0x0);
    }
  else {
    freeDesc->prev = 0x0;
    freeDesc->next = 0x0;
    freeKernDesc = freeDesc;
    return(0x0);
    }

  return(0x1);
  }

/************************************************************************

Function: void mergeMemBlocks()
Description: This Function Will Merge Free Blocks And Free Pages

Notes:

03/05/03 - We Have A Problem It Seems The First Block Is Limit 0x0

************************************************************************/
static void mergeMemBlocks() {
  struct memDescriptor *tmpDesc1 = 0x0;
  struct memDescriptor *tmpDesc2 = 0x0;
  uInt32               baseAddr  = 0x0;

  return;

  //Loop The Free Descriptors See If We Can Merge Them
  mergeStart:
  for (tmpDesc1=freeKernDesc;tmpDesc1 != 0x0;tmpDesc1=tmpDesc1->next) {
    /*
     Compare The Base Addr With The Other Descriptors If You Find The One
     That You Are Looking For Lets Merge Them
    */
    if (tmpDesc1->limit != 0x0) {
      baseAddr = (uInt32)tmpDesc1->baseAddr + (uInt32)tmpDesc1->limit;
      for (tmpDesc2=freeKernDesc;tmpDesc2;tmpDesc2=tmpDesc2->next) {
        if ((uInt32)tmpDesc2->baseAddr == baseAddr) {
          tmpDesc1->limit += tmpDesc2->limit;
          tmpDesc2->baseAddr = 0x0;
          tmpDesc2->limit    = 0x0;
          if (tmpDesc2->prev) {
            tmpDesc2->prev->next = tmpDesc2->next;
            }
          if (tmpDesc2->next) {
            tmpDesc2->next->prev = tmpDesc2->prev;
            }
          tmpDesc2->prev      = 0x0;
          tmpDesc2->next      = emptyKernDesc;
          emptyKernDesc->prev = tmpDesc2;
          emptyKernDesc       = tmpDesc2;
          if (tmpDesc1->prev) {
            tmpDesc1->prev->next = tmpDesc1->next;
            }
          if (tmpDesc1->next) {
            tmpDesc1->next->prev = tmpDesc1->prev;
            }
          tmpDesc1->prev = 0x0;
          tmpDesc1->next = 0x0;
	  kprintf("mergememBlocks: [%i]\n",tmpDesc1->limit);
          insertFreeDesc(tmpDesc1);
          //tmpDesc1 = freeKernDesc;
	  goto mergeStart;
          break;
          }
        }
      }
    }
  return;
  }


/************************************************************************

Function: void *kmalloc(uInt32 len)
Description: Allocate Kernel Memory

Notes:

02/17/03 - Do I Still Need To Pass In The Pid?

************************************************************************/    
void *kmalloc(uInt32 len) {
  struct memDescriptor *tmpDesc1 = 0x0;
  struct memDescriptor *tmpDesc2 = 0x0;
  char                 *buf      = 0x0;
  int                   i        = 0x0;
  uInt16                pages    = 0x0;


  spinLock(&mallocSpinLock);
  
  len = MALLOC_ALIGN(len);

  
  if (len == 0x0) {
    spinUnlock(&mallocSpinLock);
	kprintf("kmalloc: len = 0!\n");
    return(0x0);
    }
  for (tmpDesc1 = freeKernDesc;tmpDesc1 != 0x0;tmpDesc1=tmpDesc1->next) {
    assert(tmpDesc1);
    if (tmpDesc1->limit >= len) {
      if (tmpDesc1->prev != 0x0)
        tmpDesc1->prev->next = tmpDesc1->next;
      if (tmpDesc1->next != 0x0)
        tmpDesc1->next->prev = tmpDesc1->prev;

      if (tmpDesc1 == freeKernDesc)
        freeKernDesc = tmpDesc1->next;

      tmpDesc1->prev = 0x0;
      tmpDesc1->next = usedKernDesc;
      if (usedKernDesc != 0x0)
        usedKernDesc->prev = tmpDesc1;
      usedKernDesc = tmpDesc1; 
      if (tmpDesc1->limit > len) {
        tmpDesc2 = getEmptyDesc();
        assert(tmpDesc2);
        tmpDesc2->limit = tmpDesc1->limit - len;
        tmpDesc1->limit = len;
        tmpDesc2->baseAddr = tmpDesc1->baseAddr + len;
        tmpDesc2->next = 0x0;
        tmpDesc2->prev = 0x0;
        insertFreeDesc(tmpDesc2);
        }
      buf = (char *)tmpDesc1->baseAddr;
      for (i=0;i<tmpDesc1->limit;i++) {
        buf[i] = 0x0;
        }
      spinUnlock(&mallocSpinLock);
      //kprintf("m1[%i:%i:0x%X]",tmpDesc1->limit,len,tmpDesc1->baseAddr);
	assert(tmpDesc1->baseAddr);
      return(tmpDesc1->baseAddr);
      }
    }
  tmpDesc1 = getEmptyDesc();
  //kprintf("no empty desc\n");
  if (tmpDesc1 != 0x0) {
    pages = ((len + 4095)/4096);
    tmpDesc1->baseAddr = (struct memDescriptor *)vmm_getFreeMallocPage(pages);
    tmpDesc1->limit    = len;
    tmpDesc1->next     = usedKernDesc;
    tmpDesc1->prev     = 0x0;
    if (usedKernDesc != 0x0)
      usedKernDesc->prev       = tmpDesc1;
    usedKernDesc             = tmpDesc1;

    if (((pages * 4096)-len) > 0x0) {
      tmpDesc2           = getEmptyDesc();
      assert(tmpDesc2);
      tmpDesc2->baseAddr = tmpDesc1->baseAddr + tmpDesc1->limit;
      tmpDesc2->limit    = ((pages * 4096)-len);
      tmpDesc2->prev     = 0x0;
      tmpDesc2->next     = 0x0;
      if (tmpDesc2->limit <= 0x0)
        kprintf("kmalloc-2 tmpDesc2: [%i]\n",tmpDesc2->limit);
      insertFreeDesc(tmpDesc2);
      }

      buf = (char *)tmpDesc1->baseAddr;
      for (i=0;i<tmpDesc1->limit;i++) {
        buf[i] = 0x0;
        }
    spinUnlock(&mallocSpinLock);
    //kprintf("baseAddr2[0x%X:0x%X]",tmpDesc1,tmpDesc1->baseAddr);
    //kprintf("m2[%i:%i:0x%X]",tmpDesc1->limit,len,tmpDesc1->baseAddr);
	assert(tmpDesc1->baseAddr);
    return(tmpDesc1->baseAddr);
    }
  //Return Null If Unable To Malloc
  spinUnlock(&mallocSpinLock);
  //kprintf("baseAddr3[0x0]");
  return(0x0);
  }
  
/************************************************************************

Function: void kfree(void *baseAddr)
Description: This Will Find The Descriptor And Free It

Notes:

02/17/03 - I need To Make It Join Descriptors

************************************************************************/
void kfree(void *baseAddr) {
  struct memDescriptor *tmpDesc = 0x0;

  if (baseAddr == 0x0) return; 
  assert(baseAddr);

  assert(usedKernDesc);
  spinLock(&mallocSpinLock);
  
  for (tmpDesc = usedKernDesc;tmpDesc != 0x0;tmpDesc = tmpDesc->next) {
    
    if (tmpDesc->baseAddr == baseAddr) {
      memset(tmpDesc->baseAddr,0xBE,tmpDesc->limit);
    
      if (usedKernDesc == tmpDesc)
        usedKernDesc = tmpDesc->next;

      if (tmpDesc->prev != 0x0)
        tmpDesc->prev->next = tmpDesc->next;

      if (tmpDesc->next != 0x0)
        tmpDesc->next->prev = tmpDesc->prev;


      tmpDesc->next = 0x0;
      tmpDesc->prev = 0x0;
      
      if (tmpDesc->limit <= 0x0)
        kprintf("kfree tmpDesc1: [%i]\n",tmpDesc->limit);
      //kprintf("{0x%X}",tmpDesc->baseAddr);
      insertFreeDesc(tmpDesc);

     // mergeMemBlocks();
      spinUnlock(&mallocSpinLock);
      return;
      }
    }
  spinUnlock(&mallocSpinLock);
  kprintf("Kernel: Error Freeing Descriptor! [0x%X]\n",(uInt32)baseAddr);
  return;
  }

/***
 $Log$
 Revision 1.1.1.1  2007/01/17 03:31:54  reddawg
 UbixOS

 Revision 1.3  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.2  2006/10/06 15:48:01  reddawg
 Starting to make ubixos work with UFS2

 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.4  2006/06/01 12:42:09  reddawg
 Getting back to the basics

 Revision 1.3  2006/06/01 03:58:33  reddawg
 wondering about this stuff here

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:11  reddawg
 no message

 Revision 1.35  2005/08/04 18:32:59  fsdfs

 added error reporting

 Revision 1.34  2005/08/04 18:23:41  reddawg
 BUG: Assert has issues that must be looked into

 Revision 1.33  2005/08/04 17:11:11  fsdfs

 ----------------------------------------

 -------------------

 Revision 1.32  2004/09/28 21:50:04  reddawg
 kmalloc: now when we kfree memory is filled with 0xBE so it is easy to debug if we continue to use free'd memory

 Revision 1.31  2004/09/19 16:17:25  reddawg
 fixed memory leak we now lose no memory....

 Revision 1.30  2004/09/14 21:51:24  reddawg
 Debug info

 Revision 1.29  2004/09/11 23:39:31  reddawg
 ok time for bed

 Revision 1.28  2004/09/11 23:21:26  reddawg
 run now do you get fegfaults with BB?

 Revision 1.27  2004/09/11 22:49:28  reddawg
 pat look at lines 276-285  does the math seem right?

 Revision 1.26  2004/09/11 22:33:13  reddawg
 minor changes

 Revision 1.25  2004/09/11 12:11:11  reddawg
 Cleaning up the VFS more changes to follow...

 Revision 1.24  2004/09/08 23:19:58  reddawg
 hmm

 Revision 1.23  2004/09/06 15:13:25  reddawg
 Last commit before FreeBSD 6.0

 Revision 1.22  2004/08/26 22:51:18  reddawg
 TCA touched me :( i think he likes men....


 sched.h:        kTask_t added parentPid
 endtask.c:     fixed term back to parentPid
 exec.c:          cleaned warnings
 fork.c:            fixed term to childPid
 sched.c:         clean up for dead tasks
 systemtask.c: clean up dead tasks
 kmalloc.c:       cleaned up warnings
 udp.c:            cleaned up warnings
 bot.c:             cleaned up warnings
 shell.c:           cleaned up warnings
 tcpdump.c:     took a dump
 hd.c:             cleaned up warnings
 ubixfs.c:        stopped prning debug info

 Revision 1.21  2004/07/28 15:05:43  reddawg
 Major:
   Pages now have strict security enforcement.
   Many null dereferences have been resolved.
   When apps loaded permissions set for pages rw and ro

 Revision 1.20  2004/07/28 00:17:05  reddawg
 Major:
   Disconnected page 0x0 from the system... Unfortunately this broke many things
   all of which have been fixed. This was good because nothing deferences NULL
   any more.

 Things affected:
   malloc,kmalloc,getfreepage,getfreevirtualpage,pagefault,fork,exec,ld,ld.so,exec,file

 Revision 1.19  2004/07/26 19:15:49  reddawg
 test code, fixes and the like

 Revision 1.18  2004/07/26 16:52:45  reddawg
 here we go

 Revision 1.17  2004/07/24 23:04:44  reddawg
 Changes... mark let me know if you fault at pid 185 when you type stress

 Revision 1.16  2004/07/21 10:02:09  reddawg
 devfs: renamed functions
 device system: renamed functions
 fdc: fixed a few potential bugs and cleaned up some unused variables
 strol: fixed definition
 endtask: made it print out freepage debug info
 kmalloc: fixed a huge memory leak we had some unhandled descriptor insertion so some descriptors were lost
 ld: fixed a pointer conversion
 file: cleaned up a few unused variables
 sched: broke task deletion
 kprintf: fixed ogPrintf definition

 Revision 1.15  2004/07/20 23:20:50  reddawg
 kmalloc: forgot to remove an assert

 Revision 1.14  2004/07/20 23:18:11  reddawg
 Made malloc a little more robust but we have a serious memory leak somewhere

 Revision 1.13  2004/07/20 22:29:55  reddawg
 assert: remade assert

 Revision 1.12  2004/07/20 18:58:24  reddawg
 Few fixes

 Revision 1.11  2004/07/18 05:24:15  reddawg
 Fixens

 Revision 1.10  2004/07/17 18:00:47  reddawg
 kmalloc: added assert()

 Revision 1.9  2004/07/17 15:54:52  reddawg
 kmalloc: added assert()
 bioscall: fixed some potential problem by not making 16bit code
 paging: added assert()

 Revision 1.8  2004/06/17 14:50:32  reddawg
 kmalloc: converted some variables to static

 Revision 1.7  2004/06/17 02:54:54  flameshadow
 chg: fixed cast

 Revision 1.6  2004/05/26 11:56:51  reddawg
 kmalloc: fixed memrgeMemBlocks hopefully it will prevent future segfault issues
          by not having any more overlapping blocks

 Revision 1.5  2004/05/25 14:01:14  reddawg
 Implimented Better Spinlocking No More Issues With KMALLOC which actually
 was causing bizzare problems

 END
 ***/

