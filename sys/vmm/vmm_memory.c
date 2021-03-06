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
#include <sys/io.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/vitals.h>
#include <ubixos/spinlock.h>
#include <assert.h>

//MrOlsen (2016-01-11) NOTE: Need to Seperate Out CPU Specific Stuff Over Time
#include <i386/cpu.h>

static uint32_t freePages = 0;
static struct spinLock vmmSpinLock = SPIN_LOCK_INITIALIZER;
//static struct spinLock vmmCowSpinLock = SPIN_LOCK_INITIALIZER;

int numPages = 0x0;

mMap *vmmMemoryMap = (mMap *) VMM_MMAP_ADDR_RMODE;

/************************************************************************

 Function: void vmm_memMapInit();
 Description: This Function Initializes The Memory Map For the System
 Notes:

 02/20/2004 - Made It Report Real And Available Memory

 ************************************************************************/
int vmm_memMapInit() {
  int i = 0x0;
  int memStart = 0x0;

  /* Count System Memory */
  numPages = countMemory();

  /* Set Memory Map To Point To First Physical Page That We Will Use */
  vmmMemoryMap = (mMap *) VMM_MMAP_ADDR_RMODE;

  /* Initialize Map Make All Pages Not Available */
  for (i = 0x0; i < numPages; i++) {
    vmmMemoryMap[i].cowCounter = 0x0;
    vmmMemoryMap[i].status = memNotavail;
    vmmMemoryMap[i].pid = vmmID;
    vmmMemoryMap[i].pageAddr = i * PAGE_SIZE;
  }

  /* Calculate Start Of Free Memory */
  memStart = (0x101000 / 0x1000);

  memStart += (((sizeof(mMap) * numPages) + (sizeof(mMap) - 1)) / 0x1000);

  /* Initialize All Free Pages To Available */
  vmmMemoryMap[(0x100000 / 0x1000)].status = memAvail;

  freePages++;

  for (i = memStart; i < numPages; i++) {
    vmmMemoryMap[i].status = memAvail;
    freePages++;
  }

  if (systemVitals)
    systemVitals->freePages = freePages;

  /* Print Out Amount Of Memory */
  kprintf("Real Memory:      %iKB\n", numPages * 4);
  kprintf("Available Memory: %iKB\n", freePages * 4);

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

  /* Lets Look For A Free Page */
  if (pid < sysID)
    kpanic("Error: invalid PID %i\n", pid);

  spinLock(&vmmSpinLock);

  for (i = 0; i <= numPages; i++) {

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

  /* If No Free Memory Is Found Return NULL */
  kpanic("Out Of Memory!!!!");
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

  /* Check If Page COW Is Greater Then 0 If It Is Dec It If Not Free It */
  if (vmmMemoryMap[pageIndex].cowCounter == 0) {

    /* Set Page As Avail So It Can Be Used Again */
    spinLock(&vmmSpinLock);
    vmmMemoryMap[pageIndex].status = memAvail;
    vmmMemoryMap[pageIndex].cowCounter = 0x0;
    vmmMemoryMap[pageIndex].pid = -2;
    freePages++;
    systemVitals->freePages = freePages;
    spinUnlock(&vmmSpinLock);

  }
  else {
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

/* TODO: This can be greatly improved for performance but it gets the job done */
void vmm_freeProcessPages(pidType pid) {
  int i = 0, x = 0;
  uint32_t *tmpPageTable = 0x0;
  uint32_t *tmpPageDir = (uInt32 *) PD_BASE_ADDR;

  spinLock(&vmmSpinLock);

  /* Check Page Directory For An Avail Page Table */
  //NOTE: This cleans all memory space up to kernel space
#ifdef _IGNORE
  for (i = 0; i < (PAGE_SIZE - (PAGE_SIZE / 4)); i++) {

    if (tmpPageDir[i] != 0) {

      /* Set Up Page Table Pointer */
      tmpPageTable = (uint32_t *) (PT_BASE_ADDR + (i * PAGE_SIZE));

      /* Check The Page Table For COW Pages */
      for (x = 0; x < PD_ENTRIES; x++) {

        /* If The Page Is COW Adjust COW Counter */
        if (((uint32_t) tmpPageTable[x] & PAGE_COW) == PAGE_COW) {
          adjustCowCounter(((uint32_t) tmpPageTable[x] & 0xFFFFF000), -1);
        }
      }
    }
  }
#endif

  /* Loop Through Pages To Find Pages Owned By Process */
  for (i = 0; i < numPages; i++) {
    if (vmmMemoryMap[i].pid == pid) {
      /* Check To See If The cowCounter Is Zero If So We Can Ree It */
      if (vmmMemoryMap[i].cowCounter == 0) {
        vmmMemoryMap[i].status = memAvail;
        vmmMemoryMap[i].cowCounter = 0x0;
        vmmMemoryMap[i].pid = vmmID;
        freePages++;
        systemVitals->freePages = freePages;
      }
      else {
        spinUnlock(&vmmSpinLock);
        adjustCowCounter((i * PAGE_SIZE), -1);
        spinLock(&vmmSpinLock);
      }
    }
  }

  /* Return */
  spinUnlock(&vmmSpinLock);
  return;
}
