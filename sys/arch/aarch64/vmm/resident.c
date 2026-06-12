/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 resident-set accounting: count the physical pages mapped into a task's
 * user address space, for /proc/<pid>/statm (the activity monitor's memory
 * column).  This is the machine-dependent md_resident_pages() the MI procfs layer
 * calls; i386 has its own stub.
 *
 * Why a page-table walk works cleanly here: aarch64 has no TTBR1 split yet, so a
 * process's md_ttbr0 maps BOTH the kernel identity map and the task's user pages.
 * We therefore count only leaf entries with PTE_AP_EL0 set (EL0-accessible =
 * user) and ignore the kernel identity blocks.  All RAM is identity-mapped, so a
 * table's physical address is directly usable as a kernel pointer — no temporary
 * mapping is needed to walk another task's tables from procfs context.
 *
 * The PTE bit definitions are duplicated from pmap.c (a handful of standard
 * ARMv8-A descriptor bits) on purpose, to keep this read-only accounting out of
 * the actively-evolving pmap mapping code.
 */

#include <ubixos/sched.h> /* kTask_t, t->md.md_ttbr0 */
#include <sys/types.h>

#define PTE_VALID (1UL << 0)
#define PTE_TYPE_MASK (3UL)
#define PTE_TABLE (3UL)                  /* L1/L2 descriptor → next-level table   */
#define PTE_AP_EL0 (1UL << 6)            /* AP[1]: page is accessible at EL0 (user) */
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000UL
#define PT_ENTRIES 512                   /* 4 KB granule: 512 descriptors/table   */
#define L2_BLOCK_PAGES 512UL             /* a 2 MB L2 block = 512 × 4 KB pages     */
#define L1_BLOCK_PAGES (512UL * 512UL)   /* a 1 GB L1 block = 512² × 4 KB pages    */

/**
 * Count physical pages mapped EL0-accessible (user) in @t's address space.
 *
 * Walks md_ttbr0's L1→L2→L3 tables, tallying leaf entries (L3 pages and L2/L1
 * blocks) whose PTE_AP_EL0 bit is set.  Kernel-only identity mappings (no
 * PTE_AP_EL0) are skipped, so the result is the task's user resident set.
 *
 * @return resident user pages, or 0 for a kernel thread (md_ttbr0 == 0).
 */
u_int32_t md_resident_pages(kTask_t *t)
{
	u_int64_t ttbr0 = t->md.md_ttbr0;
	u_int64_t *l1;
	u_int32_t count = 0;
	int i, j, k;

	if (ttbr0 == 0)
		return 0;

	l1 = (u_int64_t *)(uintptr_t)(ttbr0 & PTE_ADDR_MASK);
	for (i = 0; i < PT_ENTRIES; i++)
	{
		u_int64_t d1 = l1[i];
		u_int64_t *l2;

		if ((d1 & PTE_VALID) == 0)
			continue;
		if ((d1 & PTE_TYPE_MASK) != PTE_TABLE)
		{ /* 1 GB block — almost always the kernel identity map */
			if (d1 & PTE_AP_EL0)
				count += L1_BLOCK_PAGES;
			continue;
		}

		l2 = (u_int64_t *)(uintptr_t)(d1 & PTE_ADDR_MASK);
		for (j = 0; j < PT_ENTRIES; j++)
		{
			u_int64_t d2 = l2[j];
			u_int64_t *l3;

			if ((d2 & PTE_VALID) == 0)
				continue;
			if ((d2 & PTE_TYPE_MASK) != PTE_TABLE)
			{ /* 2 MB block */
				if (d2 & PTE_AP_EL0)
					count += L2_BLOCK_PAGES;
				continue;
			}

			l3 = (u_int64_t *)(uintptr_t)(d2 & PTE_ADDR_MASK);
			for (k = 0; k < PT_ENTRIES; k++)
			{
				u_int64_t d3 = l3[k];
				if ((d3 & PTE_VALID) && (d3 & PTE_AP_EL0))
					count++;
			}
		}
	}
	return count;
}
