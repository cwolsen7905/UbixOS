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

 $Id: getfreevirtualpage.c 230 2016-01-24 01:36:51Z reddawg $

 *****************************************************************************************/

#include <vmm/vmm.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>

static spinLock_t fvpSpinLock = SPIN_LOCK_INITIALIZER;

/************************************************************************

 Function: void *vmmGetFreeVirtualPage(pidType pid,int count);
 Description: Returns A Free Page Mapped To The VM Space
 Notes:

 2016-01-21 MrOlsen - I'm not 100% happy with this, i know I can make the calculations much faster

 08/11/02 - This Will Return Next Avilable Free Page Of Tasks VM Space

 ************************************************************************/
void *vmmGetFreeVirtualPage( pidType pid, int count, int type ) {
  int y = 0, counter = 0, pdI = 0x0, ptI = 0x0;
  uint32_t *pageTableSrc = 0x0;
  uint32_t *pageDir = 0x0;
  uint32_t start_page = 0x0;

  spinLock( &fvpSpinLock );

  pageDir = (uInt32 *) PD_BASE_ADDR;

  /* Lets Search For A Free Page */
  if ( _current->oInfo.vmStart <= 0x100000 )
    kpanic( "Invalid vmStart\n" );

  /* Get Our Starting Address */
  if ( type == VM_THRD ) {
    start_page = (uint32_t)( _current->td.vm_daddr + ctob( _current->td.vm_dsize ) );
  }
  else if ( type == VM_TASK ) {
    //kprintf("vmStart");
    start_page = _current->oInfo.vmStart;
  }
  else
    K_PANIC( "Invalid Type" );

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

    /* If Page Directory Is Not Yet Allocated Allocate It */
    if ( (pageDir[pdI] & PAGE_PRESENT) != PAGE_PRESENT ) {
      pageDir[pdI] = (uInt32) vmmFindFreePage( _current->id ) | PAGE_DEFAULT;

      /* Also Add It To Virtual Space So We Can Make Changes Later */
      pageTableSrc = (uInt32 *) (PT_BASE_ADDR + (4096 * 767));
      pageTableSrc[pdI] = pageDir[pdI];

      /* Reload Page Directory */
      asm(
          "movl %cr3,%eax\n"
          "movl %eax,%cr3\n"
      );

      pageTableSrc = (uInt32 *) (PT_BASE_ADDR + (0x1000 * pdI));

      /* Initialize The New Page Table To Prevent Dirty Bits */
      for ( y = 0x0; y < PD_ENTRIES; y++ ) {
        pageTableSrc[y] = (uInt32) 0x0;
      }

    }
    else {
      pageTableSrc = (uInt32 *) (PT_BASE_ADDR + (0x1000 * pdI));
    }

    ptI = ((start_page - (pdI * 0x400000)) / 0x1000);

    for ( y = ptI; y < 1024 && counter < count; y++ ) {

      /* Loop Through The Page Table Find An UnAllocated Page */
      if ( (pageTableSrc[y] & PAGE_COW) == PAGE_COW ) {
        kprintf( "COW PAGE NOT CLEANED!" );
      }
      else if ( (uInt32) pageTableSrc[y] == (uInt32) 0x0 ) {
        if ( (vmm_remapPage( (uInt32) vmmFindFreePage( pid ), ((pdI * (1024 * 4096)) + (y * 4096)), PAGE_DEFAULT )) == 0x0 )
          kpanic( "vmmRemapPage: getFreeVirtualPage-1: (%i)[0x%X]\n", type, ((pdI * (1024 * 4096)) + (y * 4096)) );
        vmmClearVirtualPage( (uInt32)( (pdI * (1024 * 4096)) + (y * 4096) ) );
      }
      else {
        kprintf( "-> y: %i, ptI: 0x%X, pdI: 0x%X pTS: 0x%X ??\n", y, ptI, pdI, pageTableSrc[y] );
        K_PANIC( "UHM HOW DO WE HAVE AN ALLOCATED PAGE HERE!!\n" );
      }
      counter++;

    }
  }

  if ( type == VM_THRD ) {
    _current->td.vm_dsize += btoc( count * 0x1000 );
    //kprintf( "vm_dsize: [0x%X]][0x%X]\n", ctob( _current->td.vm_dsize ), _current->td.vm_dsize );
  }
  else if ( type == VM_TASK )
    _current->oInfo.vmStart += count * 0x1000;

  /*
   * MMAP Return
   */

  //kprintf( "mmap: [0x%x]\n", start_page );
  /* If No Free Page Was Found Return NULL */
  spinUnlock( &fvpSpinLock );
  return (start_page);
}

/***
 END
 ***/

