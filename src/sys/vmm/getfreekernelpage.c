/*****************************************************************************************
 Copyright (c) 2002, 2009 The UbixOS Project
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
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <lib/kprintf.h>

static spinLock_t vmmGFPlock = SPIN_LOCK_INITIALIZER;


/************************************************************************

Function: void *vmm_getFreeKernelPage(pidType pid);
Description: Returns A Free Page Mapped To The VM Space
Notes:

06/12/09 - Moved Function From paging.c To Replace This One
07/30/02 - This Returns A Free Page In The Top 1GB For The Kernel

************************************************************************/
void *vmm_getFreeKernelPage(pidType pid, u_int32_t count) {
  int        x = 0;
  int        y = 0;
  int        c = 0;
  u_int32_t *pageTableSrc = 0x0;

  spinLock(&vmmGFPlock);

  /* Lets Search For A Free Page */
  for (x = 768; x < 1024; x++) {
    /* Set Page Table Address */
    pageTableSrc = (u_int32_t *) (PAGE_TABLES_BASE_ADDR + (4096 * x));

    for (y = 0; y < 1024; y++) {
      /* Loop Through The Page Table Find An UnAllocated Page */
      if ((u_int32_t) pageTableSrc[y] == (u_int32_t) 0x0) {
        if (count > 1) {
          for (c = 0; c < count; c++) {
            if (y + c < 1024) {
              if ((u_int32_t) pageTableSrc[y + c] != (u_int32_t) 0x0) {
                c = -1;
                break;
                }
              }
            }

          if (c != -1) {
            for (c = 0; c < count; c++) {
              if ((vmm_remapPage((u_int32_t) vmm_findFreePage(pid), ((x * (1024 * 4096)) + ((y + c) * 4096)),KERNEL_PAGE_DEFAULT)) == 0x0)
                K_PANIC("vmmRemapPage failed: gfkp-1\n");
              if (vmm_zeroVirtualPage((u_int32_t) ((x * (1024 * 4096)) + ((y + c) * 4096))) == 0x1)
                K_PANIC("vmm_zeroVirtualPage: failed\n");
              }

            spinUnlock(&vmmGFPlock);
            return ((void *)((x * (1024 * 4096)) + (y * 4096)));
            }
          }
        else {
          /* Map A Physical Page To The Virtual Page */
          if ((vmm_remapPage((u_int32_t) vmm_findFreePage(pid), ((x * (1024 * 4096)) + (y * 4096)),KERNEL_PAGE_DEFAULT)) == 0x0)
            K_PANIC("vmmRemapPage failed: gfkp-2\n");

          /* Clear This Page So No Garbage Is There */
          if (vmm_zeroVirtualPage((u_int32_t) ((x * (1024 * 4096)) + (y * 4096))) == 0x1)
            K_PANIC("vmm_zeroVirtualPage: Failed\n");
          /* Return The Address Of The Newly Allocate Page */
          spinUnlock(&vmmGFPlock);
          return ((void *)((x * (1024 * 4096)) + (y * 4096)));
          }
        }
      }
    }

  /* If No Free Page Was Found Return NULL */
  spinUnlock(&vmmGFPlock);
  return (0x0);
  } /* end func */


/***
 $Log$
 Revision 1.3  2009/07/08 21:20:13  reddawg
 Getting There

 Revision 1.2  2009/07/08 16:05:56  reddawg
 Sync


 END
 ***/
