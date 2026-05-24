/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */

#include <vmm/swap.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <pci/hd.h>
#include <lib/kprintf.h>
#include <ubixos/sched.h>

static struct driveInfo *g_swap_dev = 0;
static u_int32_t g_swap_total = 0; /* pages actually available */
static int g_swap_ready = 0;

/* Bitmap: bit=1 means slot is in use. */
static u_int32_t g_swap_bitmap[SWAP_PAGES / 32];

void swap_register_dev(struct driveInfo *hdd, u_int32_t total_pages)
{
	g_swap_dev = hdd;
	g_swap_total = (total_pages < SWAP_PAGES) ? total_pages : SWAP_PAGES;
	g_swap_ready = 1;
	kprintf("swap: %u pages (%u MB) on device\n", g_swap_total, g_swap_total * 4 / 1024);
}

int swap_enabled(void)
{
	return g_swap_ready;
}

u_int32_t swap_alloc_slot(void)
{
	u_int32_t i, bit;

	for (i = 0; i < g_swap_total / 32; i++)
	{
		if (g_swap_bitmap[i] == 0xFFFFFFFFU)
		{
			continue;
		}
		for (bit = 0; bit < 32; bit++)
		{
			if ((g_swap_bitmap[i] & (1u << bit)) == 0)
			{
				g_swap_bitmap[i] |= (1u << bit);
				return (i * 32) + bit;
			}
		}
	}
	return (u_int32_t)-1;
}

void swap_free_slot(u_int32_t slot)
{
	if (slot < g_swap_total)
	{
		g_swap_bitmap[slot / 32] &= ~(1u << (slot % 32));
	}
}

int swap_write_page(u_int32_t slot, void *virt_addr)
{
	return hdWrite(g_swap_dev, virt_addr, slot * SWAP_SECTORS_PER_PAGE, SWAP_SECTORS_PER_PAGE);
}

int swap_read_page(u_int32_t slot, void *virt_addr)
{
	return hdRead(g_swap_dev, virt_addr, slot * SWAP_SECTORS_PER_PAGE, SWAP_SECTORS_PER_PAGE);
}

/*
 * Walk _current's user page tables looking for a victim page.
 *
 * Clock-like second-chance: pages with PAGE_ACCESSED set get one reprieve
 * (the bit is cleared) and the scan continues.  The first unaccessed,
 * non-COW, non-MMIO present page is evicted: written to swap, its PTE
 * replaced with a swap encoding, TLB invalidated, and freePage() called
 * to return the physical frame to the free pool.
 *
 * Returns the physical address of the reclaimed page, or 0 on failure.
 */
u_int32_t swap_evict_page(void)
{
	u_int32_t *page_dir = (u_int32_t *)PD_BASE_ADDR;
	u_int32_t *page_table = 0;
	u_int32_t pd_i, pt_i, pte, phys, vaddr, slot;

	if (!g_swap_ready)
	{
		return (0);
	}

	/* Two-pass: first pass gives second chances, second pass evicts. */
	int pass;
	for (pass = 0; pass < 2; pass++)
	{
		for (pd_i = PD_INDEX(VMM_USER_START); pd_i <= PD_INDEX(VMM_USER_END); pd_i++)
		{

			if ((page_dir[pd_i] & PAGE_PRESENT) != PAGE_PRESENT)
			{
				continue;
			}

			page_table = (u_int32_t *)(PT_BASE_ADDR + (pd_i * PAGE_SIZE));

			for (pt_i = 0; pt_i < (u_int32_t)PT_ENTRIES; pt_i++)
			{
				pte = page_table[pt_i];

				if ((pte & PAGE_PRESENT) != PAGE_PRESENT)
				{
					continue;
				}
				if ((pte & PAGE_COW) == PAGE_COW)
				{
					continue;
				}

				phys = pte & 0xFFFFF000;
				if ((phys >> 12) >= (u_int32_t)numPages)
				{
					continue; /* MMIO — never evict */
				}

				if (pte & PAGE_ACCESSED)
				{
					/* Second chance: clear and continue. */
					page_table[pt_i] &= ~PAGE_ACCESSED;
					continue;
				}

				/* Found a victim. */
				vaddr = (pd_i << 22) | (pt_i << 12);

				slot = swap_alloc_slot();
				if (slot == (u_int32_t)-1)
				{
					kprintf("swap: no free slots\n");
					return (0);
				}

				if (swap_write_page(slot, (void *)vaddr) != 0)
				{
					kprintf("swap: write error (slot %u)\n", slot);
					swap_free_slot(slot);
					return (0);
				}

				page_table[pt_i] = PTE_SWAP_ENCODE(slot);
				asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");

				/*
				 * freePage handles vmmMemoryMap accounting and
				 * freePages increment; it acquires vmmSpinLock
				 * internally, which is safe here because the
				 * caller released it before calling us.
				 */
				freePage(phys);
				return (phys);
			}
		}
	}

	kprintf("swap: no eviction candidate found\n");
	return (0);
}
