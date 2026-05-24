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
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>

static struct spinLock g_vmm_gfp_lock = SPIN_LOCK_INITIALIZER;

/************************************************************************

 Function: void *vmm_getFreePage(pidType pid);

 Description: Returns A Free Page Mapped To The VM Space

 Notes:

 07/30/02 - This Returns A Free Page In The Top 1GB For The Kernel

 ************************************************************************/
void *vmm_getFreePage(pidType pid)
{
	u_int16_t x = 0x0, y = 0x0;
	u_int32_t *page_table_src = 0x0;

	spinLock(&g_vmm_gfp_lock);

	/* Lets Search For A Free Page */
	for (x = PD_INDEX(VMM_KERN_START); x <= PD_INDEX(VMM_KERN_END); x++)
	{

		/* Set Page Table Address */
		page_table_src = (u_int32_t *)(PT_BASE_ADDR + (0x1000 * x));

		for (y = 0x0; y < 1024; y++)
		{

			/* Loop Through The Page Table Find An UnAllocated Page */
			if ((u_int32_t)page_table_src[y] == (u_int32_t)0x0)
			{

				/* Map A Physical Page To The Virtual Page */
				if ((vmm_remapPage(vmm_findFreePage(pid),
				                   ((x * 0x400000) + (y * 0x1000)),
				                   KERNEL_PAGE_DEFAULT,
				                   pid,
				                   0)) == 0x0) {
					kpanic("vmmRemapPage: vmm_getFreePage\n");
				}

				/* Clear This Page So No Garbage Is There */
				vmm_clearVirtualPage((u_int32_t)((x * 0x400000) + (y * 0x1000)));

				/* Return The Address Of The Newly Allocate Page */
				spinUnlock(&g_vmm_gfp_lock);
				return ((void *)((x * 0x400000) + (y * 0x1000)));
			}
		}
	}

	/* If No Free Page Was Found Return NULL */
	spinUnlock(&g_vmm_gfp_lock);

	return (0x0);
}
