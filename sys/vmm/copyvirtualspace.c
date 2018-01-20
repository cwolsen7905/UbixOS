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
#include <vmm/paging.h>
#include <sys/kern_sysctl.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <string.h>

static struct spinLock cvsSpinLock = SPIN_LOCK_INITIALIZER;

/************************************************************************

 Function: void *vmm_copyVirtualSpace(pidType pid);

 Description: Creates A Copy Of A Virtual Space And Set All NON Kernel
 Space To COW For A Fork This Will Also Alter The Parents
 VM Space To Make That COW As Well

 Notes:

 08/02/02 - Added Passing Of pidType pid So We Can Better Keep Track Of
 Which Task Has Which Physical Pages

 ************************************************************************/
void *vmm_copyVirtualSpace(pidType pid) {
  void *newPageDirectoryAddress = 0x0;

  uint32_t *parentPageDirectory = 0x0, *newPageDirectory = 0x0;
  uint32_t *parentPageTable = 0x0, *newPageTable = 0x0;
  uint32_t *parentStackPage = 0x0, *newStackPage = 0x0;
  uint16_t x = 0, i = 0, s = 0;

  spinLock(&cvsSpinLock);

  /* Set Address Of Parent Page Directory */
  parentPageDirectory = (uint32_t *) PD_BASE_ADDR;

  /* Allocate A New Page For The New Page Directory */
  if ((newPageDirectory = (uint32_t *) vmm_getFreeKernelPage(pid, 1)) == 0x0)
    kpanic("Error: newPageDirectory == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);

  /* Set newPageDirectoryAddress To The Newly Created Page Directories Page */
  newPageDirectoryAddress = (void *) vmm_getPhysicalAddr((uint32_t) newPageDirectory);

  /* First Set Up A Flushed Page Directory */
  bzero(newPageDirectory, PAGE_SIZE);

  /* Map Kernel Code Region Entries 0 & 1 */
  newPageDirectory[0] = parentPageDirectory[0];
  //XXX: We Dont Need This - newPageDirectory[1] = parentPageDirectory[1];

  if ((newPageTable = (uint32_t *) vmm_getFreeKernelPage(pid, 1)) == 0x0)
    kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);

  parentPageTable = (uint32_t *) (PT_BASE_ADDR + (PAGE_SIZE * 1));

  for (x = 0; x < PT_ENTRIES; x++) {
    if (((parentPageTable[x]) & PAGE_PRESENT) == PAGE_PRESENT) {

      /* Set Page To COW In Parent And Child Space */
      newPageTable[x] = (((uint32_t) parentPageTable[x] & 0xFFFFF000) | (KERNEL_PAGE_DEFAULT | PAGE_COW));

      /* Increment The COW Counter For This Page */
      if (((uint32_t) parentPageTable[x] & PAGE_COW) == PAGE_COW) {
        adjustCowCounter(((uint32_t) parentPageTable[x] & 0xFFFFF000), 1);
      }
      else {
        /* Add Two If This Is The First Time Setting To COW */
        adjustCowCounter(((uint32_t) parentPageTable[x] & 0xFFFFF000), 2);
        parentPageTable[x] |= PAGE_COW; // newPageTable[i];
      }

    }
    else
      newPageTable[x] = parentPageTable[x];
  }

  newPageDirectory[1] = (vmm_getPhysicalAddr((uint32_t) newPageTable) | KERNEL_PAGE_DEFAULT);

  vmm_unmapPage((uint32_t) newPageTable, 1);

  newPageTable = 0x0;

  /* Map The Kernel Memory Region Entry 770 Address 0xC0800000 */
  for (x = PD_INDEX(VMM_KERN_START); x <= PD_INDEX(VMM_KERN_END); x++)
    newPageDirectory[x] = parentPageDirectory[x];

  /* Map The Kernel Stack Region */
  for (x = PD_INDEX(VMM_KERN_STACK_START); x <= PD_INDEX(VMM_KERN_STACK_END); x++) {
    if ((parentPageDirectory[x] & PAGE_PRESENT) == PAGE_PRESENT) {
      /* Set Parent To Propper Page Table */
      parentPageTable = (uint32_t *) (PT_BASE_ADDR + (PAGE_SIZE * x));

      /* Allocate A New Page Table */
      if ((newPageTable = (uint32_t *) vmm_getFreeKernelPage(pid, 1)) == 0x0)
        kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);

      bzero(newPageTable, PAGE_SIZE);

      for (i = 0; i < PT_ENTRIES; i++) {
        if ((parentPageTable[i] & PAGE_PRESENT) == PAGE_PRESENT) {

          /* Alloc A New Page For This Stack Page */
          if ((newStackPage = (uint32_t *) vmm_getFreeKernelPage(pid, 1)) == 0x0)
            kpanic("Error: newStackPage == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);

          /* Set Pointer To Parents Stack Page */
          parentStackPage = (uint32_t *) (((PAGE_SIZE * PD_ENTRIES) * x) + (PAGE_SIZE * i));

          /* Copy The Stack Byte For Byte (I Should Find A Faster Way) */
          memcpy(newStackPage, parentStackPage, PAGE_SIZE);

          /* Insert New Stack Into Page Table */
          newPageTable[i] = (vmm_getPhysicalAddr((uint32_t) newStackPage) | PAGE_DEFAULT | PAGE_STACK);

          /* Unmap From Kernel Space */
          vmm_unmapPage((uint32_t) newStackPage, 1);
        }
      }
      /* Put New Page Table Into New Page Directory */
      newPageDirectory[x] = (vmm_getPhysicalAddr((uint32_t) newPageTable) | PAGE_DEFAULT);
      /* Unmap Page From Kernel Space But Keep It Marked As Not Avail */
      vmm_unmapPage((uint32_t) newPageTable, 1);
    }
  }

  /*
   * Now For The Fun Stuff For Page Tables 2-767 We Must Map These And Set
   * The Permissions On Every Mapped Pages To COW This Will Conserve Memory
   * Because The Two VM Spaces Will Be Sharing Pages Unless an EXECVE Happens
   *
   * We start at the 4MB boundary as the first 4MB is special
   */

  for (x = PD_INDEX(VMM_USER_START); x <= PD_INDEX(VMM_USER_END); x++) {

    /* If Page Table Exists Map It */
    if ((parentPageDirectory[x] & PAGE_PRESENT) == PAGE_PRESENT) {

      /* Set Parent To Propper Page Table */
      parentPageTable = (uint32_t *) (PT_BASE_ADDR + (PAGE_SIZE * x));

      /* Allocate A New Page Table */
      if ((newPageTable = (uint32_t *) vmm_getFreeKernelPage(pid, 1)) == 0x0)
        kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);

      bzero(newPageTable, PAGE_SIZE);

      /* Set Parent And New Pages To COW */
      for (i = 0; i < PD_ENTRIES; i++) {

        /* If Page Is Mapped */
        if ((parentPageTable[i] & PAGE_PRESENT) == PAGE_PRESENT) {

          /* Check To See If Its A Stack Page */
          if (((uint32_t) parentPageTable[i] & PAGE_STACK) == PAGE_STACK) {

            /* Alloc A New Page For This Stack Page */
            if ((newStackPage = (uint32_t *) vmm_getFreeKernelPage(pid, 1)) == 0x0)
              kpanic("Error: newStackPage == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);

            /* Set Pointer To Parents Stack Page */
            parentStackPage = (uint32_t *) (((PAGE_SIZE * PD_ENTRIES) * x) + (PAGE_SIZE * i));

            /* Copy The Stack Byte For Byte (I Should Find A Faster Way) */
            memcpy(newStackPage, parentStackPage, PAGE_SIZE);

            /* Insert New Stack Into Page Table */
            newPageTable[i] = (vmm_getPhysicalAddr((uint32_t) newStackPage) | PAGE_DEFAULT | PAGE_STACK);

            /* Unmap From Kernel Space */
            vmm_unmapPage((uint32_t) newStackPage, 1);

          }
          else {

            /* Set Page To COW In Parent And Child Space */
            newPageTable[i] = (((uint32_t) parentPageTable[i] & 0xFFFFF000) | (PAGE_DEFAULT | PAGE_COW));

            /* Increment The COW Counter For This Page */
            if (((uint32_t) parentPageTable[i] & PAGE_COW) == PAGE_COW) {
              adjustCowCounter(((uint32_t) parentPageTable[i] & 0xFFFFF000), 1);
            }
            else {
              /* Add Two If This Is The First Time Setting To COW */
              adjustCowCounter(((uint32_t) parentPageTable[i] & 0xFFFFF000), 2);
              parentPageTable[i] |= PAGE_COW; // newPageTable[i];
            }
          }
        }
        else {
          newPageTable[i] = (uint32_t) 0x0;
        }
      }

      /* Put New Page Table Into New Page Directory */
      newPageDirectory[x] = (vmm_getPhysicalAddr((uint32_t) newPageTable) | PAGE_DEFAULT);
      /* Unmap Page From Kernel Space But Keep It Marked As Not Avail */
      vmm_unmapPage((uint32_t) newPageTable, 1);
    }
  }

  /*
   * Allocate A New Page For The The First Page Table Where We Will Map The
   * Lower Region First 4MB
   */
  /*
   if ((newPageTable = (uint32_t *) vmm_getFreeKernelPage(pid, 1)) == 0x0)
   kpanic("Error: newPageTable == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
   */

  /* Flush The Page From Garbage In Memory */
  /*
   bzero(newPageTable, PAGE_SIZE);
   */

  /* Map This Into The Page Directory */
  /*
   newPageDirectory[0] = (vmm_getPhysicalAddr((uint32_t) newPageTable) | PAGE_DEFAULT);
   */

  /* Set Address Of Parents Page Table */
  /*
   parentPageTable = (uint32_t) PT_BASE_ADDR;
   */

  /* Map The First 1MB Worth Of Pages */
  /*
   for (x = 0; x < (PD_ENTRIES / 4); x++) {
   newPageTable[x] = parentPageTable[x];
   }
   */

  /* Map The Next 3MB Worth Of Pages But Make Them COW */
  /*
   for (x = (PD_ENTRIES / 4); x < PD_ENTRIES; x++) {

   // If Page Is Avaiable Map It
   if ((parentPageTable[x] & 0xFFFFF000) != 0x0) {

   // Set Pages To COW
   newPageTable[x] = (((uint32_t) parentPageTable[x] & 0xFFFFF000) | (PAGE_DEFAULT | PAGE_COW));

   // Increment The COW Counter For This Page
   if (((uint32_t) parentPageTable[x] & PAGE_COW) == PAGE_COW) {
   adjustCowCounter(((uint32_t) parentPageTable[x] & 0xFFFFF000), 1);
   }
   else {
   adjustCowCounter(((uint32_t) parentPageTable[x] & 0xFFFFF000), 2);
   parentPageTable[x] = newPageTable[x];
   }
   }
   }
   */

  /*
   *
   * Map Page Directory Into VM Space
   * First Page After Page Tables
   * This must be mapped into the page directory before we map all 1024 page directories into the memory space
   */
  newPageTable = (uint32_t *) vmm_getFreePage(pid);

  newPageDirectory[PD_INDEX(PD_BASE_ADDR)] = (uint32_t) (vmm_getPhysicalAddr((uint32_t) newPageTable) | PAGE_DEFAULT);

  newPageTable[0] = (uint32_t) ((uint32_t) (newPageDirectoryAddress) | PAGE_DEFAULT);

  vmm_unmapPage((uint32_t) newPageTable, 1);

  /*
   *
   * Map Page Tables Into VM Space
   * The First Page Table (4MB) Maps To All Page Directories
   *
   */

  newPageTable = (uint32_t *) vmm_getFreePage(pid);

  newPageDirectory[PD_INDEX(PT_BASE_ADDR)] = (uint32_t) (vmm_getPhysicalAddr((uint32_t) newPageTable) | PAGE_DEFAULT);

  /* Flush The Page From Garbage In Memory */
  bzero(newPageTable, PAGE_SIZE);

  for (x = 0; x < PD_ENTRIES; x++)
    newPageTable[x] = newPageDirectory[x];

  /* Unmap Page From Virtual Space */
  vmm_unmapPage((uint32_t) newPageTable, 1);

  /* Now We Are Done With The Page Directory So Lets Unmap That Too */
  vmm_unmapPage((uint32_t) newPageDirectory, 1);

  spinUnlock(&cvsSpinLock);

  /* Return Physical Address Of Page Directory */
  return (newPageDirectoryAddress);
}
