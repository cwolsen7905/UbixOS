/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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

#include <vmm/pageout.h>
#include <vmm/swap.h>
#include <vmm/vmm.h>
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <ubixos/vitals.h>
#include <lib/kprintf.h>

/*
 * pageout_daemon — kernel thread that proactively reclaims physical pages.
 *
 * Wakes every PAGEOUT_INTERVAL_TICKS scheduler ticks.  If free memory falls
 * below PAGEOUT_LOW_WATERMARK it iterates the task list and evicts one page
 * per task (by temporarily loading the task's CR3 so that PD_BASE_ADDR /
 * PT_BASE_ADDR reflect its page tables) until the free count reaches
 * PAGEOUT_HIGH_WATERMARK or there are no more candidates.
 *
 * CR3 switching is safe because the kernel virtual range (top 1 GB) is
 * identical across all page directories — kernel code, stack, and global
 * data remain accessible after the switch.
 *
 * Runs at QOS_BACKGROUND so it yields to all interactive and normal work.
 */
void
pageout_daemon(void)
{
	uint32_t  kern_cr3;
	uint32_t  last_tick = 0;
	kTask_t  *t;

	/* Save the kernel page directory CR3 for restoration after each task walk. */
	asm volatile("movl %%cr3, %0" : "=r"(kern_cr3));

	/* Background priority — never compete with real work. */
	_current->base_priority = QOS_BACKGROUND;
	_current->priority      = QOS_BACKGROUND;

	kprintf("pageout: daemon started (low=%u%% high=%u%% interval=%u ticks)\n",
	    100 / 10, 15, PAGEOUT_INTERVAL_TICKS);

	for (;;) {
		/* Sleep until the next polling interval. */
		while (systemVitals->sysTicks - last_tick < PAGEOUT_INTERVAL_TICKS)
			sched_yield();
		last_tick = systemVitals->sysTicks;

		if (!swap_enabled())
			continue;

		if (systemVitals->freePages >= (uint32_t)PAGEOUT_LOW_WATERMARK(numPages))
			continue;

		kprintf("pageout: low memory (%u free pages, target %u), reclaiming\n",
		    systemVitals->freePages,
		    (uint32_t)PAGEOUT_HIGH_WATERMARK(numPages));

		/*
		 * Walk the task list.  For each user process, temporarily switch
		 * to its address space and evict one page.  Restore the kernel
		 * CR3 after each eviction so taskList traversal stays valid.
		 */
		for (t = taskList; t != NULL; t = t->next) {
			if (systemVitals->freePages >=
			    (uint32_t)PAGEOUT_HIGH_WATERMARK(numPages))
				break;

			/* Skip kernel threads — they share kernelPageDirectory. */
			if (t->md.md_tss.cr3 == kern_cr3)
				continue;

			/* Skip tasks with no page directory yet. */
			if (t->md.md_tss.cr3 == 0)
				continue;

			asm volatile("movl %0, %%cr3" : : "r"(t->md.md_tss.cr3) : "memory");
			swap_evict_page();
			asm volatile("movl %0, %%cr3" : : "r"(kern_cr3) : "memory");
		}
	}
}
