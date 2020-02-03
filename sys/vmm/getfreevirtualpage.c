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

  uint32_t *pageDirectory = 0x0;
  uint32_t *pageTable = 0x0;

  uint32_t start_page = 0x0;
  uint32_t map_from = 0x0;

    kprintf("gFVP");

  spinLock(&fvpSpinLock);

  pageDirectory = (uint32_t *) PD_BASE_ADDR;

  /* Lets Search For A Free Page */
  if (_current->oInfo.vmStart <= 0x100000)
    kpanic("Invalid vmStart\n");

  /* Get Our Starting Address */
  if (type == VM_THRD) {
    start_page = (uint32_t) (_current->td.vm_daddr + ctob(_current->td.vm_dsize));
  }
  else if (type == VM_TASK) {
    start_page = _current->oInfo.vmStart;
  }
  else
    K_PANIC("Invalid Type");


  /* Locate Initial Page Table */
  keepMapping:
  pdI = PD_INDEX(start_page);

  if (pdI > PD_INDEX(VMM_USER_END)) {
    map_from = 0x0;
    goto doneMapping;
  }

  /* If Page Directory Is Not Yet Allocated Allocate It */
  if ((pageDirectory[pdI] & PAGE_PRESENT) != PAGE_PRESENT) {
    vmm_allocPageTable(pdI, pid);
  }

  pageTable = (uint32_t *) (PT_BASE_ADDR + (pdI * PAGE_SIZE));

  ptI = PT_INDEX(start_page);

  for (y = ptI; y < PT_ENTRIES && counter < count; y++, counter++) {

    /* Loop Through The Page Table Find An UnAllocated Page */
    if ((pageTable[y] & PAGE_PRESENT) == PAGE_PRESENT) {
      if ((pageTable[y] & PAGE_COW) == PAGE_COW)
        kprintf("COW PAGE NOT CLEANED!");

      start_page += (PAGE_SIZE * counter);
      map_from = 0x0;
      counter = 0;
      goto keepMapping;
    }

    if (map_from == 0x0)
      map_from = start_page;
  }

  if (counter < count) {
    start_page += (PAGE_SIZE * counter);
    goto keepMapping;
  }

  gotPages:
  if (type == VM_THRD)
    _current->td.vm_dsize += btoc(count * PAGE_SIZE);
  else if (type == VM_TASK)
    _current->oInfo.vmStart = map_from + (count * PAGE_SIZE);

 //_current->oInfo.vmStart += (count * PAGE_SIZE);

  for (counter = 0; counter < count; counter++) {
    if ((vmm_remapPage((uint32_t) vmm_findFreePage(pid), (map_from + (counter * PAGE_SIZE)), PAGE_DEFAULT, pid, 0)) == 0x0)
      kpanic("vmmRemapPage: getFreeVirtualPage-1: (%i)[0x%X]\n", type, map_from + (counter * PAGE_SIZE));

    bzero((map_from + (counter * PAGE_SIZE)), PAGE_SIZE);
  }

  doneMapping:
  spinUnlock(&fvpSpinLock);
  return (map_from);
}
