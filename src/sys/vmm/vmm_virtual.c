/*****************************************************************************************
 Copyright (c) 2002-2004,2005,2006,2007,2009 The UbixOS Project
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
#include <vmm/paging.h>
#include <ubixos/spinlock.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <lib/kprint.h>
#include <string.h>

int vmm_cleanVirtualSpace(u_int32_t addr) {
  int         x            = 0x0;
  int         y            = 0x0;
  u_int32_t  *pageTableSrc = 0x0;
  u_int32_t  *pageDir      = 0x0;

  pageDir = (u_int32_t *) PARENT_PAGEDIR_ADDR;

  #ifdef VMMDEBUG
  kprintf("Called: vmm_cleanVirtualSpace(0x%X)\n",addr);
  #endif

  /* Loop through the users virtual space flushing out COW pages */
  /* BUG memory leak this does not update COW counter on master page */
  for (x = (addr / (1024 * 4096)); x < 770; x++) {
    if ((pageDir[x] & PAGE_PRESENT) == PAGE_PRESENT) {
      pageTableSrc = (u_int32_t *) (PAGE_TABLES_BASE_ADDR + (0x1000 * x));
      for (y = 0;y < 1024;y++) {
        if (pageTableSrc[y] != 0x0) {
          if ((pageTableSrc[y] & PAGE_COW) == PAGE_COW) {
            //kprintf("COWi*");
            pageTableSrc[y] = 0x0;
            }
          else if ((pageTableSrc[y] & PAGE_STACK) == PAGE_STACK) {
            //pageTableSrc[y] = 0x0;
            //kprintf("STACK: (%i:%i)",x,y);
            }
          else {
            //kprintf("+");
            }
          }
        }
      }
    }

  /* we must now reload the page directory */
  asm(
    "movl %cr3,%eax\n"
    "movl %eax,%cr3\n"
    );

  return(0x0);
  } /* end func */

/************************************************************************

Function: void *vmmGetFreeVirtualPage(pidType pid,int count);
Description: Returns A Free Page Mapped To The VM Space
Notes:

08/11/02 - This Will Return Next Avilable Free Page Of Tasks VM Space

************************************************************************/
static spinLock_t fvpSpinLock = SPIN_LOCK_INITIALIZER;

void *vmmGetFreeVirtualPage(pidType pid, int count,int type) {
  int             x = 0, y = 0, c = 0;
  u_int32_t         *pageTableSrc = 0x0;
  u_int32_t         *pageDir = 0x0;
  u_int32_t          start_page = 0x0;


  spinLock(&fvpSpinLock);

  pageDir = (u_int32_t *)PARENT_PAGEDIR_ADDR;

  /* Lets Search For A Free Page */
  if (_current->oInfo.vmStart <= 0x100000)
    K_PANIC("Invalid vmStart\n");


  /* Set starting page based on page allocation type */
  if (type == VM_THRD) {
    start_page = (u_int32_t)(_current->td.vm_daddr + ctob(_current->td.vm_dsize));
    #ifdef DEBUG
    kprintf(".%i.",count);
    #endif
    //count--;
    }
  else if (type == VM_TASK) {
    //kprintf("vmStart");
    start_page = _current->oInfo.vmStart;
    }
  else
    K_PANIC("Invalid Type");

  for (x = (start_page / (1024 * 4096)); x < 1024; x++) {

    /* Set Page Table Address */
    if ((pageDir[x] & PAGE_PRESENT) != PAGE_PRESENT) {

      /* If Page Table Is Non Existant Then Set It Up */
      pageDir[x] = (u_int32_t) vmm_findFreePage(_current->id) | PAGE_DEFAULT;

      /* Also Add It To Virtual Space So We Can Make Changes Later */
      pageTableSrc = (u_int32_t *) (PAGE_TABLES_BASE_ADDR + (4096 * 767));
      pageTableSrc[x] = pageDir[x];
      y = 1;

      /* Reload Page Directory */
      asm(
        "movl %cr3,%eax\n"
        "movl %eax,%cr3\n"
        );
    }

    pageTableSrc = (u_int32_t *) (PAGE_TABLES_BASE_ADDR + (0x1000 * x));

    if (y != 0x0) {
      for (y = 0x0;y < PAGE_ENTRIES;y++) {
        pageTableSrc[y] = (u_int32_t)0x0;
        }
      }

    for (y = 0; y < 1024; y++) {

      /* Loop Through The Page Table Find An UnAllocated Page */
      if ((pageTableSrc[y] & PAGE_COW) == PAGE_COW) {
        #ifdef VMMDEBUG
        kprintf("PAGE_COW-1");
        #endif
        //_current->td.vm_dsize += btoc(0x1000);
        /* BUG MEMORY LEAK */
        //pageTableSrc[y] = 0x0;
        }

      if ((u_int32_t) pageTableSrc[y] == (u_int32_t) 0x0) {

        if (count > 0x1) {

          for (c = 0; c < count; c++) {

            if (y + c < 1024) {

              if ((pageTableSrc[y + c] & PAGE_COW) == PAGE_COW) {
                #ifdef VMMDEBUG
                kprintf("PAGE-COW-2");
                #endif
                //_current->td.vm_dsize += btoc(0x1000);
               /* BUG MEMORY LEAK */
               //pageTableSrc[y + c] = 0x0;
               }
              if ((u_int32_t) pageTableSrc[y + c] != (u_int32_t) 0x0) {
                c = -1;
                break;
                }
              }
            }
	  if (c != -1) {
	    for (c = 0; c < count; c++) {
	      if ((vmm_remapPage((u_int32_t) vmm_findFreePage(pid), ((x * (1024 * 4096)) + ((y + c) * 4096)),PAGE_DEFAULT)) == 0x0)
	        kpanic("vmmRemapPage: getFreeVirtualPage-1: [0x%X]\n",((x * (1024 * 4096)) + ((y + c) * 4096)));
	      if (vmm_zeroVirtualPage((u_int32_t) ((x * (1024 * 4096)) + ((y + c) * 4096))) == 1)
                K_PANIC("vmm_zeroVirtualPage Failed");
	      }
            if (type == VM_THRD)
              _current->td.vm_dsize += btoc(count * 0x1000);
	    spinUnlock(&fvpSpinLock);
	    return ((void *)((x * (1024 * 4096)) + (y * 4096)));
	    }
	  }
        else {
          /* Map A Physical Page To The Virtual Page */

	  /*
	   * remapPage((u_int32_t)vmm_findFreePage(pid),((x*(1024*4096))+(y*4096))
	   * ,pid);
	   */
	  if ((vmm_remapPage((u_int32_t) vmm_findFreePage(pid), ((x * (1024 * 4096)) + (y * 4096)),PAGE_DEFAULT)) == 0x0)
	    kpanic("vmmRemapPage: getFreeVirtualPage-2\n");

	  /* Clear This Page So No Garbage Is There */
	  if (vmm_zeroVirtualPage((u_int32_t) ((x * (1024 * 4096)) + (y * 4096))) == 0x1)
            K_PANIC("vmm_zeroVirtualPage Failed");

	  /* Return The Address Of The Newly Allocate Page */
          if (type == VM_THRD) {
            _current->td.vm_dsize += btoc(count * 0x1000);
            #ifdef VMMDEBUG2
            kprintf("vm_dsize: [0x%X]][0x%X]\n",ctob(_current->td.vm_dsize),_current->td.vm_dsize);
            #endif
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
  } /* end func */

void *vmmGetFreeVirtualPage_new(pidType pid, int count,int type,u_int32_t start_addr) {
  int             x = 0, y = 0, c = 0;
  u_int32_t         *pageTableSrc = 0x0;
  u_int32_t         *pageDir = 0x0;
  u_int32_t          start_page = 0x0;


  spinLock(&fvpSpinLock);

  pageDir = (u_int32_t *) PARENT_PAGEDIR_ADDR;

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
      pageDir[x] = (u_int32_t) vmm_findFreePage(_current->id) | PAGE_DEFAULT;
      /* Also Add It To Virtual Space So We Can Make Changes Later */
      pageTableSrc = (u_int32_t *) (PAGE_TABLES_BASE_ADDR + (4096 * 767));
      pageTableSrc[x] = pageDir[x];
      y = 1;
      /* Reload Page Directory */
      asm(
	  "movl %cr3,%eax\n"
	  "movl %eax,%cr3\n"
	);
    }
    pageTableSrc = (u_int32_t *) (PAGE_TABLES_BASE_ADDR + (0x1000 * x));
    if (y != 0x0) {
      for (y = 0x0;y < PAGE_ENTRIES;y++) {
        pageTableSrc[y] = (u_int32_t)0x0;
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
      if ((u_int32_t) pageTableSrc[y] == (u_int32_t) 0x0) {
	if (count > 0x1) {
          for (c = 0; c < count; c++) {
            if (y + c < 1024) {
      if ((pageTableSrc[y + c] & PAGE_COW) == PAGE_COW) {
        kprintf("PAGE-COW");
        //_current->td.vm_dsize += btoc(0x1000);
        /* HACK MEMORY LEAK */
        //pageTableSrc[y + c] = 0x0;
        }

              if ((u_int32_t) pageTableSrc[y + c] != (u_int32_t) 0x0) {
                c = -1;
                break;
                }
              }
            }
	  if (c != -1) {
	    for (c = 0; c < count; c++) {
	      if ((vmm_remapPage((u_int32_t) vmm_findFreePage(pid), ((x * (1024 * 4096)) + ((y + c) * 4096)),PAGE_DEFAULT)) == 0x0)
	        kpanic("vmmRemapPage: getFreeVirtualPage-1: [0x%X]\n",((x * (1024 * 4096)) + ((y + c) * 4096)));
	      if (vmm_zeroVirtualPage((u_int32_t) ((x * (1024 * 4096)) + ((y + c) * 4096))) == 0x1)
                K_PANIC("vmm_zeroVirtualPage failed");
	    }
            if (type == VM_THRD)
              _current->td.vm_dsize += btoc(count * 0x1000);
	    spinUnlock(&fvpSpinLock);
	    return ((void *)((x * (1024 * 4096)) + (y * 4096)));
	  }
	} else {
	  /* Map A Physical Page To The Virtual Page */

	  /*
	   * remapPage((u_int32_t)vmm_findFreePage(pid),((x*(1024*4096))+(y*4096))
	   * ,pid);
	   */
	  if ((vmm_remapPage((u_int32_t) vmm_findFreePage(pid), ((x * (1024 * 4096)) + (y * 4096)),PAGE_DEFAULT)) == 0x0)
	    kpanic("vmmRemapPage: getFreeVirtualPage-2\n");

	  /* Clear This Page So No Garbage Is There */
	  if (vmm_zeroVirtualPage((u_int32_t) ((x * (1024 * 4096)) + (y * 4096))) == 0x1)
            K_PANIC("vmm_zeroVirtualPage Failed");

	  /* Return The Address Of The Newly Allocate Page */
          if (type == VM_THRD) {
            _current->td.vm_dsize += btoc(count * 0x1000);
            #ifdef DEBUG
            kprintf("vm_dsize: [0x%X]][0x%X]\n",ctob(_current->td.vm_dsize),_current->td.vm_dsize);
            #endif
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

/************************************************************************

Function: void *vmmCopyVirtualSpace(pidType pid);

Description: Creates A Copy Of A Virtual Space And Set All NON Kernel
             Space To COW For A Fork This Will Also Alter The Parents
             VM Space To Make That COW As Well

Notes:

08/02/02 - Added Passing Of pidType pid So We Can Better Keep Track Of
           Which Task Has Which Physical Pages

************************************************************************/
static spinLock_t cvsSpinLock = SPIN_LOCK_INITIALIZER;

void *vmmCopyVirtualSpace(pidType pid) {
  void           *newPageDirectoryAddress = 0x0;
  u_int32_t         *parentPageDirectory = 0x0, *newPageDirectory = 0x0;
  u_int32_t         *parentPageTable = 0x0, *newPageTable = 0x0;
  u_int32_t         *parentStackPage = 0x0, *newStackPage = 0x0;
  uInt16          x = 0, i = 0, s = 0;

  spinLock(&cvsSpinLock);

  /* Set Address Of Parent Page Directory */
  parentPageDirectory = (u_int32_t *) PARENT_PAGEDIR_ADDR;
  /* Allocate A New Page For The New Page Directory */
  if ((newPageDirectory = (u_int32_t *) vmm_getFreeKernelPage(pid,1)) == 0x0)
    kpanic("Error: newPageDirectory == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);

  /* Set newPageDirectoryAddress To The Newly Created Page Directories Page */
  newPageDirectoryAddress = (void *)vmm_getPhysicalAddr((u_int32_t) newPageDirectory);

  /* First Set Up A Flushed Page Directory */
  if (vmm_zeroVirtualPage((u_int32_t)newPageDirectory) == 0x1)
    K_PANIC("ZVP: Failed");

  /* Map The Top 1GB Region Of The VM Space */
  for (x = 768; x < PAGE_ENTRIES; x++) {
    newPageDirectory[x] = parentPageDirectory[x];
    }

  /*
   * Now For The Fun Stuff For Page Tables 1-766 We Must Map These And Set
   * The Permissions On Every Mapped Pages To COW This Will Conserve Memory
   * Because The Two VM Spaces Will Be Sharing Some Pages
   */
  for (x = 0x1; x <= 766; x++) {
    /* If Page Table Exists Map It */
    if (parentPageDirectory[x] != 0) {
      /* Set Parent  To Propper Page Table */
      parentPageTable = (u_int32_t *) (PAGE_TABLES_BASE_ADDR + (0x1000 * x));
      /* Allocate A New Page Table */
      if ((newPageTable = (u_int32_t *) vmm_getFreeKernelPage(pid,1)) == 0x0)
        kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);

      /* Set Parent And New Pages To COW */
      for (i = 0; i < PAGE_ENTRIES; i++) {

	/* If Page Is Mapped */
	if ((parentPageTable[i] & 0xFFFFF000) != 0x0) {
	  /* Check To See If Its A Stack Page */
	  if (((u_int32_t) parentPageTable[i] & PAGE_STACK) == PAGE_STACK) {
	    /* Alloc A New Page For This Stack Page */
	    if ((newStackPage = (u_int32_t *) vmm_getFreeKernelPage(pid,1)) == 0x0)
	      kpanic("Error: newStackPage == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);

	    /* Set Pointer To Parents Stack Page */
	    parentStackPage = (u_int32_t *) (((1024 * 4096) * x) + (4096 * i));

	    /* Copy The Tack Byte For Byte (I Should Find A Faster Way) */
	    for (s = 0x0; s < PAGE_ENTRIES; s++) {
	      newStackPage[s] = parentStackPage[s];
	      }
	    /* Insert New Stack Into Page Table */
	    newPageTable[i] = (vmm_getPhysicalAddr((u_int32_t) newStackPage) | PAGE_DEFAULT | PAGE_STACK);
	    /* Unmap From Kernel Space */
	    vmm_unmapPages((u_int32_t) newStackPage, 1, 1);
  	    }
          else {
	    /* Set Page To COW In Parent And Child Space */
	    newPageTable[i] = (((u_int32_t) parentPageTable[i] & 0xFFFFF000) | (PAGE_DEFAULT | PAGE_COW));
	    /* Increment The COW Counter For This Page */
	    if (((u_int32_t) parentPageTable[i] & PAGE_COW) == PAGE_COW) {
	      vmm_adjustCowCounter(((u_int32_t) parentPageTable[i] & 0xFFFFF000), 1);
	      }
            else {
	      vmm_adjustCowCounter(((u_int32_t) parentPageTable[i] & 0xFFFFF000), 2);
	      parentPageTable[i] = newPageTable[i];
	      }
	    }
	  }
        else {
	  newPageTable[i] = (u_int32_t) 0x0;
	  }
        }

      /* Put New Page Table Into New Page Directory */
      newPageDirectory[x] = (vmm_getPhysicalAddr((u_int32_t) newPageTable) | PAGE_DEFAULT);
      /* Unmap Page From Kernel Space But Keep It Marked As Not Avail */
      vmm_unmapPages((u_int32_t) newPageTable, 1, 1);
    } else {
      newPageDirectory[x] = (u_int32_t) 0x0;
    }
  }
  /*
   * Allocate A New Page For The The First Page Table Where We Will Map The
   * Lower Region
   */
  if ((newPageTable = (u_int32_t *) vmm_getFreeKernelPage(pid,1)) == 0x0)
    kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);

  /* Flush The Page From Garbage In Memory */
  if (vmm_zeroVirtualPage((u_int32_t)newPageTable) == 0x1)
    K_PANIC("ZPE: Failed");

  /* Map This Into The Page Directory */
  newPageDirectory[0] = (vmm_getPhysicalAddr((u_int32_t) newPageTable) | PAGE_DEFAULT);
  /* Set Address Of Parents Page Table */
  parentPageTable = (u_int32_t *) PAGE_TABLES_BASE_ADDR;
  /* Map The First 1MB Worth Of Pages */
  for (x = 0; x < (PAGE_ENTRIES / 4); x++) {
    newPageTable[x] = parentPageTable[x];
  }
  /* Map The Next 3MB Worth Of Pages But Make Them COW */
  for (x = (PAGE_ENTRIES / 4) + 1; x < PAGE_ENTRIES; x++) {
    /* If Page Is Avaiable Map It */
    if ((parentPageTable[x] & 0xFFFFF000) != 0x0) {
      /* Set Pages To COW */
      newPageTable[x] = (((u_int32_t) parentPageTable[x] & 0xFFFFF000) | (PAGE_DEFAULT | PAGE_COW));
      /* Increment The COW Counter For This Page */
      if (((u_int32_t) parentPageTable[x] & PAGE_COW) == PAGE_COW) {
	vmm_adjustCowCounter(((u_int32_t) parentPageTable[x] & 0xFFFFF000), 1);
      } else {
	vmm_adjustCowCounter(((u_int32_t) parentPageTable[x] & 0xFFFFF000), 2);
	parentPageTable[x] = newPageTable[x];
      }
    } else {
      newPageTable[x] = (u_int32_t) 0x0;
    }
  }
  /* Set Virtual Mapping For Page Directory */
  newPageTable[256] = (vmm_getPhysicalAddr((u_int32_t) newPageDirectory) | PAGE_DEFAULT);

  /*
   * Now The Fun Stuff Build The Initial Virtual Page Space So We Don't Have
   * To Worry About Mapping Them In Later How Ever I'm Concerned This May
   * Become A Security Issue
   */
  /* First Lets Unmap The Previously Allocated Page Table */
  vmm_unmapPages((u_int32_t) newPageTable, 1, 1);
  /* Allocate A New Page Table */
  if ((newPageTable = (u_int32_t *) vmm_getFreeKernelPage(pid,1)) == 0x0)
    kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n",__FILE__,__LINE__);
  /* First Set Our Page Directory To Contain This */
  newPageDirectory[767] = vmm_getPhysicalAddr((u_int32_t) newPageTable) | PAGE_DEFAULT;
  /* Now Lets Build The Page Table */
  for (x = 0; x < PAGE_ENTRIES; x++) {
    newPageTable[x] = newPageDirectory[x];
  }
  /* Now We Are Done So Lets Unmap This Page */
  vmm_unmapPages((u_int32_t) newPageTable, 1, 1);
  /* Now We Are Done With The Page Directory So Lets Unmap That Too */
  vmm_unmapPages((u_int32_t) newPageDirectory, 1, 1);

  spinUnlock(&cvsSpinLock);

  /* Return Physical Address Of Page Directory */
  return (newPageDirectoryAddress);
  } /* end func */

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
void *vmmCreateVirtualSpace(pid_t pid) {
  void           *newPageDirectoryAddress = 0x0;
  u_int32_t         *parentPageDirectory = 0x0, *newPageDirectory = 0x0;
  u_int32_t         *parentPageTable = 0x0, *newPageTable = 0x0;
  int             x = 0;

  /* Set Address Of Parent Page Directory */
  parentPageDirectory = (u_int32_t *) PARENT_PAGEDIR_ADDR;

  /* Allocate A New Page For The New Page Directory */
  newPageDirectory = (u_int32_t *) vmm_getFreeKernelPage(pid,1);

  /* Set newPageDirectoryAddress To The Newly Created Page Directories Page */
  newPageDirectoryAddress = (void *)vmm_getPhysicalAddr((u_int32_t) newPageDirectory);

  /* First Set Up A Flushed Page Directory */
  for (x = 0; x < PAGE_ENTRIES; x++) {
    newPageDirectory[x] = 0x0;
    }

  /* Map The Top 1GB Region Of The VM Space */
  for (x = 768; x < PAGE_ENTRIES; x++) {
    newPageDirectory[x] = parentPageDirectory[x];
    }

  /*
   * Allocate A New Page For The The First Page Table Where We Will Map The
   * Lower Region
   */
  newPageTable = (u_int32_t *) vmm_getFreeKernelPage(pid,1);
  /* Flush The Page From Garbage In Memory */
  for (x = 0; x < PAGE_ENTRIES; x++) {
    newPageTable[x] = 0x0;
  }
  /* Map This Into The Page Directory */
  newPageDirectory[0] = (vmm_getPhysicalAddr((u_int32_t) newPageTable) | PAGE_DEFAULT);
  /* Set Address Of Parents Page Table */
  parentPageTable = (u_int32_t *) PAGE_TABLES_BASE_ADDR;
  /* Map The First 1MB Worth Of Pages */
  for (x = 0; x < (PAGE_ENTRIES / 4); x++) {
    newPageTable[x] = parentPageTable[x];
  }
  /* Set Virtual Mapping For Page Directory */
  newPageTable[256] = (vmm_getPhysicalAddr((u_int32_t) newPageDirectory) | PAGE_DEFAULT);

  /*
   * Now The Fun Stuff Build The Initial Virtual Page Space So We Don't Have
   * To Worry About Mapping Them In Later How Ever I'm Concerned This May
   * Become A Security Issue
   */
  /* First Lets Unmap The Previously Allocated Page Table */
  vmm_unmapPages((u_int32_t) newPageTable, 1, 1);
  /* Allocate A New Page Table */
  newPageTable = (u_int32_t *) vmm_getFreeKernelPage(pid,1);
  /* First Set Our Page Directory To Contain This */
  newPageDirectory[767] = vmm_getPhysicalAddr((u_int32_t) newPageTable) | PAGE_DEFAULT;
  /* Now Lets Build The Page Table */
  for (x = 0; x < PAGE_ENTRIES; x++) {
    newPageTable[x] = newPageDirectory[x];
  }
  /* Now We Are Done So Lets Unmap This Page */
  vmm_unmapPages((u_int32_t) newPageTable, 1, 1);
  /* Now We Are Done With The Page Directory So Lets Unmap That Too */
  vmm_unmapPages((u_int32_t) newPageDirectory, 1, 1);
  /* Return Physical Address Of Page Directory */
  return (newPageDirectoryAddress);
  } /* End Func */

/*
 END
 */
