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

#include <sys/types.h>
#include <vmm/vmm.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/kpanic.h>
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <string.h>
#include <assert.h>
#include <sys/descrip.h>
#include <ubixos/vitals.h>

uint32_t *kernelPageDirectory = 0x0; // Pointer To Kernel Page Directory

static struct spinLock fkpSpinLock = SPIN_LOCK_INITIALIZER;

static struct spinLock rmpSpinLock = SPIN_LOCK_INITIALIZER;

/*****************************************************************************************
 Function: int vmm_pagingInit();

 Description: This Function Will Initialize The Operating Systems Paging
 Abilities.

 Notes:
 02/20/2004 - Looked Over Code And Have Approved Its Quality
 07/28/3004 - All pages are set for ring-0 only no more user accessable

 *****************************************************************************************/

int vmm_pagingInit() {
  uint32_t i = 0x0;
  uint32_t *pageTable = 0x0;

  /* Allocate A Page Of Memory For Kernels Page Directory */
  kernelPageDirectory = (uint32_t *) vmm_findFreePage( sysID);

  if (kernelPageDirectory == 0x0) {
    K_PANIC("Error: vmm_findFreePage Failed");
  } /* end if */

  /* Clear The Memory To Ensure There Is No Garbage */
  for (i = 0; i < PD_ENTRIES; i++) {
    kernelPageDirectory[i] = (uint32_t) 0x0;
  } /* end for */

  /* Allocate a page for the first 4MB of memory */
  if ((pageTable = (uint32_t *) vmm_findFreePage( sysID)) == 0x0)
    K_PANIC("Error: vmm_findFreePage Failed");

  /* Make Sure The Page Table Is Clean */
  memset(pageTable, 0x0, 0x1000);

  kernelPageDirectory[0] = (uint32_t) ((uint32_t) (pageTable) | PAGE_DEFAULT);

  /*
   * Map the first 1MB of Memory to the kernel MM space because our kernel starts
   * at 0x30000
   * Do not map page at address 0x0 this is reserved for null...
   */

  /* MrOlsen (2016-01-15) NOTE: I'm Mappying It For Now Until I Can Figure Out Why FS:0x0 */
  for (i = 0x0; i < (PD_ENTRIES / 0x4); i++) {
    pageTable[i] = (uint32_t) ((i * 0x1000) | KERNEL_PAGE_DEFAULT);  //MrOlsen 2018-01-14 PAGE_DEFAULT
  } /* end for */

  /*
   * Create page tables for the top 1GB of VM space. This space is set aside
   * for kernel space and will be shared with each process
   */

  kprintf("PD: %i\n", PD_INDEX(VMM_KERN_START));

  for (i = PD_INDEX(VMM_KERN_START); i <= PD_INDEX(VMM_KERN_END); i++) {
    if ((pageTable = (uint32_t *) vmm_findFreePage( sysID)) == 0x0)
      K_PANIC("Error: vmm_findFreePage Failed");

    /* Make Sure The Page Table Is Clean */
    memset(pageTable, 0x0, 0x1000);

    /* Map In The Page Directory */
    kernelPageDirectory[i] = (uint32_t) ((uint32_t) (pageTable) | KERNEL_PAGE_DEFAULT | PAGE_GLOBAL);
  } /* end for */

  kernelPageDirectory[1023] = (uint32_t) ((uint32_t) (pageTable) | KERNEL_PAGE_DEFAULT);
  pageTable = (uint32_t *) (kernelPageDirectory[1023] & 0xFFFFF000);
  pageTable[1023] = (vmm_findFreePage(sysID) | KERNEL_PAGE_DEFAULT | PAGE_STACK);
  pageTable[1022] = (vmm_findFreePage(sysID) | KERNEL_PAGE_DEFAULT | PAGE_STACK);

  /*
   * Map Page Tables Into VM Space
   * The First Page Table (4MB) Maps To All Page Directories
   */
  if (kernelPageDirectory[PD_INDEX(PT_BASE_ADDR)] == 0) {
    if ((pageTable = (uint32_t *) vmm_findFreePage( sysID)) == 0x0)
      K_PANIC("Error: vmm_findFreePage Failed");

    kernelPageDirectory[PD_INDEX(PT_BASE_ADDR)] = (uint32_t) ((uint32_t) (pageTable) | KERNEL_PAGE_DEFAULT);
  }

  kprintf("PD2: %i\n", PD_INDEX(PT_BASE_ADDR));
  pageTable = (uint32_t *) (kernelPageDirectory[PD_INDEX(PT_BASE_ADDR)] & 0xFFFFF000);

  for (i = 0; i < PD_ENTRIES; i++) {
    pageTable[i] = kernelPageDirectory[i];
  } /* end for */

  /*
   * Map Page Directory Into VM Space
   * First Page After Page Tables
   */
  kprintf("PPD3: %i\n", PD_INDEX(PD_BASE_ADDR));
  if (kernelPageDirectory[PD_INDEX(PD_BASE_ADDR)] == 0) {
    if ((pageTable = (uint32_t *) vmm_findFreePage( sysID)) == 0x0)
      K_PANIC("Error: vmm_findFreePage Failed");

    kernelPageDirectory[PD_INDEX(PD_BASE_ADDR)] = (uint32_t) ((uint32_t) (pageTable) | KERNEL_PAGE_DEFAULT);
  }

  pageTable = (uint32_t *) (kernelPageDirectory[PD_INDEX(PD_BASE_ADDR)] & 0xFFFFF000);
  pageTable[0] = (uint32_t) ((uint32_t) (kernelPageDirectory) | PAGE_DEFAULT);

  /* Also Set Up Page Directory To Be The The First Page In 0xE0400000 */
  /*
   pageTable = (uint32_t *) (kernelPageDirectory[0] & 0xFFFFF000);

   pageTable[256] = (uint32_t)( (uint32_t)( kernelPageDirectory ) | PAGE_DEFAULT );
   */

  /* Allocate New Stack Space */
  if ((pageTable = (uint32_t *) vmm_findFreePage(sysID)) == 0x0)
    K_PANIC("ERROR: vmm_findFreePage Failed");

  /* Now Lets Turn On Paging With This Initial Page Table */
  asm volatile(
    "movl %0,%%eax          \n"
    "movl %%eax,%%cr3       \n"
    "movl %%cr0,%%eax       \n"
    "orl  $0x80010000,%%eax \n" /* Turn on memory protection */
    "movl %%eax,%%cr0       \n"
    :
    : "d"((uint32_t *) (kernelPageDirectory))
  );

  /* Remap The Memory List */
  for (i = 0x101000; i <= (0x101000 + (numPages * sizeof(mMap))); i += 0x1000) {
    if ((vmm_remapPage(i, (VMM_MMAP_ADDR_PMODE + (i - 0x101000)), PAGE_DEFAULT, sysID, 0)) == 0x0)
      K_PANIC("vmmRemapPage failed\n");
  }

  /* Set New Address For Memory Map Since Its Relocation */
  vmmMemoryMap = (mMap *) VMM_MMAP_ADDR_PMODE;

  /* Print information on paging */
  kprintf("paging0 - Address: [0x%X], PagingISR Address: [0x%X]\n", kernelPageDirectory, &_vmm_pageFault);

  /* Return so we know everything went well */
  return (0x0);
} /* END */

/*****************************************************************************************
 Function: int vmmRemapPage(Physical Source,Virtual Destination)

 Description: This Function Will Remap A Physical Page Into Virtual Space

 Notes:
 07/29/02 - Rewrote This To Work With Our New Paging System
 07/30/02 - Changed Address Of Page Tables And Page Directory
 07/28/04 - If perms == 0x0 set to PAGE_DEFAULT

 *****************************************************************************************/
int vmm_remapPage(uint32_t source, uint32_t dest, uint16_t perms, pidType pid, int haveLock) {

  uint16_t destPageDirectoryIndex = 0x0, destPageTableIndex = 0x0;
  uint32_t *pageDir = 0x0, *pageTable = 0x0;

  short i = 0x0;

  if (pid < sysID)
    kpanic("Invalid PID %i", pid);

  if (source == 0x0)
    K_PANIC("source == 0x0");

  if (dest == 0x0)
    K_PANIC("dest == 0x0");

  if (haveLock == 0) {
  if (dest >= VMM_USER_START && dest <= VMM_USER_END)
    spinLock(&rmpSpinLock);
  else
    spinLock(&pdSpinLock);
  }

  if (perms == 0x0)
    perms = KERNEL_PAGE_DEFAULT;

  /* Set Pointer pageDirectory To Point To The Virtual Mapping Of The Page Directory */
  pageDir = (uint32_t *) PD_BASE_ADDR;

  /* Get Index Into The Page Directory */
  destPageDirectoryIndex = PD_INDEX(dest);

  if ((pageDir[destPageDirectoryIndex] & PAGE_PRESENT) != PAGE_PRESENT) {
    vmm_allocPageTable(destPageDirectoryIndex, pid);
  }

  /* Set Address To Page Table */
  pageTable = (uint32_t *) (PT_BASE_ADDR + (PAGE_SIZE * destPageDirectoryIndex));

  /* Get The Index To The Page Table */
  destPageTableIndex = PT_INDEX(dest);

  /* If The Page Is Mapped In Free It Before We Remap */
  if ((pageTable[destPageTableIndex] & PAGE_PRESENT) == PAGE_PRESENT) {
    kpanic("A Page Is Already Mapped Here: 0x%X:0x%X", dest, destPageTableIndex);

    if ((pageTable[destPageTableIndex] & PAGE_STACK) == PAGE_STACK)
      kprintf("Stack Page: [0x%X]\n", dest);

    if ((pageTable[destPageTableIndex] & PAGE_COW) != PAGE_COW) {
      kprintf("Page NOT COW\n");
      kprintf("Page Present: [0x%X][0x%X]", dest, pageTable[destPageTableIndex]);
      source = 0x0;
      goto rmDone;
    }

    /* Clear The Page First For Security Reasons */
    freePage(((uint32_t) pageTable[destPageTableIndex] & 0xFFFFF000));

  }

  /* Set The Source Address In The Destination */
  pageTable[destPageTableIndex] = (uint32_t) (source | perms);

  /* Reload The Page Table; */
  asm volatile(
    "push %eax     \n"
    "movl %cr3,%eax\n"
    "movl %eax,%cr3\n"
    "pop  %eax     \n"
  );

  rmDone:
  /* Return */
  if (haveLock == 0x0) {
  if (dest >= VMM_USER_START && dest <= VMM_USER_END)
    spinUnlock(&rmpSpinLock);
  else
    spinUnlock(&pdSpinLock);
  }
  return (source);
}

/************************************************************************

 Function: void *vmm_getFreeKernelPage(pidType pid, uint16_t count);
 Description: Returns A Free Page Mapped To The VM Space
 Notes:

 07/30/02 - This Returns A Free Page In The Kernel Space

 ***********************************************************************/
void *vmm_getFreeKernelPage(pidType pid, uint16_t count) {
  int pdI = 0x0, ptI = 0x0, c = 0, lc = 0;

  uint32_t *pageDirectory = PD_BASE_ADDR;

  uint32_t *pageTable = 0x0;

  uint32_t startAddress = 0x0;

  /* Lock The Page Dir & Tables For All Since Kernel Space Is Shared */
  spinLock(&pdSpinLock);

  /* Lets Search For A Free Page */
  for (pdI = PD_INDEX(VMM_KERN_START); pdI <= PD_INDEX(VMM_KERN_END); pdI++) {

    if ((pageDirectory[pdI] & PAGE_PRESENT) != PAGE_PRESENT)
      if (vmm_allocPageTable(pdI, pid) == -1)
        kpanic("Failed To Allocate Page Dir: 0x%X", pdI);

    /* Set Page Table Address */
    pageTable = (uint32_t *) (PT_BASE_ADDR + (PAGE_SIZE * pdI));

    /* Loop Through The Page Table For A Free Page */
    for (ptI = 0; ptI < PT_ENTRIES; ptI++) {

      /* Check For Unalocated Page */
      if ((pageTable[ptI] & PAGE_PRESENT) != PAGE_PRESENT) {
        if (startAddress == 0x0)
          startAddress = ((pdI * (PD_ENTRIES * PAGE_SIZE)) + (ptI * PAGE_SIZE));
        c++;
      }
      else {
        startAddress = 0x0;
        c = 0;
      }

      if (c == count)
        goto gotPages;

    }
  }

  startAddress = 0x0;
  goto noPagesAvail;

  gotPages: 
for (c = 0; c < count; c++) {
    if ((vmm_remapPage((uint32_t) vmm_findFreePage(pid), (startAddress + (PAGE_SIZE * c)), KERNEL_PAGE_DEFAULT, pid, 1)) == 0x0)
      K_PANIC("vmmRemapPage failed: gfkp-1\n");

    vmm_clearVirtualPage((uint32_t) (startAddress + (PAGE_SIZE * c)));

  }

  noPagesAvail: 
spinUnlock(&pdSpinLock);

  return (startAddress);
}

/************************************************************************

 Function: void vmm_clearVirtualPage(uint32_t pageAddr);

 Description: This Will Null Out A Page Of Memory

 Notes:

 ************************************************************************/
int vmm_clearVirtualPage(uint32_t pageAddr) {
  uint32_t *src = 0x0;
  int counter = 0x0;

  /* Set Source Pointer To Virtual Page Address */
  src = (uint32_t *) pageAddr;

  /* Clear Out The Page */
  for (counter = 0x0; counter < PD_ENTRIES; counter++) {
    src[counter] = (uint32_t) 0x0;
  }

  /* Return */
  return (0x0);
}

void *vmm_mapFromTask(pidType pid, void *ptr, uint32_t size) {
  kTask_t *child = 0x0;
  uint32_t i = 0x0, x = 0x0, y = 0x0, count = ((size + 4095) / 0x1000), c = 0x0;
  uInt16 dI = 0x0, tI = 0x0;
  uint32_t baseAddr = 0x0, offset = 0x0;
  uint32_t *childPageDir = (uint32_t *) 0x5A00000;
  uint32_t *childPageTable = 0x0;
  uint32_t *pageTableSrc = 0x0;
  offset = (uint32_t) ptr & 0xFFF;
  baseAddr = (uint32_t) ptr & 0xFFFFF000;
  child = schedFindTask(pid);
  //Calculate The Page Table Index And Page Directory Index
  dI = (baseAddr / (1024 * 4096));
  tI = ((baseAddr - (dI * (1024 * 4096))) / 4096);

  if (vmm_remapPage(child->tss.cr3, 0x5A00000, KERNEL_PAGE_DEFAULT, _current->id, 0) == 0x0)
    K_PANIC("vmm_remapPage: Failed");

  for (i = 0; i < 0x1000; i++) {
    if (vmm_remapPage(childPageDir[i], 0x5A01000 + (i * 0x1000), KERNEL_PAGE_DEFAULT, _current->id, 0) == 0x0)
      K_PANIC("Returned NULL");
  }
  for (x = (_current->oInfo.vmStart / (1024 * 4096)); x < 1024; x++) {
    kpanic("v_mFT");
    pageTableSrc = (uint32_t *) (PT_BASE_ADDR + (4096 * x));

    for (y = 0; y < 1024; y++) {

      //Loop Through The Page Table Find An UnAllocated Page
      if ((uint32_t) pageTableSrc[y] == (uint32_t) 0x0) {

        if (count > 1) {

          for (c = 0; ((c < count) && (y + c < 1024)); c++) {

            if ((uint32_t) pageTableSrc[y + c] != (uint32_t) 0x0) {

              c = -1;
              break;

            }

          }

          if (c != -1) {

            for (c = 0; c < count; c++) {

              if ((tI + c) >= 0x1000) {

                dI++;
                tI = 0 - c;

              }

              childPageTable = (uint32_t *) (0x5A01000 + (0x1000 * dI));

              if (vmm_remapPage(childPageTable[tI + c], ((x * (1024 * 4096)) + ((y + c) * 4096)), KERNEL_PAGE_DEFAULT, _current->id, 0) == 0x0)
                K_PANIC("remap == NULL");

            }

            vmm_unmapPage(0x5A00000, 1);

            for (i = 0; i < 0x1000; i++) {
              vmm_unmapPage((0x5A01000 + (i * 0x1000)), 1);
            }

            return ((void *) ((x * (1024 * 4096)) + (y * 4096) + offset));

          }

        }
        else {

          //Map A Physical Page To The Virtual Page
          childPageTable = (uint32_t *) (0x5A01000 + (0x1000 * dI));

          if (vmm_remapPage(childPageTable[tI], ((x * (1024 * 4096)) + (y * 4096)), KERNEL_PAGE_DEFAULT, _current->id, 0) == 0x0)
            K_PANIC("remap Failed");

          //Return The Address Of The Mapped In Memory
          vmm_unmapPage(0x5A00000, 1);

          for (i = 0; i < 0x1000; i++) {
            vmm_unmapPage((0x5A01000 + (i * 0x1000)), 1);
          }

          return ((void *) ((x * (1024 * 4096)) + (y * 4096) + offset));

        }

      }

    }

  }

  return (0x0);
}

void *vmm_getFreeMallocPage(uInt16 count) {
  uInt16 x = 0x0, y = 0x0;
  int c = 0x0;
  uint32_t *pageTableSrc = 0x0;
  uint32_t *pageDirectory = 0x0;

  pageDirectory = (uint32_t *) PD_BASE_ADDR;

  spinLock(&fkpSpinLock);

  /* Lets Search For A Free Page */
  for (x = PD_INDEX(VMM_KERN_START); x <= PD_INDEX(VMM_KERN_END); x++) {

    if ((pageDirectory[x] & PAGE_PRESENT) != PAGE_PRESENT) { /* If Page Directory Is Not Yet Allocated Allocate It */

      pageDirectory[x] = (uint32_t) vmm_findFreePage(_current->id) | PAGE_DEFAULT;

      /* Also Add It To Virtual Space So We Can Make Changes Later */
      pageTableSrc = (uint32_t *) (PT_BASE_ADDR + (PD_INDEX( PT_BASE_ADDR ) * 0x1000)); /* Table that maps that 4MB */

      pageTableSrc[x] = (pageDirectory[x] & 0xFFFFF000) | PAGE_DEFAULT;
      pageTableSrc = (uint32_t *) (PT_BASE_ADDR + (x * 0x1000));

      /* Reload Page Directory */
      asm(
        "movl %cr3,%eax\n"
        "movl %eax,%cr3\n"
      );

      bzero(pageTableSrc, PAGE_SIZE);
    }

    /* Set Page Table Address */
    pageTableSrc = (uint32_t *) (PT_BASE_ADDR + (0x1000 * x));

    for (y = 0; y < 1024; y++) {
      /* Loop Through The Page Table Find An UnAllocated Page */
      if ((uint32_t) pageTableSrc[y] == (uint32_t) 0x0) {
        if (count > 1) {
          for (c = 0; c < count; c++) {
            if (y + c < 1024) {
              if ((uint32_t) pageTableSrc[y + c] != (uint32_t) 0x0) {
                c = -1;
                break;
              }
            }
          }
          if (c != -1) {
            for (c = 0; c < count; c++) {
              if (vmm_remapPage((uint32_t) vmm_findFreePage( sysID), ((x * 0x400000) + ((y + c) * 0x1000)), KERNEL_PAGE_DEFAULT, sysID, 0) == 0x0)
                K_PANIC("remap Failed");

              vmm_clearVirtualPage((uint32_t) ((x * 0x400000) + ((y + c) * 0x1000)));
            }
            spinUnlock(&fkpSpinLock);
            return ((void *) ((x * (PAGE_SIZE * PD_ENTRIES)) + (y * PAGE_SIZE)));
          }
        }
        else {
          /* Map A Physical Page To The Virtual Page */
          if (vmm_remapPage((uint32_t) vmm_findFreePage( sysID), ((x * 0x400000) + (y * 0x1000)), KERNEL_PAGE_DEFAULT, sysID, 0) == 0x0)
            K_PANIC("Failed");

          /* Clear This Page So No Garbage Is There */
          vmm_clearVirtualPage((uint32_t) ((x * 0x400000) + (y * 0x1000)));
          /* Return The Address Of The Newly Allocate Page */
          spinUnlock(&fkpSpinLock);
          return ((void *) ((x * (PAGE_SIZE * PD_ENTRIES)) + (y * PAGE_SIZE)));
        }
      }
    }
  }
  /* If No Free Page Was Found Return NULL */
  spinUnlock(&fkpSpinLock);
  return (0x0);
}

int obreak(struct thread *td, struct obreak_args *uap) {
  uint32_t i = 0x0;
  vm_offset_t old = 0x0;
  vm_offset_t base = 0x0;
  vm_offset_t new = 0x0;

  /*
   #ifdef _VMM_DEBUG
   */
  kprintf("vm_offset_t: [%i]\n", sizeof(vm_offset_t));
  kprintf("nsize:    [0x%X]\n", uap->nsize);
  kprintf("vm_daddr: [0x%X]\n", td->vm_daddr);
  kprintf("vm_dsize: [0x%X]\n", td->vm_dsize);
  kprintf("total:    [0x%X]\n", td->vm_daddr + td->vm_dsize);
  /*
   #endif
   */

  new = round_page((vm_offset_t )uap->nsize);

  base = round_page((vm_offset_t ) td->vm_daddr);

  old = base + ctob(td->vm_dsize);

  if (new < base)
    K_PANIC("EINVAL");

  if (new > old) {
    for (i = old; i < new; i += 0x1000) {
      if (vmm_remapPage(vmm_findFreePage(_current->id), i, PAGE_DEFAULT, _current->id, 0) == 0x0)
        K_PANIC("remap Failed");
    }
    td->vm_dsize += btoc(new - old);
  }
  else if (new < old) {
    K_PANIC("new < old");
    td->vm_dsize -= btoc(old - new);
  }

  return (0x0);
}

int vmm_cleanVirtualSpace(uint32_t addr) {
  int x = 0x0;
  int y = 0x0;

  uint32_t *pageTableSrc = 0x0;
  uint32_t *pageDir = 0x0;

  pageDir = (uint32_t *) PD_BASE_ADDR;
  kprintf("PDE*PS: 0x%X", (PD_ENTRIES * PAGE_SIZE));

  for (x = (addr / (PD_ENTRIES * PAGE_SIZE)); x <= PD_INDEX(VMM_USER_END); x++) {
    if ((pageDir[x] & PAGE_PRESENT) == PAGE_PRESENT) {
      pageTableSrc = (uint32_t *) (PT_BASE_ADDR + (PAGE_SIZE * x));

      for (y = 0; y < PT_ENTRIES; y++) {
        if ((pageTableSrc[y] & PAGE_PRESENT) == PAGE_PRESENT) {
          if ((pageTableSrc[y] & PAGE_COW) == PAGE_COW) {
            adjustCowCounter(((uint32_t) pageTableSrc[y] & 0xFFFFF000), -1);
            pageTableSrc[y] = 0x0;
          }
/*
          else if ((pageTableSrc[y] & PAGE_STACK) == PAGE_STACK) {
            //TODO: We need to fix this so we can clean the stack!
            //kprintf("Page Stack!: 0x%X", (x * 0x400000) + (y * 0x1000));
            // pageTableSrc[y] = 0x0;
            //MrOlsen (2016-01-18) NOTE: WHat should I Do Here? kprintf( "STACK: (%i:%i)", x, y );
          }
*/
          else {
/*
int vmmMemoryMapIndex = ((pageTableSrc[y] & 0xFFFFF000) / 4096);
    vmmMemoryMap[vmmMemoryMapIndex].cowCounter = 0x0;
    vmmMemoryMap[vmmMemoryMapIndex].pid = vmmID;
    vmmMemoryMap[vmmMemoryMapIndex].status = memAvail;
    systemVitals->freePages++;
*/
            pageTableSrc[y] = 0x0;
          }
        }
      }
    }
  }

  asm(
    "movl %cr3,%eax\n"
    "movl %eax,%cr3\n"
  );
kprintf("Here?");

  return (0x0);
}
