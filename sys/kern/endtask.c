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
#include <sys/gdt.h>
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
	 * Force GS = SEL_PCPU (the per-CPU GDT selector) before freeing the address
	 * space.  A user task can reach here with GS still holding the LDT-based TLS
	 * selector (0xF, LDT entry 1 at VMM_USER_LDT 0x7FF000, PD[1]); since
	 * vmm_clean_virtual_space below frees that page, any later GS validation
	 * would fault reading the freed LDT.  SEL_PCPU is a GDT descriptor with no
	 * LDT dependency, so it is safe across the free — and, critically, it keeps
	 * _current (read as %gs:8) valid for the rest of this function.  (Clearing
	 * GS to 0, as the hardware-task-switch era did, now poisons every _current
	 * access in the remainder of endTask.)
	 */
	asm volatile("movw %0, %%gs" : : "r"((u_int16_t)SEL_PCPU) : "memory");

	/*
	 * Release the full per-process address space while we are still _current
	 * so PT_BASE_ADDR reflects our own page tables.  Start at PD[1]
	 * (0x400000) rather than VMM_USER_START (0x800000): PD[1] holds COW'd
	 * pages from fork and must have its reference counts decremented here.
	 * PD[0] (0–4 MB identity map) is shared by all processes and must not
	 * be touched.  systemTask's vmm_free_process_pages only needs to reclaim
	 * the private PT/PD physical pages after this runs.
	 */
	vmm_clean_virtual_space(0x400000U);

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
