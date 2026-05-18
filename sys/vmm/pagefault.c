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
#include <vmm/swap.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <ubixos/spinlock.h>
#include <ubixos/endtask.h>
#include <lib/kprintf.h>
#include <sys/trap.h>
#include <string.h>

static struct spinLock pageFaultSpinLock = SPIN_LOCK_INITIALIZER;

/*****************************************************************************************

 Function:    void vmm_pageFault(uInt32 memAddr,uInt32 eip,uInt32 esp);
 Description: This is the page fault handler, it will handle COW and trap all other
 exceptions and segfault the thread.

 Notes:

 07/30/02 - Fixed COW However I Need To Think Of A Way To Impliment
 A Paging System Also Start To Add Security Levels

 07/27/04 - Added spin locking to ensure that we are thread safe. I know that spining a
 cpu is a waste of resources but for now it prevents errors.

 *****************************************************************************************/
/* void vmm_pageFault(uInt32 memAddr,uInt32 eip,uInt32 esp) { */
void vmm_pageFault(struct trapframe *frame, uint32_t cr2)
{
	uInt32 i = 0x0, pageTableIndex = 0x0, pageDirectoryIndex = 0x0;
	uInt32 *pageDir = 0x0, *pageTable = 0x0;
	uInt32 *src = 0x0, *dst = 0x0;

	uint32_t esp = frame->tf_esp;
	uint32_t eip = frame->tf_eip;
	uint32_t memAddr = cr2;

	/* Try to aquire lock otherwise spin till we do */
	spinLock(&pageFaultSpinLock);

	/*
	 * VM86 (BIOS) tasks may fault on MMIO or BDA addresses not in the kernel
	 * page table.  Identity-map the page on demand rather than die_if_kernel.
	 */
	if (_current->oInfo.v86Task)
	{
		uint32_t physPage = memAddr & 0xFFFFF000;
		if (vmm_remapPage(physPage, physPage, KERNEL_PAGE_DEFAULT, _current->id, 1) == 0)
		{
			kprintf("v86 pageFault: remap failed for 0x%X, killing task\n", memAddr);
			spinUnlock(&pageFaultSpinLock);
			_current->state = DEAD;
			sched_yield();
			return;
		}
		spinUnlock(&pageFaultSpinLock);
		return;
	}

	/* Set page dir pointer to the address of the visable page directory */
	pageDir = (uint32_t *)PD_BASE_ADDR;

	/* UBU - This is a temp panic for 0x0 read write later on I will handle this differently */
	if (memAddr == 0x0)
	{
		kprintf("Segfault At Address: [0x%X], ESP: [0x%X], PID: [%i], EIP: [0x%X]\n", memAddr, esp, _current->id, eip);
		spinUnlock(&pageFaultSpinLock);
		kpanic("Error We Wrote To 0x0\n");
	}

	/* Calculate The Page Directory Index */
	pageDirectoryIndex = PD_INDEX(memAddr);

	/* Calculate The Page Table Index     */
	pageTableIndex = PT_INDEX(memAddr);

	/* UBU - This is a temporary routine for handling access to a page of a non existant page table */
	if (pageDir[pageDirectoryIndex] == 0x0)
	{
		kprintf("Segfault At Address: [0x%X][0x%X][%i][0x%X], Not A Valid Page Table\n", memAddr, esp, _current->id, eip);
		spinUnlock(&pageFaultSpinLock);
		endTask(_current->id);
		return;
	}

	/* Set pageTable To Point To Virtual Address Of Page Table */
	pageTable = (uint32_t *)(PT_BASE_ADDR + (PAGE_SIZE * pageDirectoryIndex));

	/* Test if this is a COW on page */
	if (((uint32_t)pageTable[pageTableIndex] & PAGE_COW) == PAGE_COW)
	{
		/* Set Src To Base Address Of Page To Copy */
		src = (uInt32 *)(memAddr & 0xFFFFF000);
		/* Allocate A Free Page For Destination */
		dst = (uInt32 *)vmm_getFreeVirtualPage(_current->id, 1, 0x1);
		if (dst == 0x0)
		{
			kprintf("vmm_pageFault: out of virtual pages during COW at 0x%X\n", memAddr);
			spinUnlock(&pageFaultSpinLock);
			endTask(_current->id);
			return;
		}
		/* Copy Memory */
		for (i = 0; i < PD_ENTRIES; i++)
		{
			dst[i] = src[i];
		}
		/* Adjust The COW Counter For Physical Page */
		adjustCowCounter(((uInt32)pageTable[pageTableIndex] & 0xFFFFF000), -1);
		/* Remap In New Page — use PAGE_DEFAULT, not the fault address offset */
		pageTable[pageTableIndex] = (uInt32)(vmm_getPhysicalAddr((uInt32)dst) | PAGE_DEFAULT);
		/* Unlink From Memory Map Allocated Page */
		vmm_unmapPage((uInt32)dst, 1);
	}
	else if ((pageTable[pageTableIndex] & PAGE_SWAPPED) == PAGE_SWAPPED)
	{
		/* Page was evicted to swap — bring it back in. */
		uint32_t slot    = PTE_SWAP_SLOT(pageTable[pageTableIndex]);
		uint32_t newPage = vmm_findFreePage(_current->id);

		if (newPage == 0x0) {
			kprintf("pageFault: OOM during swap-in at 0x%X pid %i\n",
			    memAddr, _current->id);
			spinUnlock(&pageFaultSpinLock);
			endTask(_current->id);
			return;
		}

		pageTable[pageTableIndex] = newPage | PAGE_DEFAULT;
		asm volatile("invlpg (%0)" : : "r"(memAddr & 0xFFFFF000) : "memory");

		if (swap_read_page(slot, (void *)(memAddr & 0xFFFFF000)) != 0) {
			kprintf("pageFault: swap read error slot %u at 0x%X\n",
			    slot, memAddr);
			pageTable[pageTableIndex] = 0;
			asm volatile("invlpg (%0)" : : "r"(memAddr & 0xFFFFF000) : "memory");
			freePage(newPage);
			spinUnlock(&pageFaultSpinLock);
			endTask(_current->id);
			return;
		}

		swap_free_slot(slot);
	}
	else if (pageTable[pageTableIndex] != 0x0)
	{
		kprintf("Security failed pagetable not user permission\n");
		kprintf("pageDir: [0x%X]\n", pageDir[pageDirectoryIndex]);
		kprintf("pageTable: [0x%X:0x%X:0x%X:0x%X]\n", pageTable[pageTableIndex], pageTableIndex, pageDirectoryIndex, eip);
		kprintf("Segfault At Address: [0x%X][0x%X][%i][0x%X] Non Mapped.\n", memAddr, esp, _current->id, eip);
		spinUnlock(&pageFaultSpinLock);
		kpanic("SIT HERE FOR NOW");
		die_if_kernel("SEGFAULT", frame, 0xC);
		endTask(_current->id);
		return;
	}
	else if (memAddr < (_current->td.vm_dsize + _current->td.vm_daddr))
	{
		uInt32 newPage = vmm_findFreePage(_current->id);
		if (newPage == 0x0)
		{
			kprintf("pageFault: OOM at 0x%X pid %i\n", memAddr, _current->id);
			spinUnlock(&pageFaultSpinLock);
			endTask(_current->id);
			return;
		}
		pageTable[pageTableIndex] = newPage | PAGE_DEFAULT;
		/* Flush TLB for this page so the zero-write goes to the new physical page */
		asm volatile("invlpg (%0)" : : "r"(memAddr & 0xFFFFF000) : "memory");
		memset((void *)(memAddr & 0xFFFFF000), 0, PAGE_SIZE);
	}
	else
	{
		/* Need To Create A Routine For Attempting To Access Non Mapped Memory */
		kprintf("pageDir: [0x%X]\n", pageDir[pageDirectoryIndex]);
		kprintf("pageTable: [0x%X:0x%X:0x%X:0x%X]\n", pageTable[pageTableIndex], pageTableIndex, pageDirectoryIndex, eip);
		kprintf("Segfault At Address: [0x%X][0x%X][%i][0x%X] Non Mapped!\n", memAddr, esp, _current->id, eip);
		die_if_kernel("SEGFAULT", frame, 0xC);
		spinUnlock(&pageFaultSpinLock);
		kpanic("SIT HERE FOR NOW");
		kprintf("Out Of Stack Space: [0x%X]\n", memAddr & 0xFF0000);
		endTask(_current->id);
		return;
	}

	asm volatile("movl %cr3,%eax\n"
	             "movl %eax,%cr3\n");

	/* Release the spin lock */
	spinUnlock(&pageFaultSpinLock);
	return;
}
