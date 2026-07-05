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
/* Rotating cursor: vmm_find_free_page resumes the bitmap scan here instead of
 * restarting from 0 every call.  Allocations are mostly sequential, so this turns
 * an O(numPages)-per-page scan into amortized O(1) — without it a large image
 * (a ~100MB clang = ~25000 pages) takes O(n*numPages) to load and appears to hang.
 * Freed pages below the cursor are still found via the wrap-around scan. */
static u_int32_t g_vmm_free_hint = 0;

/* High-water mark for the orphan-page audit log: the per-dead-pid breakdown is
 * printed only when the orphan count reaches a NEW maximum, so a steady count
 * (or repeated /proc/meminfo polls) cannot flood the serial console. */
static u_int32_t g_orphan_log_hwm = 0;

u_int32_t numPages = 0;

/* Physical address where the page bitmap is staged (set in vmm_mem_map_init,
 * read by paging.c to remap the bitmap into kernel virtual space). */
uintptr_t vmm_bitmap_phys = 0;

vmm_page_info_t *vmmMemoryMap = NULL;

/************************************************************************

 Function: void vmm_mem_map_init();
 Description: This Function Initializes The Memory Map For the System
 Notes:

 02/20/2004 - Made It Report Real And Available Memory

 ************************************************************************/
/*
 * Machine-independent page-bitmap helpers used by the per-arch vmm_mem_map_init
 * (sys/arch/<arch>/vmm_machdep.c).  The arch code detects RAM and decides the
 * layout; these own the bitmap contents and the free-page accounting, so
 * g_free_pages stays encapsulated here with the allocator that maintains it.
 */

/**
 * Stage the page bitmap at @bitmap_phys for @num_pages frames and mark every
 * frame unavailable.  Call once at init before vmm_mem_mark_available().
 */
void vmm_mem_bitmap_init(uintptr_t bitmap_phys, u_int32_t num_pages)
{
	u_int32_t i;

	numPages = num_pages;
	vmm_bitmap_phys = bitmap_phys;
	vmmMemoryMap = (vmm_page_info_t *)bitmap_phys;
	g_free_pages = 0;

	/* Bitmap lives in raw RAM, not BSS, so initialise every entry explicitly. */
	for (i = 0; i < numPages; i++)
	{
		vmmMemoryMap[i].cowCounter = 0;
		vmmMemoryMap[i].status = memNotavail;
		vmmMemoryMap[i].pid = vmmID;
		vmmMemoryMap[i].pageAddr = (uintptr_t)i * PAGE_SIZE;
	}
}

/**
 * Mark the half-open frame range [@first_page, @last_page) as available RAM,
 * updating the free-page count (and systemVitals->freePages when available).
 */
void vmm_mem_mark_available(u_int32_t first_page, u_int32_t last_page)
{
	u_int32_t i;

	for (i = first_page; i < last_page && i < numPages; i++)
	{
		if (vmmMemoryMap[i].status != memAvail)
		{
			vmmMemoryMap[i].status = memAvail;
			g_free_pages++;
		}
	}

	if (systemVitals)
		systemVitals->freePages = g_free_pages;
}

/**
 * @return current count of available physical pages.
 */
u_int32_t vmm_mem_free_pages(void)
{
	return g_free_pages;
}

/************************************************************************

 Function: u_int32_t vmm_find_free_page(pid_t pid);

 Description: This Returns A Free  Physical Page Address Then Marks It
 Not Available As Well As Setting The PID To The Proccess
 Allocating This Page
 Notes:

 ************************************************************************/
uintptr_t vmm_find_free_page(pidType pid)
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

	/* Circular scan starting at the rotating cursor (see g_vmm_free_hint). */
	for (u_int32_t scanned = 0; scanned < numPages; scanned++)
	{
		i = g_vmm_free_hint + scanned;
		if (i >= numPages)
			i -= numPages;

		/*
		 * If We Found A Free Page Set It To Not Available After That Set Its Own
		 * And Return The Address
		 */
		if ((vmmMemoryMap[i].status == memAvail) && (vmmMemoryMap[i].cowCounter == 0))
		{
			vmmMemoryMap[i].status = memNotavail;
			vmmMemoryMap[i].pid = pid;
			g_free_pages--;
			g_vmm_free_hint = (i + 1 < numPages) ? (i + 1) : 0;
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

/**
 * Find and claim a run of @count contiguous free physical pages.
 *
 * Like vmm_find_free_page but for a contiguous block — used where a caller needs
 * physically-contiguous frames (e.g. an identity-mapped kernel heap on arches
 * without a separate kernel page-table mapping step).  Marks the run not-available
 * and assigns @pid.  Does not evict on failure.
 *
 * @return physical address of the first page, or 0 if no run that large is free.
 */
uintptr_t vmm_find_free_pages_contig(u_int32_t count, pidType pid)
{
	u_int32_t i, c;

	if (pid < sysID)
		kpanic("Error: invalid PID %i\n", pid);
	if (count == 0)
		return 0;

	spinLock(&g_vmm_spin_lock);

	for (i = 0; i + count <= numPages; i++)
	{
		for (c = 0; c < count; c++)
		{
			if (vmmMemoryMap[i + c].status != memAvail || vmmMemoryMap[i + c].cowCounter != 0)
				break;
		}
		if (c != count)
		{
			i += c; /* skip past the blocking page */
			continue;
		}

		for (c = 0; c < count; c++)
		{
			vmmMemoryMap[i + c].status = memNotavail;
			vmmMemoryMap[i + c].pid = pid;
			g_free_pages--;
		}
		if (systemVitals)
			systemVitals->freePages = g_free_pages;

		spinUnlock(&g_vmm_spin_lock);
		return (vmmMemoryMap[i].pageAddr);
	}

	spinUnlock(&g_vmm_spin_lock);
	return 0;
}

/************************************************************************

 Function: int free_page(u_int32_t pageAddr);

 Description: This Function Marks The Page As Free

 Notes:

 ************************************************************************/
int free_page(uintptr_t page_addr)
{

	u_int32_t page_index = 0;
	assert((page_addr & 0xFFF) == 0);

	/* Find The Page Index To The Memory Map */
	page_index = page_addr / 4096;

	if (page_index >= numPages)
	{
		kprintf("free_page: addr %p out of bounds (index %u numPages %u mmap %p)\n",
		        (void *)page_addr,
		        page_index,
		        numPages,
		        (void *)vmmMemoryMap);
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
int adjust_cow_counter(uintptr_t base_addr, int adjustment)
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
void vmm_share_ref(uintptr_t phys)
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

/**
 * Audit physical pages owned by processes that no longer exist.
 *
 * Walks vmmMemoryMap and counts every in-use page owned by a real user PID (> 1)
 * that schedFindTask() can no longer find — an "orphaned" frame whose owning
 * process died without its teardown path reclaiming it.  The total is returned
 * (surfaced via /proc/meminfo "OrphanPages") and is normally a small constant; a
 * value that climbs each logout/login cycle indicates a process-teardown leak.
 *
 * Pages with cowCounter > 0 are skipped: a copy-on-write or shared frame carries
 * the PID of its ORIGINAL allocator, so once that process exits while a forked
 * child still maps the frame it would otherwise be miscounted as orphaned — the
 * frame is still live (the COW counter tracks its remaining mappers) and is freed
 * when the last one exits.  Without this a fork-heavy workload (a build) inflates
 * the count with tens of thousands of false positives.
 *
 * The per-dead-PID breakdown is logged to the serial console only when the count
 * sets a new high-water mark, so a steady count under repeated /proc/meminfo polls
 * cannot flood the console.
 *
 * @return number of in-use, unreferenced pages owned by dead PIDs.
 */
u_int32_t vmm_audit_orphan_pages(void)
{
	struct holder
	{
		pid_t pid;
		u_int32_t count;
	};
	struct holder dead[32]; /* per-dead-pid breakdown (logged only on a leak) */
	u_int32_t ndead = 0;
	u_int32_t orphan_total = 0;
	u_int32_t dead_overflow = 0;
	u_int32_t i = 0, h = 0;

	for (i = 0; i < numPages; i++)
	{
		pid_t owner = vmmMemoryMap[i].pid;

		if (vmmMemoryMap[i].status == memAvail)
			continue;
		if (owner <= 1 || owner == vmmID) /* kernel/init/free — not a leak */
			continue;
		if (vmmMemoryMap[i].cowCounter > 0) /* still referenced by a live COW/shared mapper */
			continue;
		if (schedFindTask(owner) != NULL) /* owner still alive */
			continue;

		orphan_total++;
		for (h = 0; h < ndead; h++)
			if (dead[h].pid == owner)
				break;
		if (h < ndead)
			dead[h].count++;
		else if (ndead < (sizeof(dead) / sizeof(dead[0])))
		{
			dead[ndead].pid = owner;
			dead[ndead].count = 1;
			ndead++;
		}
		else
			dead_overflow++;
	}

	/* Log the breakdown only when the count sets a NEW high — reading /proc/meminfo
	 * (this function's only caller) is polled by the taskbar, and printing on every
	 * poll flooded the serial console. */
	if (orphan_total > g_orphan_log_hwm)
	{
		g_orphan_log_hwm = orphan_total;
		kprintf("vmm_audit: %u orphan pages (%u kB) owned by %u dead pid(s)\n",
		        orphan_total,
		        orphan_total * 4,
		        ndead + (dead_overflow ? 1 : 0));
		for (i = 0; i < ndead; i++)
			kprintf("vmm_audit:   dead pid %d -> %u pages (%u kB)\n",
			        dead[i].pid,
			        dead[i].count,
			        dead[i].count * 4);
		if (dead_overflow)
			kprintf("vmm_audit:   (+%u more orphan pages from additional dead pids)\n", dead_overflow);
	}

	return orphan_total;
}
