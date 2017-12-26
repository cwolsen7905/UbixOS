/*****************************************************************************************
 Copyright (c) 2002 The UbixOS Project
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

 $Id: getfreepage.c 188 2016-01-23 03:09:10Z reddawg $

 *****************************************************************************************/

#include <vmm/vmm.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>

static spinLock_t vmmGFPlock = SPIN_LOCK_INITIALIZER;

/************************************************************************

 Function: void *vmmGetFreePage(pidType pid);

 Description: Returns A Free Page Mapped To The VM Space

 Notes:

 07/30/02 - This Returns A Free Page In The Top 1GB For The Kernel

 ************************************************************************/
void *vmm_getFreePage( pidType pid ) {
  uInt16 x = 0x0, y = 0x0;
  uInt32 *pageTableSrc = 0x0;

  spinLock( &vmmGFPlock );

  /* Lets Search For A Free Page */
  for ( x = 768; x < 1024; x++ ) {

    /* Set Page Table Address */
    pageTableSrc = (uInt32 *) (PT_BASE_ADDR + (0x1000 * x));
    for ( y = 0x0; y < 1024; y++ ) {
      /* Loop Through The Page Table Find An UnAllocated Page */
      if ( (uInt32) pageTableSrc[y] == (uInt32) 0x0 ) {
        /* Map A Physical Page To The Virtual Page */
        if ( (vmm_remapPage( vmmFindFreePage( pid ), ((x * 0x400000) + (y * 0x1000)), KERNEL_PAGE_DEFAULT )) == 0x0 )
          kpanic( "vmmRemapPage: vmmGetFreePage\n" );
        /* Clear This Page So No Garbage Is There */
        vmmClearVirtualPage( (uInt32)( (x * 0x400000) + (y * 0x1000) ) );
        /* Return The Address Of The Newly Allocate Page */
        spinUnlock( &vmmGFPlock );
        return ((void *) ((x * 0x400000) + (y * 0x1000)));
      }
    }
  }

  /* If No Free Page Was Found Return NULL */
  spinUnlock( &vmmGFPlock );
  return (0x0);
}

/***
 END
 ***/

