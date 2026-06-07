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

#include <vmm/vmm.h>
#include <vmm/swap.h>
#include <vmm/vm_map.h>
#include <vmm/vm_filecache.h>
#include <fs/vfs/file.h>
#include <fs/vfs/vfs.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <ubixos/endtask.h>
#include <ubixos/signal.h>
#include <lib/kprintf.h>
#include <i386/signal.h>
#include <sys/trap.h>
#include <string.h>

static struct spinLock g_page_fault_spin_lock = SPIN_LOCK_INITIALIZER;

/**
 * Print a clear, labelled segfault report to the kernel console (kprintf — VGA
 * + COM1 serial).  Shows the faulting task, the fault address (CR2), the
 * instruction and stack pointers, the trap error code, and the page-directory
 * and page-table entries governing the fault address — so e.g. pte=0 (unmapped)
 * or a wild eip (e.g. 0xAAAAAAAA, a corrupted code pointer) is obvious at a
 * glance.  Output goes to the kernel console only; user-facing "Segmentation
 * fault" reporting is the shell's job when it reaps the signalled child.
 */
static void vmm_report_segfault(const char *reason, struct trapframe *frame, u_int32_t cr2)
{
	u_int32_t *pd = (u_int32_t *)PD_BASE_ADDR;
	u_int32_t pdi = PD_INDEX(cr2);
	u_int32_t pti = PT_INDEX(cr2);
	u_int32_t pde = pd[pdi];
	u_int32_t pte = 0;

	if (pde & PAGE_PRESENT)
		pte = ((u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * pdi)))[pti];

	kprintf("\nSIGSEGV: pid=%i (%s): %s\n", _current->id, _current->name, reason);
	kprintf("  fault=0x%X  eip=0x%X  esp=0x%X  cs=0x%X  err=0x%X (%s)\n",
	        cr2,
	        (u_int32_t)frame->tf_eip,
	        (u_int32_t)frame->tf_esp,
	        (u_int32_t)frame->tf_cs,
	        (u_int32_t)frame->tf_err,
	        ((frame->tf_cs & 3) == 3) ? "user" : "kernel");
	kprintf("  pde[0x%X]=0x%X  pte[0x%X]=0x%X\n", pdi, pde, pti, pte);

	/* Dump the VMAs bracketing the fault — this tells VMM-bug from corruption:
	 *   "<== CONTAINS FAULT" on a VMA  => a real demand/lookup bug (fault is in
	 *                                      a registered region the handler should
	 *                                      have backed).
	 *   no containing VMA / large gap  => a wild/corrupted pointer (e.g. a
	 *                                      smashed return address) — not the VMM. */
	{
		struct rb_node *n;
		int shown = 0;
		int contained = 0;

		kprintf("  VMAs near fault:\n");
		for (n = rb_first(&_current->vm_map.vm_root); n != NULL && shown < 24; n = rb_next(n))
		{
			vm_map_entry_t *e = (vm_map_entry_t *)n;

			/* Only those within 16 MB either side of the fault. */
			if (e->vm_end + 0x1000000U < cr2 || cr2 + 0x1000000U < e->vm_start)
				continue;

			int here = (cr2 >= e->vm_start && cr2 < e->vm_end);
			contained |= here;
			kprintf("    [0x%X-0x%X) prot=0x%X flags=0x%X%s\n",
			        (u_int32_t)e->vm_start,
			        (u_int32_t)e->vm_end,
			        e->vm_prot,
			        e->vm_flags,
			        here ? "  <== CONTAINS FAULT" : "");
			shown++;
		}
		if (!contained)
			kprintf("    (no VMA contains the fault — wild/corrupted pointer, not a "
			        "lazy-mapping miss)\n");
	}

	/* Which mapping holds the faulting instruction?  For a library-backed VMA
	 * this names the backing file and the offset within the mapping, so eip can
	 * be symbolised offline (addr2line -e <file> <off>). */
	{
		vm_map_entry_t *ev = vm_map_lookup(&_current->vm_map, (uintptr_t)frame->tf_eip);
		if (ev != NULL)
		{
			const char *name = "(anon)";
			if ((ev->vm_flags & VM_MAP_FILE) && ev->vm_vnode != NULL)
				name = ((fileDescriptor_t *)ev->vm_vnode)->fileName;
			kprintf("  eip in [0x%X-0x%X) flags=0x%X off=0x%X file=%s\n",
			        (u_int32_t)ev->vm_start,
			        (u_int32_t)ev->vm_end,
			        ev->vm_flags,
			        (u_int32_t)((u_int32_t)frame->tf_eip - (u_int32_t)ev->vm_start),
			        name);
		}
		else
		{
			kprintf("  eip 0x%X is in no VMA (corrupted code pointer)\n", (u_int32_t)frame->tf_eip);
		}
	}
}

/**
 * Demand-fault one page of a file-backed (VM_MAP_FILE) VMA: read the page from
 * the VMA's private backing fd and map it.  Read-only pages go through the
 * shared file-page cache (one physical copy across processes, PAGE_SHARED);
 * writable pages get a private copy.  Called from the page-fault handler with
 * g_page_fault_spin_lock held, in the faulting process's address space.  Safe
 * to read here because the IDE driver polls (never sleeps).
 *
 * @return 1 if the page was mapped, 0 on failure (caller delivers SIGSEGV).
 */
static int vmm_demand_file_page(vm_map_entry_t *vma, u_int32_t mem_addr)
{
	fileDescriptor_t *bfd = (fileDescriptor_t *)vma->vm_vnode;
	u_int32_t pg = mem_addr & 0xFFFFF000;
	off_t foff = vma->vm_offset + (off_t)(pg - vma->vm_start);
	int ro = ((vma->vm_prot & VM_PROT_WRITE) == 0);
	u_int32_t phys, new_page, winner = 0;

	if (bfd == NULL)
		return (0);

	/* Read-only and already cached: map the one shared physical copy. */
	if (ro)
	{
		phys = vm_filecache_lookup_ref(bfd->mp, bfd->ino, foff);
		if (phys != 0)
			return (vmm_remap_page(phys, pg, PAGE_PRESENT | PAGE_USER | PAGE_SHARED, _current->id, 0) != 0);
	}

	/* Miss (or writable): allocate a page, map it writable, read the file in. */
	new_page = vmm_find_free_page(_current->id);
	if (new_page == 0)
		return (0);
	if (vmm_remap_page(new_page, pg, PAGE_DEFAULT, _current->id, 0) == 0)
		return (0);
	asm volatile("invlpg (%0)" : : "r"(pg) : "memory");
	memset((void *)pg, 0, PAGE_SIZE);
	if (bfd->mp != NULL && bfd->mp->fs != NULL && bfd->mp->fs->vfsRead != NULL)
		bfd->mp->fs->vfsRead(bfd, (void *)pg, foff, PAGE_SIZE);

	if (ro)
	{
		/* Publish into the cache and downgrade the live mapping to shared RO. */
		phys = vmm_get_physical_addr(pg);
		if (vm_filecache_insert(bfd->mp, bfd->ino, foff, phys, &winner) == 0)
		{
			vmm_set_page_attributes(pg, PAGE_PRESENT | PAGE_USER | PAGE_SHARED);
		}
		else if (winner != 0)
		{
			vmm_unmap_page(pg, VMM_FREE);
			if (vmm_remap_page(winner, pg, PAGE_PRESENT | PAGE_USER | PAGE_SHARED, _current->id, 0) == 0)
				return (0);
		}
	}
	else
	{
		/* Writable file page: the memset + vfsRead above dirtied the PTE (the
		 * CPU set D on those kernel writes through the user mapping).  Reset to
		 * a clean PAGE_DEFAULT so msync only writes back pages the *application*
		 * subsequently modifies, not freshly demand-read ones. */
		vmm_set_page_attributes(pg, PAGE_DEFAULT);
	}
	return (1);
}

/*****************************************************************************************

 Function:    void vmm_page_fault(u_int32_t mem_addr,u_int32_t eip,u_int32_t esp);
 Description: This is the page fault handler, it will handle COW and trap all other
 exceptions and segfault the thread.

 Notes:

 07/30/02 - Fixed COW However I Need To Think Of A Way To Impliment
 A Paging System Also Start To Add Security Levels

 07/27/04 - Added spin locking to ensure that we are thread safe. I know that spining a
 cpu is a waste of resources but for now it prevents errors.

 *****************************************************************************************/
/* void vmm_page_fault(u_int32_t mem_addr,u_int32_t eip,u_int32_t esp) { */
void vmm_page_fault(struct trapframe *frame, u_int32_t cr2)
{
	u_int32_t i = 0, page_table_index = 0, page_directory_index = 0;
	u_int32_t *page_dir = NULL, *page_table = NULL;
	u_int32_t *src = NULL, *dst = NULL;

	u_int32_t esp = frame->tf_esp;
	u_int32_t mem_addr = cr2;

	/* Try to aquire lock otherwise spin till we do */
	spinLock(&g_page_fault_spin_lock);

	/*
	 * VM86 (BIOS) tasks may fault on MMIO or BDA addresses not in the kernel
	 * page table.  Identity-map the page on demand rather than die_if_kernel.
	 */
	if (_current->oInfo.v86Task)
	{
		u_int32_t phys_page = mem_addr & 0xFFFFF000;
		if (vmm_remap_page(phys_page, phys_page, KERNEL_PAGE_DEFAULT, _current->id, 1) == 0)
		{
			kprintf("v86 pageFault: remap failed for 0x%X, killing task\n", mem_addr);
			spinUnlock(&g_page_fault_spin_lock);
			sched_dead(_current);
			sched_yield();
			return;
		}
		spinUnlock(&g_page_fault_spin_lock);
		return;
	}

	/* Set page dir pointer to the address of the visable page directory */
	page_dir = (u_int32_t *)PD_BASE_ADDR;

	/* NULL dereference: deliver SIGSEGV to user, kpanic in kernel. */
	if (mem_addr == 0)
	{
		vmm_report_segfault("NULL dereference", frame, mem_addr);
		if ((frame->tf_cs & 3) == 3)
		{
			spinUnlock(&g_page_fault_spin_lock);
			signal_post_fault(SIGSEGV, (void *)mem_addr, SEGV_MAPERR);
			signal_check(frame);
			return;
		}
		spinUnlock(&g_page_fault_spin_lock);
		kpanic("Error We Wrote To 0x0\n");
	}

	/* Calculate The Page Directory Index */
	page_directory_index = PD_INDEX(mem_addr);

	/* Calculate The Page Table Index     */
	page_table_index = PT_INDEX(mem_addr);

	/* UBU - This is a temporary routine for handling access to a page of a non existant page table */
	if (page_dir[page_directory_index] == 0) // NOLINT(clang-analyzer-core.FixedAddressDereference)
	{
		/* Stack growth across a page-directory boundary: allocate a new PT. */
		if ((frame->tf_cs & 3) == 3 && mem_addr < esp && mem_addr >= (esp - 0x800000) &&
		    mem_addr >= 0x10000000U)
		{
			u_int32_t new_page = vmm_find_free_page(_current->id);
			if (new_page != 0 &&
			    vmm_remap_page(new_page, mem_addr & 0xFFFFF000, PAGE_DEFAULT, _current->id, 0) != 0)
			{
				memset((void *)(mem_addr & 0xFFFFF000), 0, PAGE_SIZE);
				asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");
				spinUnlock(&g_page_fault_spin_lock);
				return;
			}
		}

		/* Demand-fault a file-backed VMA (PT not yet allocated — vmm_remap_page
		 * creates it).  Handles user- and kernel-mode faults alike. */
		{
			vm_map_entry_t *fvma = vm_map_lookup(&_current->vm_map, mem_addr);
			if (fvma != NULL && (fvma->vm_flags & VM_MAP_FILE))
			{
				if (vmm_demand_file_page(fvma, mem_addr))
				{
					asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");
					spinUnlock(&g_page_fault_spin_lock);
					return;
				}
				vmm_report_segfault("file demand-page failed (no PT)", frame, mem_addr);
				spinUnlock(&g_page_fault_spin_lock);
				if ((frame->tf_cs & 3) == 3)
				{
					signal_post_fault(SIGSEGV, (void *)mem_addr, SEGV_MAPERR);
					signal_check(frame);
					return;
				}
				endTask(_current->id);
				return;
			}
		}

		/* Lazy anonymous VMA: PT not yet allocated — vmm_remap_page will create it. */
		if ((frame->tf_cs & 3) == 3)
		{
			vm_map_entry_t *vma = vm_map_lookup(&_current->vm_map, mem_addr);
			if (vma != NULL && (vma->vm_flags & VM_MAP_ANON))
			{
				u_int32_t new_page = vmm_find_free_page(_current->id);
				if (new_page != 0 &&
				    vmm_remap_page(new_page, mem_addr & 0xFFFFF000, PAGE_DEFAULT, _current->id, 0) != 0)
				{
					memset((void *)(mem_addr & 0xFFFFF000), 0, PAGE_SIZE);
					asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");
					spinUnlock(&g_page_fault_spin_lock);
					return;
				}
				kprintf("pageFault: OOM (lazy anon PD) at 0x%X pid %i\n", mem_addr, _current->id);
				spinUnlock(&g_page_fault_spin_lock);
				endTask(_current->id);
				return;
			}
		}

		vmm_report_segfault("page table not present", frame, mem_addr);
		spinUnlock(&g_page_fault_spin_lock);
		if ((frame->tf_cs & 3) == 3)
		{
			signal_post_fault(SIGSEGV, (void *)mem_addr, SEGV_MAPERR);
			signal_check(frame);
			return;
		}
		endTask(_current->id);
		return;
	}

	/* Set page_table To Point To Virtual Address Of Page Table */
	page_table = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * page_directory_index));

	/* Test if this is a COW on page */
	if ((page_table[page_table_index] & PAGE_COW) == PAGE_COW)
	{
		/* Set Src To Base Address Of Page To Copy */
		src = (u_int32_t *)(mem_addr & 0xFFFFF000);
		/* Allocate A Free Page For Destination */
		dst = (u_int32_t *)vmm_get_free_virtual_page(_current->id, 1, 0x1);
		if (dst == NULL)
		{
			kprintf("vmm_page_fault: out of virtual pages during COW at 0x%X\n", mem_addr);
			spinUnlock(&g_page_fault_spin_lock);
			endTask(_current->id);
			return;
		}
		/* Copy Memory */
		for (i = 0; i < PD_ENTRIES; i++)
		{
			dst[i] = src[i];
		}
		/* Adjust The COW Counter For Physical Page */
		adjust_cow_counter((page_table[page_table_index] & 0xFFFFF000), -1);
		/* Remap In New Page — use PAGE_DEFAULT, not the fault address offset */
		page_table[page_table_index] = vmm_get_physical_addr((u_int32_t)dst) | PAGE_DEFAULT;
		/* Unlink From Memory Map Allocated Page */
		vmm_unmap_page((u_int32_t)dst, VMM_KEEP);
	}
	else if ((page_table[page_table_index] & PAGE_SWAPPED) == PAGE_SWAPPED)
	{
		/* Page was evicted to swap — bring it back in. */
		u_int32_t slot = PTE_SWAP_SLOT(page_table[page_table_index]);
		u_int32_t new_page = vmm_find_free_page(_current->id);

		if (new_page == 0)
		{
			kprintf("pageFault: OOM during swap-in at 0x%X pid %i\n", mem_addr, _current->id);
			spinUnlock(&g_page_fault_spin_lock);
			endTask(_current->id);
			return;
		}

		page_table[page_table_index] = new_page | PAGE_DEFAULT;
		asm volatile("invlpg (%0)" : : "r"(mem_addr & 0xFFFFF000) : "memory");

		if (swap_read_page(slot, (void *)(mem_addr & 0xFFFFF000)) != 0)
		{
			kprintf("pageFault: swap read error slot %u at 0x%X\n", slot, mem_addr);
			page_table[page_table_index] = 0;
			asm volatile("invlpg (%0)" : : "r"(mem_addr & 0xFFFFF000) : "memory");
			free_page(new_page);
			spinUnlock(&g_page_fault_spin_lock);
			endTask(_current->id);
			return;
		}

		swap_free_slot(slot);
	}
	else if (page_table[page_table_index] != 0)
	{
		vmm_report_segfault("page present but not user-accessible", frame, mem_addr);
		spinUnlock(&g_page_fault_spin_lock);
		if ((frame->tf_cs & 3) == 3)
		{
			signal_post_fault(SIGSEGV, (void *)mem_addr, SEGV_ACCERR);
			signal_check(frame);
			return;
		}
		die_if_kernel("SEGFAULT", frame, 0xC);
		endTask(_current->id);
		return;
	}
	else if (mem_addr < (_current->td.vm_dsize + _current->td.vm_daddr))
	{
		u_int32_t new_page = vmm_find_free_page(_current->id);
		if (new_page == 0)
		{
			kprintf("pageFault: OOM at 0x%X pid %i\n", mem_addr, _current->id);
			spinUnlock(&g_page_fault_spin_lock);
			endTask(_current->id);
			return;
		}
		page_table[page_table_index] = new_page | PAGE_DEFAULT;
		/* Flush TLB for this page so the zero-write goes to the new physical page */
		asm volatile("invlpg (%0)" : : "r"(mem_addr & 0xFFFFF000) : "memory");
		memset((void *)(mem_addr & 0xFFFFF000), 0, PAGE_SIZE);
	}
	else if ((frame->tf_cs & 3) == 3 && mem_addr < esp && mem_addr >= (esp - 0x800000) && mem_addr >= 0x10000000U)
	{
		/* Stack growth: fault just below ESP — map a new zeroed page. */
		u_int32_t new_page = vmm_find_free_page(_current->id);
		if (new_page == 0)
		{
			kprintf("pageFault: OOM (stack) at 0x%X pid %i\n", mem_addr, _current->id);
			spinUnlock(&g_page_fault_spin_lock);
			signal_post_fault(SIGSEGV, (void *)mem_addr, SEGV_MAPERR);
			signal_check(frame);
			return;
		}
		page_table[page_table_index] = new_page | PAGE_DEFAULT;
		asm volatile("invlpg (%0)" : : "r"(mem_addr & 0xFFFFF000) : "memory");
		memset((void *)(mem_addr & 0xFFFFF000), 0, PAGE_SIZE);
	}
	else
	{
		/* Check if fault address is in a registered anonymous VMA (demand-zero).
		 * Handles both user-mode faults and kernel-mode copy-from/to-user faults:
		 * a kernel syscall may access a lazy anon page before the user has
		 * touched it, causing a ring-0 fault that also needs demand-zero. */
		{
			vm_map_entry_t *vma = vm_map_lookup(&_current->vm_map, mem_addr);

			/* File-backed VMA: demand-read the page from its backing fd. */
			if (vma != NULL && (vma->vm_flags & VM_MAP_FILE) && vmm_demand_file_page(vma, mem_addr))
			{
				asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");
				spinUnlock(&g_page_fault_spin_lock);
				return;
			}

			if (vma != NULL && (vma->vm_flags & VM_MAP_ANON))
			{
				u_int32_t new_page = vmm_find_free_page(_current->id);
				if (new_page == 0)
				{
					kprintf("pageFault: OOM (anon vma) at 0x%X pid %i\n", mem_addr, _current->id);
					spinUnlock(&g_page_fault_spin_lock);
					if ((frame->tf_cs & 3) == 3)
					{
						signal_post_fault(SIGSEGV, (void *)mem_addr, SEGV_MAPERR);
						signal_check(frame);
						return;
					}
					endTask(_current->id);
					return;
				}
				page_table[page_table_index] = new_page | PAGE_DEFAULT;
				asm volatile("invlpg (%0)" : : "r"(mem_addr & 0xFFFFF000) : "memory");
				memset((void *)(mem_addr & 0xFFFFF000), 0, PAGE_SIZE);
				asm volatile("movl %cr3,%eax\n movl %eax,%cr3\n");
				spinUnlock(&g_page_fault_spin_lock);
				return;
			}
		}

		/* Access to non-mapped memory: SIGSEGV user, kpanic kernel. */
		vmm_report_segfault("address not mapped", frame, mem_addr);
		spinUnlock(&g_page_fault_spin_lock);
		if ((frame->tf_cs & 3) == 3)
		{
			signal_post_fault(SIGSEGV, (void *)mem_addr, SEGV_MAPERR);
			signal_check(frame);
			return;
		}
		die_if_kernel("SEGFAULT", frame, 0xC);
		endTask(_current->id);
		return;
	}

	asm volatile("movl %cr3,%eax\n"
	             "movl %eax,%cr3\n");

	/* Release the spin lock */
	spinUnlock(&g_page_fault_spin_lock);
	return;
}
