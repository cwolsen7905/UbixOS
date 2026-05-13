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

/*
 * TODO (BUG-COW-03 proper): The full FreeBSD/Linux approach is for the dying
 * task to release its entire address space here (user + kernel low region),
 * leaving systemTask to only free the task struct and kernel stack.
 * vmm_cleanVirtualSpace already handles the user region; a matching cleaner
 * for the kernel-mapped low region (PD index 1, where COW is also used) and
 * the per-process kernel stack is still needed before systemTask's
 * vmm_freeProcessPages can be safely removed.
 */

/************************************************************************

 Function: endTask(pidType pid)

 Description: This will do cleanup for an ending task

 Notes:

 ************************************************************************/
void endTask(pidType pid)
{

	/* Release user address space while we are still _current so that
	 * PT_BASE_ADDR reflects our own page tables.  This decrements COW
	 * counters for shared pages and frees private pages before the
	 * scheduler switches us away. */
	vmm_cleanVirtualSpace((uint32_t)VMM_USER_START);

	/* Return TTY ownership to parent so the shell gets its prompt back. */
	if (_current->term != NULL && _current->term->owner == _current->id &&
	    _current->parent != NULL)
		_current->term->owner = _current->parent->id;

	sched_setStatus(pid, DEAD);
	sched_yield();
	while (1)
	{
		asm("hlt");
	}
}
