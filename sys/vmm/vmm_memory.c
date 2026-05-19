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
#include <vmm/swap.h>
#include <sys/io.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/vitals.h>
#include <ubixos/spinlock.h>
#include <ubixos/sched.h>
#include <assert.h>

#include <machine/cpu.h>

static uint32_t freePages = 0;
static struct spinLock vmmSpinLock = SPIN_LOCK_INITIALIZER;

int numPages = 0x0;

/* Physical address where the page bitmap is staged (set in vmm_memMapInit,
 * read by paging.c to remap the bitmap into kernel virtual space). */
uint32_t vmm_bitmap_phys = 0;

mMap *vmmMemoryMap = NULL;

/* Linker symbols bracketing the kernel image. */
extern char _start[];
extern char _end[];

/************************************************************************

 Function: void vmm_memMapInit();
 Description: This Function Initializes The Memory Map For the System
 Notes:

 02/20/2004 - Made It Report Real And Available Memory

 ************************************************************************/
int vmm_memMapInit() {
  int i = 0x0;
  uint32_t kernel_start_page, bitmap_end_page;
  uint32_t bitmap_size;

  /* Count System Memory */
  numPages = countMemory();

  /*
   * Place the page bitmap immediately after the kernel image (page-aligned).
   * This makes the layout RAM-size-independent: with N pages the bitmap is
   * N*sizeof(mMap) bytes, and free pages begin right after it regardless of
   * how much RAM is installed.
   */
  vmm_bitmap_phys = ((uint32_t)_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
  vmmMemoryMap    = (mMap *)vmm_bitmap_phys;

  bitmap_size     = (uint32_t)numPages * sizeof(mMap);
  bitmap_end_page = (vmm_bitmap_phys + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

  /* Initialize every entry — bitmap lives in raw RAM, not in BSS, so we
   * must not assume it is zeroed. */
  for (i = 0x0; i < numPages; i++) {
    vmmMemoryMap[i].cowCounter = 0x0;
    vmmMemoryMap[i].status     = memNotavail;
    vmmMemoryMap[i].pid        = vmmID;
    vmmMemoryMap[i].pageAddr   = i * PAGE_SIZE;
  }

  /*
   * Free page ranges:
   *  [0x100000, kernel_start): RAM below the kernel (former bitmap staging area)
   *  [bitmap_end, numPages*PAGE_SIZE): all RAM above the bitmap
   *
   * The first 1 MB (0x0–0xFFFFF) stays reserved: ISA devices, VGA, BIOS.
   */
  kernel_start_page = ((uint32_t)_start & ~(PAGE_SIZE - 1)) / PAGE_SIZE;

  for (i = 0x100; i < (int)kernel_start_page; i++) {
    vmmMemoryMap[i].status = memAvail;
    freePages++;
  }

  for (i = (int)bitmap_end_page; i < numPages; i++) {
    vmmMemoryMap[i].status = memAvail;
    freePages++;
  }

  if (systemVitals)
    systemVitals->freePages = freePages;

  /* Print Out Amount Of Memory */
  kprintf("Real Memory:      %iKB\n", numPages * 4);
  kprintf("Available Memory: %iKB\n", freePages * 4);
  kprintf("vmm: bitmap phys=0x%X pages=%i end_page=%i\n",
      vmm_bitmap_phys, numPages, bitmap_end_page);

  /* Return */
  return (0);
}

/************************************************************************

 Function: int countMemory();
 Description: This Function Counts The Systems Physical Memory
 Notes:

 02/20/2004 - Inspect For Quality And Approved

 ************************************************************************/
int countMemory() {
  register uInt32 *mem = 0x0;
  unsigned long memCount = -1, tempMemory = 0x0;
  unsigned short memKb = 8;
  unsigned char irq1State, irq2State;
  unsigned long cr0 = 0x0;

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
  cr0 = rcr0();

  /*
   asm volatile (
   "movl %%cr0, %%ebx\n"
   : "=a" (cr0)
   :
   : "ebx"
   );
   */

  asm volatile ("wbinvd");

  load_cr0(cr0 | 0x00000001 | 0x40000000 | 0x20000000);

  /*
   asm volatile (
   "movl %%ebx, %%cr0\n"
   :
   : "a" (cr0 | 0x00000001 | 0x40000000 | 0x20000000)
   : "ebx"
   );
   */

  while (memKb < 4096 && memCount != 0) {
    memKb++;

    if (memCount == -1)
      memCount = 8388608;
    else
      memCount += 1024 * 1024;

    mem = (uInt32 *) memCount;

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

  asm("nop");

  //MrOlsen (2016-01-10) NOTE: I don't like this but I start incrementing form the start.
  memKb--;

  asm("nop");

  load_cr0(cr0);

  /*
   asm volatile (
   "movl %%ebx, %%cr0\n"
   :
   : "a" (cr0)
   : "ebx"
   );
   */

  asm("nop");

  /* Restore States For Both IRQ 1 And 2 */
  outportByte(0x21, irq1State);
  outportByte(0xA1, irq2State);

  asm("nop");

  /* Return Amount Of Memory In Pages */
  return ((memKb * 1024 * 1024) / PAGE_SIZE);
}

/************************************************************************

 Function: uInt32 vmm_findFreePage(pid_t pid);

 Description: This Returns A Free  Physical Page Address Then Marks It
 Not Available As Well As Setting The PID To The Proccess
 Allocating This Page
 Notes:

 ************************************************************************/
uint32_t vmm_findFreePage(pidType pid) {

  int i = 0x0;
  uint32_t evicted = 0x0;

  /* Lets Look For A Free Page */
  if (pid < sysID)
    kpanic("Error: invalid PID %i\n", pid);

retry:
  spinLock(&vmmSpinLock);

  for (i = 0; i < numPages; i++) {

    /*
     * If We Found A Free Page Set It To Not Available After That Set Its Own
     * And Return The Address
     */
    if ((vmmMemoryMap[i].status == memAvail) && (vmmMemoryMap[i].cowCounter == 0)) {
      vmmMemoryMap[i].status = memNotavail;
      vmmMemoryMap[i].pid = pid;
      freePages--;
      if (systemVitals)
        systemVitals->freePages = freePages;

      spinUnlock(&vmmSpinLock);
      return (vmmMemoryMap[i].pageAddr);
    }
  }

  spinUnlock(&vmmSpinLock);

  /* No free pages — attempt to evict a page to swap. */
  kprintf("vmm: OOM (pid %i) — attempting page eviction\n", pid);
  evicted = swap_evict_page();
  if (evicted != 0x0)
    goto retry;

  /* Eviction failed or no swap device. */
  kprintf("vmm: OOM — out of memory and swap (pid %i)\n", pid);

  if (pid == sysID)
    kpanic("Out Of Memory — kernel has no swap fallback");

  sched_setStatus(pid, DEAD);
  sched_yield();
  return (0x0);
}

/************************************************************************

 Function: int freePage(uInt32 pageAddr);

 Description: This Function Marks The Page As Free

 Notes:

 ************************************************************************/
int freePage(uint32_t pageAddr) {

  int pageIndex = 0x0;
  assert((pageAddr & 0xFFF) == 0x0);

  /* Find The Page Index To The Memory Map */
  pageIndex = (pageAddr / 4096);

  if (pageIndex < 0 || pageIndex >= numPages) {
    kprintf("freePage: addr 0x%X out of bounds (index %i numPages %i mmap 0x%X)\n",
        pageAddr, pageIndex, numPages, (uint32_t)vmmMemoryMap);
    return (-1);
  }

  /* Check If Page COW Is Greater Then 0 If It Is Dec It If Not Free It */
  spinLock(&vmmSpinLock);
  if (vmmMemoryMap[pageIndex].cowCounter == 0) {
    /* Set Page As Avail So It Can Be Used Again */
    vmmMemoryMap[pageIndex].status = memAvail;
    vmmMemoryMap[pageIndex].cowCounter = 0x0;
    vmmMemoryMap[pageIndex].pid = -2;
    freePages++;
    systemVitals->freePages = freePages;
    spinUnlock(&vmmSpinLock);
  }
  else {
    spinUnlock(&vmmSpinLock);
    /* Adjust The COW Counter */
    adjustCowCounter(((uint32_t) vmmMemoryMap[pageIndex].pageAddr), -1);
  }

  /* Return */
  return (0);
}

/************************************************************************

 Function: int adjustCowCounter(uInt32 baseAddr,int adjustment);

 Description: This Adjust The COW Counter For Page At baseAddr It Will
 Error If The Count Goes Below 0

 Notes:

 08/01/02 - I Think If Counter Gets To 0 I Should Free The Page

 ************************************************************************/
int adjustCowCounter(uInt32 baseAddr, int adjustment) {

  int vmmMemoryMapIndex = (baseAddr / PAGE_SIZE);

  assert((baseAddr & 0xFFF) == 0x0);

  if (vmmMemoryMapIndex < 0 || vmmMemoryMapIndex >= numPages) {
    kprintf("adjustCowCounter: addr 0x%X out of bounds (index %i, numPages %i)\n", baseAddr, vmmMemoryMapIndex, numPages);
    return (-1);
  }

  spinLock(&vmmSpinLock);
  /* Adjust COW Counter */
  vmmMemoryMap[vmmMemoryMapIndex].cowCounter += adjustment;

  if (vmmMemoryMap[vmmMemoryMapIndex].cowCounter <= 0) {

    if (vmmMemoryMap[vmmMemoryMapIndex].cowCounter < 0)
      kprintf("ERROR: Why is COW less than 0");

    vmmMemoryMap[vmmMemoryMapIndex].cowCounter = 0x0;
    vmmMemoryMap[vmmMemoryMapIndex].pid = vmmID;
    vmmMemoryMap[vmmMemoryMapIndex].status = memAvail;
    freePages++;
    systemVitals->freePages = freePages;
  }

  spinUnlock(&vmmSpinLock);
  /* Return */
  return (0);
}

/************************************************************************

 Function: void vmm_freeProcessPages(pid_t pid);

 Description: This Function Will Free Up Memory For The Exiting Process

 Notes:

 08/04/02 - Added Checking For COW Pages First

 ************************************************************************/

void vmm_freeProcessPages(pidType pid) {
  int i = 0;

  spinLock(&vmmSpinLock);

  /*
   * By the time we are called, endTask() has already run
   * vmm_cleanVirtualSpace(0x400000) while the dying task was still _current.
   * That walk decremented every COW reference in PD[1..767] and freed every
   * private user page.  All that remains here is to reclaim the physical pages
   * used for page tables and the page directory itself, which vmm_cleanVirtualSpace
   * zeroes out but does not free.
   *
   * Pages with cowCounter > 0 are owned by this pid but still shared with
   * live children — leave them alone and let each child's own cleanup path
   * free the page when the last reference drops.
   */
  for (i = 0; i < numPages; i++) {
    if (vmmMemoryMap[i].pid == pid && vmmMemoryMap[i].cowCounter == 0) {
      vmmMemoryMap[i].status = memAvail;
      vmmMemoryMap[i].pid = vmmID;
      freePages++;
      systemVitals->freePages = freePages;
    }
  }

  /* Return */
  spinUnlock(&vmmSpinLock);
  return;
}
