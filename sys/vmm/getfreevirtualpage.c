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
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>

static struct spinLock fvpSpinLock = SPIN_LOCK_INITIALIZER;

/************************************************************************

 Function: void *vmm_getFreeVirtualPage(pidType pid,int count);
 Description: Returns A Free Page Mapped To The VM Space
 Notes:

 2016-01-21 MrOlsen - I'm not 100% happy with this, i know I can make the calculations much faster

 08/11/02 - This Will Return Next Avilable Free Page Of Tasks VM Space

 ************************************************************************/
void *vmm_getFreeVirtualPage(pidType pid, int count, int type) {
  int y = 0, counter = 0, pdI = 0x0, ptI = 0x0;
  uint32_t *pageTableSrc = 0x0;
  uint32_t *pageDir = 0x0;
  uint32_t start_page = 0x0;

  spinLock(&fvpSpinLock);

  pageDir = (uInt32 *) PD_BASE_ADDR;

  /* Lets Search For A Free Page */
  if (_current->oInfo.vmStart <= 0x100000)
    kpanic("Invalid vmStart\n");

 //MrOlsen kprintf("type: %i ", type);

  /* Get Our Starting Address */
  if (type == VM_THRD) {
    start_page = (uint32_t) (_current->td.vm_daddr + ctob(_current->td.vm_dsize));
  }
  else if (type == VM_TASK) {
    //kprintf("vmStart");
    start_page = _current->oInfo.vmStart;
  }
  else
    K_PANIC("Invalid Type");

  //kprintf( "Entering MMAP: Type: %i, Returning %i Pages At Address: 0x%X\n", type, count, start_page );

  /*
   *
   * I Need To Write Some Function For Space That Is Returned Maybe A Malloc Type Map
   *
   */

  /*
   * Lets Start Allocating Pages
   */
  for ( counter = 0; counter < count; counter++ ) {
    /* Locate Initial Page Table */
    pdI = ((start_page + (counter * 0x1000)) / 0x400000);

    keepMapping:
      //kprintf("PAGE IS");
    /* If Page Directory Is Not Yet Allocated Allocate It */
    if ((pageDir[pdI] & PAGE_PRESENT) != PAGE_PRESENT) {
      //kprintf("PAGE NOT %i,", __LINE__);
      pageDir[pdI] = (uInt32) vmm_findFreePage(_current->id) | PAGE_DEFAULT;
      //kprintf("PAGE NOT %i,", __LINE__);

      //kprintf("PAGE NOT %i,", __LINE__);

    /* Also Add It To Virtual Space So We Can Make Changes Later */
    pageTableSrc = (uint32_t *) (PT_BASE_ADDR + (PD_INDEX( PT_BASE_ADDR ) * 0x1000)); /* Table that maps that 4b */
    pageTableSrc[pdI] = (pageDir[pdI] & 0xFFFFF000) | PAGE_DEFAULT; /* Is This Why Page Needs To Be User As Well? */
    pageTableSrc = (uint32_t *) (PT_BASE_ADDR + (pdI * 0x1000));



      //kprintf("PAGE NOT %i,", __LINE__);

      /* Reload Page Directory */
      asm(
        "movl %cr3,%eax\n"
        "movl %eax,%cr3\n"
      );

      //kprintf("PAGE NOT %i,", __LINE__);
      pageTableSrc = (uInt32 *) (PT_BASE_ADDR + (0x1000 * pdI));

      //kprintf("PAGE NOT %i,", __LINE__);
      /* Initialize The New Page Table To Prevent Dirty Bits */
      for (y = 0x0; y < PD_ENTRIES; y++) {
        pageTableSrc[y] = (uInt32) 0x0;
      }
      //kprintf("PAGE NOT %i,", __LINE__);

    }
    else {
      pageTableSrc = (uInt32 *) (PT_BASE_ADDR + (0x1000 * pdI));
    }
//kprintf("HERE?");

    ptI = ((start_page - (pdI * 0x400000)) / 0x1000);

    for (y = ptI; y < 1024 && counter < count; y++) {

      /* Loop Through The Page Table Find An UnAllocated Page */
      if ((pageTableSrc[y] & PAGE_COW) == PAGE_COW) {
        kprintf("COW PAGE NOT CLEANED!");
      }
      else if ((uInt32) pageTableSrc[y] == (uInt32) 0x0) {
        if ((vmm_remapPage((uInt32) vmm_findFreePage(pid), ((pdI * (1024 * 4096)) + (y * 4096)), PAGE_DEFAULT, pid)) == 0x0)
          kpanic("vmmRemapPage: getFreeVirtualPage-1: (%i)[0x%X]\n", type, ((pdI * (1024 * 4096)) + (y * 4096)));
        vmm_clearVirtualPage((uInt32) ((pdI * (1024 * 4096)) + (y * 4096)));
      }
      else {
        kprintf("-> y: %i, ptI: 0x%X, pdI: 0x%X pTS: 0x%X ??\n", y, ptI, pdI, pageTableSrc[y]);
        K_PANIC("UHM HOW DO WE HAVE AN ALLOCATED PAGE HERE!!\n");
      }

      //kprintf("[0x%X:%i:%i:%i]", ((pdI * (1024 * 4096)) + (y * 4096)), y, counter, count);
      counter++;

    }
    if (counter < count) {
      //kprintf("Need More Pages!");
      start_page += (0x1000 * counter);
      pdI = ((start_page + (counter * 0x1000)) / 0x400000);
      goto keepMapping;
    }
  }

  if (type == VM_THRD) {
    _current->td.vm_dsize += btoc(count * 0x1000);
    //kprintf( "vm_dsize: [0x%X]][0x%X]\n", ctob( _current->td.vm_dsize ), _current->td.vm_dsize );
  }
  else if (type == VM_TASK)
    _current->oInfo.vmStart += (count * 0x1000);

  /*
   * MMAP Return
   */

  //kprintf( "mmap: [0x%x]\n", start_page );
  /* If No Free Page Was Found Return NULL */
  spinUnlock(&fvpSpinLock);
  return (start_page);
}

/***
 END
 ***/

