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
#include <sys/io.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/vitals.h>
#include <ubixos/spinlock.h>
#include <ubixos/sched.h>
#include <assert.h>

#include <machine/cpu.h>

static u_int32_t g_free_pages = 0;
static struct spinLock g_vmm_spin_lock = SPIN_LOCK_INITIALIZER;

u_int32_t numPages = 0;

/* Physical address where the page bitmap is staged (set in vmm_mem_map_init,
 * read by paging.c to remap the bitmap into kernel virtual space). */
u_int32_t vmm_bitmap_phys = 0;

vmm_page_info_t *vmmMemoryMap = NULL;

/* Linker symbols bracketing the kernel image. */
extern char _start[]; // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
extern char _end[];   // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)

/************************************************************************

 Function: void vmm_mem_map_init();
 Description: This Function Initializes The Memory Map For the System
 Notes:

 02/20/2004 - Made It Report Real And Available Memory

 ************************************************************************/
int vmm_mem_map_init()
{
	u_int32_t i = 0;
	u_int32_t kernel_start_page, bitmap_end_page;
	u_int32_t bitmap_size;

	/* Count System Memory */
	numPages = count_memory();

	/*
	 * Place the page bitmap immediately after the kernel image (page-aligned).
	 * This makes the layout RAM-size-independent: with N pages the bitmap is
	 * N*sizeof(vmm_page_info_t) bytes, and free pages begin right after it
	 * regardless of how much RAM is installed.
	 */
	vmm_bitmap_phys = ((u_int32_t)_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	vmmMemoryMap = (vmm_page_info_t *)vmm_bitmap_phys;

	bitmap_size = numPages * sizeof(vmm_page_info_t);
	bitmap_end_page = (vmm_bitmap_phys + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

	/* Initialize every entry — bitmap lives in raw RAM, not in BSS, so we
	 * must not assume it is zeroed. */
	for (i = 0; i < numPages; i++)
	{
		vmmMemoryMap[i].cowCounter = 0;
		vmmMemoryMap[i].status = memNotavail;
		vmmMemoryMap[i].pid = vmmID;
		vmmMemoryMap[i].pageAddr = i * PAGE_SIZE;
	}

	/*
	 * Free page ranges:
	 *  [0x100000, kernel_start): RAM below the kernel (former bitmap staging area)
	 *  [bitmap_end, numPages*PAGE_SIZE): all RAM above the bitmap
	 *
	 * The first 1 MB (0x0–0xFFFFF) stays reserved: ISA devices, VGA, BIOS.
	 */
	kernel_start_page = ((u_int32_t)_start & ~(PAGE_SIZE - 1)) / PAGE_SIZE;

	for (i = 0x100; i < kernel_start_page; i++)
	{
		vmmMemoryMap[i].status = memAvail;
		g_free_pages++;
	}

	for (i = bitmap_end_page; i < numPages; i++)
	{
		vmmMemoryMap[i].status = memAvail;
		g_free_pages++;
	}

	if (systemVitals)
	{
		systemVitals->freePages = g_free_pages;
	}

	/* Print Out Amount Of Memory */
	kprintf("Real Memory:      %uKB\n", numPages * 4);
	kprintf("Available Memory: %uKB\n", g_free_pages * 4);
	kprintf("vmm: bitmap phys=0x%X pages=%u end_page=%u\n", vmm_bitmap_phys, numPages, bitmap_end_page);

	/* Return */
	return (0);
}

/************************************************************************

 Function: int count_memory();
 Description: This Function Counts The Systems Physical Memory
 Notes:

 02/20/2004 - Inspect For Quality And Approved

 ************************************************************************/
u_int32_t count_memory()
{
	register u_int32_t *mem = NULL;
	unsigned long mem_count = -1, temp_memory = 0;
	unsigned short mem_kb = 8;
	unsigned char irq1_state, irq2_state;
	unsigned long cr0 = 0;

	/*
	 * Save The States Of Both IRQ 1 And 2 So We Can Turn Them Off And Restore
	 * Them Later
	 */
	irq1_state = inportByte(0x21);
	irq2_state = inportByte(0xA1);

	/* Turn Off IRQ 1 And 2 To Prevent Chances Of Faults While Examining Memory */
	outportByte(0x21, 0xFF);
	outportByte(0xA1, 0xFF);

	/* Save The State Of Register CR0 */
	cr0 = rcr0();

	/*
	 asm volatile (
	 "movl %%cr0, %%ebx\n"
	 : "=a" (cr0)
	 :
	 : "ebx"
	 );
	 */

	asm volatile("wbinvd");

	load_cr0(cr0 | 0x00000001 | 0x40000000 | 0x20000000);

	/*
	 asm volatile (
	 "movl %%ebx, %%cr0\n"
	 :
	 : "a" (cr0 | 0x00000001 | 0x40000000 | 0x20000000)
	 : "ebx"
	 );
	 */

	while (mem_kb < 4096 && mem_count != 0)
	{
		mem_kb++;

		if (mem_count == -1)
		{
			mem_count = 8388608;
		}
		else
		{
			mem_count += 1024 * 1024;
		}

		mem = (u_int32_t *)mem_count;

		temp_memory = *mem; // NOLINT(clang-analyzer-core.FixedAddressDereference)

		*mem = 0x55AA55AA;

		asm("" : : : "memory");

		if (*mem != 0x55AA55AA)
		{
			mem_count = 0;
		}
		else
		{
			*mem = 0xAA55AA55;
			asm("" : : : "memory");
			if (*mem != 0xAA55AA55)
			{
				mem_count = 0;
			}
		}
		asm("" : : : "memory");
		*mem = temp_memory;
	}

	asm("nop");

	// MrOlsen (2016-01-10) NOTE: I don't like this but I start incrementing form the start.
	mem_kb--;

	asm("nop");

	load_cr0(cr0);

	/*
	 asm volatile (
	 "movl %%ebx, %%cr0\n"
	 :
	 : "a" (cr0)
	 : "ebx"
	 );
	 */

	asm("nop");

	/* Restore States For Both IRQ 1 And 2 */
	outportByte(0x21, irq1_state);
	outportByte(0xA1, irq2_state);

	asm("nop");

	/* Return Amount Of Memory In Pages */
	return ((mem_kb * 1024 * 1024) / PAGE_SIZE);
}

/************************************************************************

 Function: u_int32_t vmm_find_free_page(pid_t pid);

 Description: This Returns A Free  Physical Page Address Then Marks It
 Not Available As Well As Setting The PID To The Proccess
 Allocating This Page
 Notes:

 ************************************************************************/
u_int32_t vmm_find_free_page(pidType pid)
{

	u_int32_t i = 0;
	u_int32_t evicted = 0;

	/* Lets Look For A Free Page */
	if (pid < sysID)
	{
		kpanic("Error: invalid PID %i\n", pid);
	}

retry:
	spinLock(&g_vmm_spin_lock);

	for (i = 0; i < numPages; i++)
	{

		/*
		 * If We Found A Free Page Set It To Not Available After That Set Its Own
		 * And Return The Address
		 */
		if ((vmmMemoryMap[i].status == memAvail) && (vmmMemoryMap[i].cowCounter == 0))
		{
			vmmMemoryMap[i].status = memNotavail;
			vmmMemoryMap[i].pid = pid;
			g_free_pages--;
			if (systemVitals)
			{
				systemVitals->freePages = g_free_pages;
			}

			spinUnlock(&g_vmm_spin_lock);
			return (vmmMemoryMap[i].pageAddr);
		}
	}

	spinUnlock(&g_vmm_spin_lock);

	/* No free pages — attempt to evict a page to swap. */
	kprintf("vmm: OOM (pid %i) — attempting page eviction\n", pid);
	evicted = swap_evict_page();
	if (evicted != 0)
	{
		goto retry;
	}

	/* Eviction failed or no swap device. */
	kprintf("vmm: OOM — out of memory and swap (pid %i)\n", pid);

	if (pid == sysID)
	{
		kpanic("Out Of Memory — kernel has no swap fallback");
	}

	sched_setStatus(pid, DEAD);
	sched_yield();
	return 0;
}

/************************************************************************

 Function: int free_page(u_int32_t pageAddr);

 Description: This Function Marks The Page As Free

 Notes:

 ************************************************************************/
int free_page(u_int32_t page_addr)
{

	u_int32_t page_index = 0;
	assert((page_addr & 0xFFF) == 0);

	/* Find The Page Index To The Memory Map */
	page_index = page_addr / 4096;

	if (page_index >= numPages)
	{
		kprintf("free_page: addr 0x%X out of bounds (index %u numPages %u mmap 0x%X)\n",
		        page_addr,
		        page_index,
		        numPages,
		        (u_int32_t)vmmMemoryMap);
		return (-1);
	}

	/* Check If Page COW Is Greater Then 0 If It Is Dec It If Not Free It */
	spinLock(&g_vmm_spin_lock);
	if (vmmMemoryMap[page_index].cowCounter == 0)
	{
		/* Set Page As Avail So It Can Be Used Again */
		vmmMemoryMap[page_index].status = memAvail;
		vmmMemoryMap[page_index].cowCounter = 0;
		vmmMemoryMap[page_index].pid = -2;
		g_free_pages++;
		systemVitals->freePages = g_free_pages;
		spinUnlock(&g_vmm_spin_lock);
	}
	else
	{
		spinUnlock(&g_vmm_spin_lock);
		/* Adjust The COW Counter */
		adjust_cow_counter(vmmMemoryMap[page_index].pageAddr, -1);
	}

	/* Return */
	return (0);
}

/************************************************************************

 Function: int adjust_cow_counter(u_int32_t baseAddr,int adjustment);

 Description: This Adjust The COW Counter For Page At baseAddr It Will
 Error If The Count Goes Below 0

 Notes:

 08/01/02 - I Think If Counter Gets To 0 I Should Free The Page

 ************************************************************************/
int adjust_cow_counter(u_int32_t base_addr, int adjustment)
{

	u_int32_t vmm_memory_map_index = base_addr / PAGE_SIZE;

	assert((base_addr & 0xFFF) == 0);

	if (vmm_memory_map_index >= numPages)
	{
		kprintf("adjust_cow_counter: addr 0x%X out of bounds (index %u, numPages %u)\n",
		        base_addr,
		        vmm_memory_map_index,
		        numPages);
		return (-1);
	}

	spinLock(&g_vmm_spin_lock);
	/* Adjust COW Counter */
	vmmMemoryMap[vmm_memory_map_index].cowCounter += adjustment;

	if (vmmMemoryMap[vmm_memory_map_index].cowCounter <= 0)
	{

		if (vmmMemoryMap[vmm_memory_map_index].cowCounter < 0)
		{
			kprintf("ERROR: Why is COW less than 0");
		}

		vmmMemoryMap[vmm_memory_map_index].cowCounter = 0;
		vmmMemoryMap[vmm_memory_map_index].pid = vmmID;
		vmmMemoryMap[vmm_memory_map_index].status = memAvail;
		g_free_pages++;
		systemVitals->freePages = g_free_pages;
	}

	spinUnlock(&g_vmm_spin_lock);
	/* Return */
	return (0);
}

/************************************************************************

 Function: void vmm_share_ref(u_int32_t phys);

 Description: Account for one new mapping of a shared (non-COW, non-file-cache)
 frame in the COW counter, which holds the total number of mappers.  A fresh
 frame has counter 0 meaning "one owner"; the first additional mapper must bring
 it to 2 (two mappers => two free_page calls before release), mirroring COW's
 first-time +2.  Used by vmm_share_region and by fork when a share_region page
 is shared into a child.

 ************************************************************************/
void vmm_share_ref(u_int32_t phys)
{
	u_int32_t idx = phys / PAGE_SIZE;

	if (idx >= numPages)
		return;

	spinLock(&g_vmm_spin_lock);
	if (vmmMemoryMap[idx].cowCounter == 0)
		vmmMemoryMap[idx].cowCounter = 2;
	else
		vmmMemoryMap[idx].cowCounter += 1;
	spinUnlock(&g_vmm_spin_lock);
}

/************************************************************************

 Function: void vmm_free_process_pages(pid_t pid);

 Description: This Function Will Free Up Memory For The Exiting Process

 Notes:

 08/04/02 - Added Checking For COW Pages First

 ************************************************************************/

void vmm_free_process_pages(pidType pid)
{
	u_int32_t i = 0;

	spinLock(&g_vmm_spin_lock);

	/*
	 * By the time we are called, endTask() has already run
	 * vmm_clean_virtual_space(0x400000) while the dying task was still _current.
	 * That walk decremented every COW reference in PD[1..767] and freed every
	 * private user page.  All that remains here is to reclaim the physical pages
	 * used for page tables and the page directory itself, which vmm_clean_virtual_space
	 * zeroes out but does not free.
	 *
	 * Pages with cowCounter > 0 are owned by this pid but still shared with
	 * live children — leave them alone and let each child's own cleanup path
	 * free the page when the last reference drops.
	 */
	for (i = 0; i < numPages; i++)
	{
		if (vmmMemoryMap[i].pid == pid && vmmMemoryMap[i].cowCounter == 0)
		{
			vmmMemoryMap[i].status = memAvail;
			vmmMemoryMap[i].pid = vmmID;
			g_free_pages++;
			systemVitals->freePages = g_free_pages;
		}
	}

	/* Return */
	spinUnlock(&g_vmm_spin_lock);
	return;
}
