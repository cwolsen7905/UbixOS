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

#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <ubixos/kpanic.h>
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <vmm/vmm.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>

/*
 Three descriptor lists:
   usedKernDesc  - descriptors currently backing live allocations
   freeKernDesc  - free blocks, kept sorted by base address (ascending)
                   so adjacent blocks can be coalesced in O(1) on every kfree
   emptyKernDesc - descriptor objects with no memory backing, ready for reuse
*/
static struct memDescriptor *usedKernDesc = 0x0;
static struct memDescriptor *freeKernDesc = 0x0;
static struct memDescriptor *emptyKernDesc = 0x0;

static struct spinLock mallocSpinLock = SPIN_LOCK_INITIALIZER;

/* Return a descriptor object from the empty pool, allocating a new page of
   them when the pool is empty.  Always called with mallocSpinLock held. */
static struct memDescriptor *getEmptyDesc(void)
{
	struct memDescriptor *tmpDesc = 0x0;
	int i;

	if (emptyKernDesc != 0x0)
	{
		tmpDesc = emptyKernDesc;
		emptyKernDesc = tmpDesc->next;
		if (emptyKernDesc != 0x0)
			emptyKernDesc->prev = 0x0;
		tmpDesc->next = 0x0;
		tmpDesc->prev = 0x0;
		return (tmpDesc);
	}

	emptyKernDesc = (struct memDescriptor *)vmm_getFreeMallocPage(4);
	if (emptyKernDesc == 0x0)
		kpanic("kmalloc: vmm_getFreeMallocPage returned NULL\n");

	memset(emptyKernDesc, 0x0, 0x4000);

	emptyKernDesc[0].next = &emptyKernDesc[1];
	for (i = 1; i < (int)(0x4000 / sizeof(struct memDescriptor)); i++)
	{
		emptyKernDesc[i].prev = &emptyKernDesc[i - 1];
		if (i + 1 < (int)(0x4000 / sizeof(struct memDescriptor)))
			emptyKernDesc[i].next = &emptyKernDesc[i + 1];
		else
			emptyKernDesc[i].next = 0x0;
	}

	tmpDesc = &emptyKernDesc[0];
	emptyKernDesc = tmpDesc->next;
	emptyKernDesc->prev = 0x0;
	tmpDesc->next = 0x0;
	tmpDesc->prev = 0x0;
	return (tmpDesc);
}

/* Return a descriptor object to the empty pool (called with mallocSpinLock
   held, so no need to acquire emptyDescSpinLock separately). */
static void returnEmptyDesc(struct memDescriptor *desc)
{
	desc->baseAddr = 0x0;
	desc->limit = 0x0;
	desc->prev = 0x0;
	desc->next = emptyKernDesc;
	if (emptyKernDesc != 0x0)
		emptyKernDesc->prev = desc;
	emptyKernDesc = desc;
}

/*
 Insert a free descriptor into the free list, which is kept sorted by base
 address (ascending).  After insertion, merge with the immediately following
 and/or preceding block if they are physically adjacent.  This gives O(1)
 coalescing on every kfree and eliminates heap fragmentation over time.
*/
static void insertFreeDesc(struct memDescriptor *freeDesc)
{
	struct memDescriptor *cur = 0x0;

	assert(freeDesc);
	if (freeDesc->limit == 0x0)
		kpanic("kmalloc: inserting descriptor with zero limit\n");

	/* --- insert in address order --- */
	if (freeKernDesc == 0x0 || (uInt32)freeDesc->baseAddr < (uInt32)freeKernDesc->baseAddr)
	{
		freeDesc->prev = 0x0;
		freeDesc->next = freeKernDesc;
		if (freeKernDesc != 0x0)
			freeKernDesc->prev = freeDesc;
		freeKernDesc = freeDesc;
	}
	else
	{
		for (cur = freeKernDesc; cur->next != 0x0; cur = cur->next)
		{
			if ((uInt32)freeDesc->baseAddr < (uInt32)cur->next->baseAddr)
				break;
		}
		freeDesc->next = cur->next;
		freeDesc->prev = cur;
		if (cur->next != 0x0)
			cur->next->prev = freeDesc;
		cur->next = freeDesc;
	}

	/* --- coalesce with next block if physically adjacent --- */
	if (freeDesc->next != 0x0 && (uInt32)freeDesc->baseAddr + freeDesc->limit == (uInt32)freeDesc->next->baseAddr)
	{
		struct memDescriptor *next = freeDesc->next;
		freeDesc->limit += next->limit;
		freeDesc->next = next->next;
		if (next->next != 0x0)
			next->next->prev = freeDesc;
		returnEmptyDesc(next);
	}

	/* --- coalesce with prev block if physically adjacent --- */
	if (freeDesc->prev != 0x0 && (uInt32)freeDesc->prev->baseAddr + freeDesc->prev->limit == (uInt32)freeDesc->baseAddr)
	{
		struct memDescriptor *prev = freeDesc->prev;
		prev->limit += freeDesc->limit;
		prev->next = freeDesc->next;
		if (freeDesc->next != 0x0)
			freeDesc->next->prev = prev;
		returnEmptyDesc(freeDesc);
	}
}

/*
 kmalloc: allocate len bytes of kernel memory.

 Walks the free list (address order, first-fit) for a block large enough.
 If the block is larger than needed the remainder is split and reinserted.
 When no free block is large enough a new page run is obtained from VMM and
 any unused tail is returned to the free list so it is immediately available
 for the next allocation.
*/
void *kmalloc(uInt32 len)
{
	struct memDescriptor *tmpDesc1 = 0x0;
	struct memDescriptor *tmpDesc2 = 0x0;
	uInt32 pages = 0x0;

	spinLock(&mallocSpinLock);

	if (len > 0xFFFFFFE0u)
	{
		spinUnlock(&mallocSpinLock);
		kprintf("kmalloc: request too large (0x%X)\n", len);
		return (0x0);
	}

	len = MALLOC_ALIGN(len);

	if (len == 0x0)
	{
		spinUnlock(&mallocSpinLock);
		kprintf("kmalloc: len = 0\n");
		return (0x0);
	}

	/* First-fit search */
	for (tmpDesc1 = freeKernDesc; tmpDesc1 != 0x0; tmpDesc1 = tmpDesc1->next)
	{
		if (tmpDesc1->limit >= len)
		{
			/* Unlink from free list */
			if (tmpDesc1->prev != 0x0)
				tmpDesc1->prev->next = tmpDesc1->next;
			if (tmpDesc1->next != 0x0)
				tmpDesc1->next->prev = tmpDesc1->prev;
			if (tmpDesc1 == freeKernDesc)
				freeKernDesc = tmpDesc1->next;

			/* Split remainder back into free list */
			if (tmpDesc1->limit > len)
			{
				tmpDesc2 = getEmptyDesc();
				assert(tmpDesc2);
				tmpDesc2->baseAddr = (struct memDescriptor *)((uInt32)tmpDesc1->baseAddr + len);
				tmpDesc2->limit = tmpDesc1->limit - len;
				tmpDesc2->next = 0x0;
				tmpDesc2->prev = 0x0;
				tmpDesc1->limit = len;
				insertFreeDesc(tmpDesc2);
			}

			/* Link into used list */
			tmpDesc1->prev = 0x0;
			tmpDesc1->next = usedKernDesc;
			if (usedKernDesc != 0x0)
				usedKernDesc->prev = tmpDesc1;
			usedKernDesc = tmpDesc1;

			memset(tmpDesc1->baseAddr, 0x0, tmpDesc1->limit);
			spinUnlock(&mallocSpinLock);
			assert(tmpDesc1->baseAddr);
			return (tmpDesc1->baseAddr);
		}
	}

	/* No free block large enough — get fresh pages from VMM */
	tmpDesc1 = getEmptyDesc();
	if (tmpDesc1 == 0x0)
	{
		spinUnlock(&mallocSpinLock);
		return (0x0);
	}

	pages = (len + 4095) / 4096;
	tmpDesc1->baseAddr = (struct memDescriptor *)vmm_getFreeMallocPage(pages);
	if (tmpDesc1->baseAddr == 0x0)
	{
		returnEmptyDesc(tmpDesc1);
		spinUnlock(&mallocSpinLock);
		return (0x0);
	}
	tmpDesc1->limit = len;
	tmpDesc1->prev = 0x0;
	tmpDesc1->next = usedKernDesc;
	if (usedKernDesc != 0x0)
		usedKernDesc->prev = tmpDesc1;
	usedKernDesc = tmpDesc1;

	/* Return any page tail above the requested size */
	if ((uInt32)(pages * 4096) > len)
	{
		tmpDesc2 = getEmptyDesc();
		assert(tmpDesc2);
		tmpDesc2->baseAddr = (struct memDescriptor *)((uInt32)tmpDesc1->baseAddr + len);
		tmpDesc2->limit = (pages * 4096) - len;
		tmpDesc2->prev = 0x0;
		tmpDesc2->next = 0x0;
		insertFreeDesc(tmpDesc2);
	}

	memset(tmpDesc1->baseAddr, 0x0, tmpDesc1->limit);
	spinUnlock(&mallocSpinLock);
	assert(tmpDesc1->baseAddr);
	return (tmpDesc1->baseAddr);
}

/*
 kfree: return a previously kmalloc'd block to the free list.

 Finds the descriptor in the used list by base address, unlinks it, and
 calls insertFreeDesc which coalesces with any adjacent free blocks.
*/
void kfree(void *baseAddr)
{
	struct memDescriptor *tmpDesc = 0x0;

	if (baseAddr == 0x0)
		return;

	assert(usedKernDesc);
	spinLock(&mallocSpinLock);

	for (tmpDesc = usedKernDesc; tmpDesc != 0x0; tmpDesc = tmpDesc->next)
	{
		if (tmpDesc->baseAddr == baseAddr)
		{
			/* Poison freed memory to catch use-after-free */
			memset(tmpDesc->baseAddr, 0xBE, tmpDesc->limit);

			if (usedKernDesc == tmpDesc)
				usedKernDesc = tmpDesc->next;
			if (tmpDesc->prev != 0x0)
				tmpDesc->prev->next = tmpDesc->next;
			if (tmpDesc->next != 0x0)
				tmpDesc->next->prev = tmpDesc->prev;

			tmpDesc->next = 0x0;
			tmpDesc->prev = 0x0;

			insertFreeDesc(tmpDesc);
			spinUnlock(&mallocSpinLock);
			return;
		}
	}

	/* Check if this pointer is already on the free list — double-free. */
	for (tmpDesc = freeKernDesc; tmpDesc != 0x0; tmpDesc = tmpDesc->next) {
		if (tmpDesc->baseAddr == baseAddr) {
			spinUnlock(&mallocSpinLock);
			kpanic("kfree: double-free 0x%X\n", (uInt32)baseAddr);
			return;
		}
	}

	spinUnlock(&mallocSpinLock);
	kprintf("kfree: descriptor not found for 0x%X\n", (uInt32)baseAddr);
}
