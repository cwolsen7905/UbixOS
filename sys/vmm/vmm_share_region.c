/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>

/*
 * vmm_share_region — map pages from the calling process into dst_pid's space.
 *
 * vaddr must be page-aligned and already mapped in the current (src) process.
 * size is rounded up to a page boundary.  The same physical pages are mapped
 * read-write into dst_pid's user space at a freshly allocated virtual address,
 * which is returned.  Returns 0 on failure.
 *
 * Strategy: collect physical addresses from the current CR3, then temporarily
 * switch CR3 to dst's page directory (with interrupts disabled) and call
 * vmm_remapPage for each page.  Kernel space is shared across all address
 * spaces so kernel code remains reachable during the switch.
 */
uintptr_t
vmm_share_region(uintptr_t vaddr, size_t size, pidType dst_pid)
{
	uint32_t  n, i, old_cr3, dst_vaddr;
	uint32_t *phys;
	kTask_t  *dst;

	if (vaddr == 0 || size == 0)
		return 0;

	n = (uint32_t)((size + PAGE_SIZE - 1) / PAGE_SIZE);
	if (n > 4096) {
		kprintf("vmm_share_region: region too large (%u pages)\n", n);
		return 0;
	}

	phys = kmalloc(n * sizeof(uint32_t));
	if (!phys)
		return 0;

	/* Collect physical addresses from current (src) address space */
	for (i = 0; i < n; i++) {
		phys[i] = vmm_getPhysicalAddr(vaddr + i * PAGE_SIZE);
		if (phys[i] == 0) {
			kprintf("vmm_share_region: page %u at 0x%X not mapped\n",
			    i, vaddr + i * PAGE_SIZE);
			kfree(phys);
			return 0;
		}
	}

	dst = schedFindTask(dst_pid);
	if (dst == 0) {
		kprintf("vmm_share_region: dst pid %d not found\n", dst_pid);
		kfree(phys);
		return 0;
	}

	/*
	 * Reserve a virtual range in dst's address space by bumping its
	 * vmStart pointer.  We read vmStart before the CR3 switch because
	 * _current still points to the src task.
	 */
	dst_vaddr = dst->oInfo.vmStart;
	dst->oInfo.vmStart += n * PAGE_SIZE;

	/*
	 * Sync kernel PD entries into dst's page directory before switching
	 * CR3.  Kernel PD entries (VMM_KERN_START..VMM_KERN_END) may differ
	 * between tasks when new kernel page tables are allocated after a
	 * task's PD was created.  Without the sync, kernel code and data are
	 * unreachable after the CR3 switch, causing a page fault with IF=0
	 * ("INT OFF! KERN").  Map dst's physical PD at VMM_CHILD_PD_WINDOW,
	 * copy the stale-prone entries, then unmap before switching CR3.
	 */
	{
		uint32_t *src_pd  = (uint32_t *)PD_BASE_ADDR;
		uint32_t *dst_pd;
		uint32_t  kstart  = PD_INDEX(VMM_KERN_START);
		uint32_t  kend    = PD_INDEX(VMM_KERN_END);
		uint32_t  x;

		if (vmm_remapPage(dst->md.md_tss.cr3, VMM_CHILD_PD_WINDOW,
		    KERNEL_PAGE_DEFAULT, _current->id, 0) == 0) {
			kprintf("vmm_share_region: failed to map dst PD\n");
			dst->oInfo.vmStart -= n * PAGE_SIZE;
			kfree(phys);
			return 0;
		}
		dst_pd = (uint32_t *)VMM_CHILD_PD_WINDOW;
		for (x = kstart; x <= kend; x++)
			dst_pd[x] = src_pd[x];
		vmm_unmapPage(VMM_CHILD_PD_WINDOW, 1);
	}

	/*
	 * Switch to dst's page directory and map the physical pages.
	 * Interrupts disabled to prevent the scheduler from running with
	 * the wrong CR3 loaded.  Kernel entries are now current so all
	 * kernel code and globals are reachable under dst's CR3.
	 */
	asm volatile("cli");
	asm volatile("movl %%cr3, %0" : "=r"(old_cr3));
	asm volatile("movl %0, %%cr3" :: "r"((uint32_t)dst->md.md_tss.cr3));

	for (i = 0; i < n; i++) {
		if (vmm_remapPage(phys[i], dst_vaddr + i * PAGE_SIZE,
		    PAGE_DEFAULT | PAGE_SHARED, dst_pid, 0) == 0) {
			kprintf("vmm_share_region: vmm_remapPage failed at page %u\n", i);
			/* Unmap pages already installed before the failure */
			for (uint32_t j = 0; j < i; j++)
				vmm_unmapPage(dst_vaddr + j * PAGE_SIZE, 1);
			asm volatile("movl %0, %%cr3" :: "r"(old_cr3));
			asm volatile("sti");
			dst->oInfo.vmStart -= n * PAGE_SIZE;
			kfree(phys);
			return 0;
		}
	}

	asm volatile("movl %0, %%cr3" :: "r"(old_cr3));
	asm volatile("sti");

	kprintf("vmm_share_region: src vaddr 0x%X -> pid %d vaddr 0x%X (%u pages) phys[0]=0x%X\n",
	    vaddr, dst_pid, dst_vaddr, n, phys[0]);

	kfree(phys);
	return (uintptr_t)dst_vaddr;
}
