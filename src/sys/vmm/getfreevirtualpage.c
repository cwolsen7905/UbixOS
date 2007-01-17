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

#include <vmm/vmm.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kprint.h>

static spinLock_t fvpSpinLock = SPIN_LOCK_INITIALIZER;

/************************************************************************

Function: void *vmmGetFreeVirtualPage(pidType pid,int count);
Description: Returns A Free Page Mapped To The VM Space
Notes:

08/11/02 - This Will Return Next Avilable Free Page Of Tasks VM Space

************************************************************************/
void *vmmGetFreeVirtualPage(pidType pid, int count,int type) {
  int             x = 0, y = 0, c = 0;
  uInt32         *pageTableSrc = 0x0;
  uInt32         *pageDir = 0x0;
  uInt32          start_page = 0x0;

  
  spinLock(&fvpSpinLock);
  
  pageDir = (uInt32 *) parentPageDirAddr;

  /* Lets Search For A Free Page */
  if (_current->oInfo.vmStart <= 0x100000)
    kpanic("Invalid vmStart\n");


  if (type == VM_THRD) {
    start_page = (u_int32_t)(_current->td.vm_daddr + ctob(_current->td.vm_dsize));
kprintf(".%i.",count);
   //count--; 
    }
  else if (type == VM_TASK) {
    //kprintf("vmStart");
    start_page = _current->oInfo.vmStart;
    }
  else
    K_PANIC("Invalid Type");

  //for (x = ((_current->td.vm_daddr + _current->td.vm_dsize) / (1024 * 4096)); x < 1024; x++) {
  for (x = (start_page / (1024 * 4096)); x < 1024; x++) {
    /* Set Page Table Address */
    if ((pageDir[x] & PAGE_PRESENT) != PAGE_PRESENT) {
      /* If Page Table Is Non Existant Then Set It Up */
      pageDir[x] = (uInt32) vmmFindFreePage(_current->id) | PAGE_DEFAULT;
      /* Also Add It To Virtual Space So We Can Make Changes Later */
      pageTableSrc = (uInt32 *) (tablesBaseAddress + (4096 * 767));
      pageTableSrc[x] = pageDir[x];
      y = 1;
      /* Reload Page Directory */
      asm(
	  "movl %cr3,%eax\n"
	  "movl %eax,%cr3\n"
	);
    }
    pageTableSrc = (uInt32 *) (tablesBaseAddress + (0x1000 * x));
    if (y != 0x0) {
      for (y = 0x0;y<pageEntries;y++) {
        pageTableSrc[y] = (uInt32)0x0;
        }
      }
    for (y = 0; y < 1024; y++) {
      /* Loop Through The Page Table Find An UnAllocated Page */
      if ((pageTableSrc[y] & PAGE_COW) == PAGE_COW) {
        kprintf("PAGE_COW");
        //_current->td.vm_dsize += btoc(0x1000);
        /* HACK MEMORY LEAK */
        //pageTableSrc[y] = 0x0;
        }
      if ((uInt32) pageTableSrc[y] == (uInt32) 0x0) {
	if (count > 0x1) {
          for (c = 0; c < count; c++) {
            if (y + c < 1024) {
      if ((pageTableSrc[y + c] & PAGE_COW) == PAGE_COW) {
        kprintf("PAGE-COW");
        //_current->td.vm_dsize += btoc(0x1000);
        /* HACK MEMORY LEAK */
        //pageTableSrc[y + c] = 0x0;
        }

              if ((uInt32) pageTableSrc[y + c] != (uInt32) 0x0) {
                c = -1;
                break;
                }
              }
            }
	  if (c != -1) {
	    for (c = 0; c < count; c++) {
	      if ((vmm_remapPage((uInt32) vmmFindFreePage(pid), ((x * (1024 * 4096)) + ((y + c) * 4096)),PAGE_DEFAULT)) == 0x0)
	        kpanic("vmmRemapPage: getFreeVirtualPage-1: [0x%X]\n",((x * (1024 * 4096)) + ((y + c) * 4096)));
	      vmmClearVirtualPage((uInt32) ((x * (1024 * 4096)) + ((y + c) * 4096)));
	    }
            if (type == VM_THRD)
              _current->td.vm_dsize += btoc(count * 0x1000);
	    spinUnlock(&fvpSpinLock);
	    return ((void *)((x * (1024 * 4096)) + (y * 4096)));
	  }
	} else {
	  /* Map A Physical Page To The Virtual Page */

	  /*
	   * remapPage((uInt32)vmmFindFreePage(pid),((x*(1024*4096))+(y*4096))
	   * ,pid);
	   */
	  if ((vmm_remapPage((uInt32) vmmFindFreePage(pid), ((x * (1024 * 4096)) + (y * 4096)),PAGE_DEFAULT)) == 0x0)
	    kpanic("vmmRemapPage: getFreeVirtualPage-2\n");

	  /* Clear This Page So No Garbage Is There */
	  vmmClearVirtualPage((uInt32) ((x * (1024 * 4096)) + (y * 4096)));

	  /* Return The Address Of The Newly Allocate Page */
          if (type == VM_THRD) {
            _current->td.vm_dsize += btoc(count * 0x1000);
            kprintf("vm_dsize: [0x%X]][0x%X]\n",ctob(_current->td.vm_dsize),_current->td.vm_dsize);
            }
          //kprintf("(0x%X:0x%X)",_current->td.vm_dsize,vmm_getPhysicalAddr(((x * (1024 * 4096)) + (y * 4096))));
//          kprintf("(0x%X:0x%X)",_current->td.vm_dsize + _current->td.vm_daddr,((x * (1024 * 4096)) + (y * 4096)));
	  spinUnlock(&fvpSpinLock);
	  return ((void *)((x * (1024 * 4096)) + (y * 4096)));
	}
      }
    }
  }
  /* If No Free Page Was Found Return NULL */
  spinUnlock(&fvpSpinLock);
  return (0x0);
}

void *vmmGetFreeVirtualPage_new(pidType pid, int count,int type,u_int32_t start_addr) {
  int             x = 0, y = 0, c = 0;
  uInt32         *pageTableSrc = 0x0;
  uInt32         *pageDir = 0x0;
  uInt32          start_page = 0x0;

  
  spinLock(&fvpSpinLock);
  
  pageDir = (uInt32 *) parentPageDirAddr;

  /* Lets Search For A Free Page */
  if (_current->oInfo.vmStart <= 0x100000)
    kpanic("Invalid vmStart\n");

  if (type == VM_THRD) {
    start_page = (u_int32_t)(_current->td.vm_daddr + ctob(_current->td.vm_dsize));
    }
  else if (type == VM_TASK) {
    //kprintf("vmStart");
    start_page = _current->oInfo.vmStart;
    }
  else
    K_PANIC("Invalid Type");

start_page = start_addr;

  //for (x = ((_current->td.vm_daddr + _current->td.vm_dsize) / (1024 * 4096)); x < 1024; x++) {
  for (x = (start_page / (1024 * 4096)); x < 1024; x++) {
    /* Set Page Table Address */
    if ((pageDir[x] & PAGE_PRESENT) != PAGE_PRESENT) {
      /* If Page Table Is Non Existant Then Set It Up */
      pageDir[x] = (uInt32) vmmFindFreePage(_current->id) | PAGE_DEFAULT;
      /* Also Add It To Virtual Space So We Can Make Changes Later */
      pageTableSrc = (uInt32 *) (tablesBaseAddress + (4096 * 767));
      pageTableSrc[x] = pageDir[x];
      y = 1;
      /* Reload Page Directory */
      asm(
	  "movl %cr3,%eax\n"
	  "movl %eax,%cr3\n"
	);
    }
    pageTableSrc = (uInt32 *) (tablesBaseAddress + (0x1000 * x));
    if (y != 0x0) {
      for (y = 0x0;y<pageEntries;y++) {
        pageTableSrc[y] = (uInt32)0x0;
        }
      }
    for (y = 0; y < 1024; y++) {
      /* Loop Through The Page Table Find An UnAllocated Page */
      if ((pageTableSrc[y] & PAGE_COW) == PAGE_COW) {
        kprintf("PAGE_COW");
        //_current->td.vm_dsize += btoc(0x1000);
        /* HACK MEMORY LEAK */
        //pageTableSrc[y] = 0x0;
        }
      if ((uInt32) pageTableSrc[y] == (uInt32) 0x0) {
	if (count > 0x1) {
          for (c = 0; c < count; c++) {
            if (y + c < 1024) {
      if ((pageTableSrc[y + c] & PAGE_COW) == PAGE_COW) {
        kprintf("PAGE-COW");
        //_current->td.vm_dsize += btoc(0x1000);
        /* HACK MEMORY LEAK */
        //pageTableSrc[y + c] = 0x0;
        }

              if ((uInt32) pageTableSrc[y + c] != (uInt32) 0x0) {
                c = -1;
                break;
                }
              }
            }
	  if (c != -1) {
	    for (c = 0; c < count; c++) {
	      if ((vmm_remapPage((uInt32) vmmFindFreePage(pid), ((x * (1024 * 4096)) + ((y + c) * 4096)),PAGE_DEFAULT)) == 0x0)
	        kpanic("vmmRemapPage: getFreeVirtualPage-1: [0x%X]\n",((x * (1024 * 4096)) + ((y + c) * 4096)));
	      vmmClearVirtualPage((uInt32) ((x * (1024 * 4096)) + ((y + c) * 4096)));
	    }
            if (type == VM_THRD)
              _current->td.vm_dsize += btoc(count * 0x1000);
	    spinUnlock(&fvpSpinLock);
	    return ((void *)((x * (1024 * 4096)) + (y * 4096)));
	  }
	} else {
	  /* Map A Physical Page To The Virtual Page */

	  /*
	   * remapPage((uInt32)vmmFindFreePage(pid),((x*(1024*4096))+(y*4096))
	   * ,pid);
	   */
	  if ((vmm_remapPage((uInt32) vmmFindFreePage(pid), ((x * (1024 * 4096)) + (y * 4096)),PAGE_DEFAULT)) == 0x0)
	    kpanic("vmmRemapPage: getFreeVirtualPage-2\n");

	  /* Clear This Page So No Garbage Is There */
	  vmmClearVirtualPage((uInt32) ((x * (1024 * 4096)) + (y * 4096)));

	  /* Return The Address Of The Newly Allocate Page */
          if (type == VM_THRD) {
            _current->td.vm_dsize += btoc(count * 0x1000);
            kprintf("vm_dsize: [0x%X]][0x%X]\n",ctob(_current->td.vm_dsize),_current->td.vm_dsize);
            }
          //kprintf("(0x%X:0x%X)",_current->td.vm_dsize,vmm_getPhysicalAddr(((x * (1024 * 4096)) + (y * 4096))));
//          kprintf("(0x%X:0x%X)",_current->td.vm_dsize + _current->td.vm_daddr,((x * (1024 * 4096)) + (y * 4096)));
	  spinUnlock(&fvpSpinLock);
	  return ((void *)((x * (1024 * 4096)) + (y * 4096)));
	}
      }
    }
  }
  /* If No Free Page Was Found Return NULL */
  spinUnlock(&fvpSpinLock);
  return (0x0);
}

/***
 END
 ***/

