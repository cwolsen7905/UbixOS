/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions, the following disclaimer and the list of authors. 2)
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions, the following disclaimer and the list of authors in
 * the documentation and/or other materials provided with the distribution. 3)
 * Neither the name of the UbixOS Project nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>
#include <ubixos/vitals.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kprintf.h>
#include <isa/8259.h>


/************************************************************************

 Function: endTask(pidType pid)

 Description: This will do cleanup for an ending task

 Notes:

 ************************************************************************/
void endTask(pidType pid)
{

	/*
	 * GS = 0xF is an LDT-based selector (LDT entry 1) and the LDT lives at
	 * VMM_USER_LDT (0x7FF000, PD[1]).  vmm_cleanVirtualSpace frees that page
	 * below, so we must clear GS now while the LDT is still mapped.  If the
	 * scheduler later switches away and back while interrupts are disabled
	 * (or races via a DEAD-child wakeup), the hardware task switch will try
	 * to validate GS from GDT[3].base+8 = 0x7FF008 and fault on a
	 * not-present page.  GS = 0 has no descriptor and requires no LDT read.
	 */
	asm volatile("xorl %%eax, %%eax\n\t"
	             "movw %%ax, %%gs" : : : "eax", "memory");

	/*
	 * Release the full per-process address space while we are still _current
	 * so PT_BASE_ADDR reflects our own page tables.  Start at PD[1]
	 * (0x400000) rather than VMM_USER_START (0x800000): PD[1] holds COW'd
	 * pages from fork and must have its reference counts decremented here.
	 * PD[0] (0–4 MB identity map) is shared by all processes and must not
	 * be touched.  systemTask's vmm_freeProcessPages only needs to reclaim
	 * the private PT/PD physical pages after this runs.
	 */
	vmm_cleanVirtualSpace(0x400000U);

	/* Return TTY ownership to parent so the shell gets its prompt back. */
	if (_current->term != NULL && _current->term->owner == _current->id && _current->parent != NULL)
		_current->term->owner = _current->parent->id;

	sched_zombie(_current);
	sched_yield();
	while (1)
	{
		asm("hlt");
	}
}
