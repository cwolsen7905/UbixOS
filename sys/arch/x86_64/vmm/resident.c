/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 resident-set accounting: count the physical pages mapped into a task's
 * user address space, for /proc/<pid>/statm (the Activity Monitor's memory
 * column).  This is the machine-dependent md_resident_pages() the MI procfs
 * layer calls; sibling of aarch64/vmm/resident.c.
 *
 * A process's user pages live under PML4[0] in PDPT[1..511] (VAs >= 1 GB) — the
 * private region x86_64_fork_copy walks; PDPT[0] is the shared kernel low-1 GB
 * identity and PML4[256..511] is the kernel higher half, both excluded.  We
 * count leaf entries marked PTE_US (user-accessible): 4 KB PT pages and 2 MB PD
 * blocks (PTE_PS).  Tables are reachable through the physmap (P2V), so another
 * task's CR3 can be walked from procfs context without a temporary mapping.
 *
 * The PTE bit definitions mirror fork.c / usermode.c (standard x86-64 paging
 * bits) on purpose, to keep this read-only accounting out of the mapping code.
 */

#include <ubixos/sched.h> /* kTask_t, t->md.md_cr3 */
#include <sys/types.h>

#define PTE_P 0x1
#define PTE_US 0x4
#define PTE_PS 0x80 /* 2 MB page (PD leaf) */
#define PTE_ADDR_MASK (~0xFFFUL)
#define PHYSMAP_BASE 0xFFFF800000000000UL
#define P2V_TBL(p) ((u_int64_t *)((u_int64_t)((p) & PTE_ADDR_MASK) + PHYSMAP_BASE))
#define PT_ENTRIES 512
#define PD_BLOCK_PAGES 512UL /* a 2 MB PD block = 512 × 4 KB pages */

/**
 * Count physical pages mapped user-accessible (PTE_US) in @t's address space.
 *
 * Walks md_cr3's PML4[0] → PDPT[1..511] → PD → PT, tallying leaf entries (PT
 * pages and 2 MB PD blocks) whose PTE_US bit is set.  The kernel identity
 * (PDPT[0]) and higher half are skipped, so the result is the task's user
 * resident set.
 *
 * @return resident user pages, or 0 for a kernel thread (md_cr3 == 0).
 */
u_int32_t md_resident_pages(kTask_t *t)
{
	u_int64_t cr3 = t->md.md_cr3;
	u_int64_t *pml4, *pdpt;
	u_int32_t count = 0;
	int pi, di, ti;

	if (cr3 == 0)
		return 0;

	pml4 = P2V_TBL(cr3);
	if ((pml4[0] & PTE_P) == 0)
		return 0;
	pdpt = P2V_TBL(pml4[0]);

	for (pi = 1; pi < PT_ENTRIES; pi++) /* skip PDPT[0] (kernel identity) */
	{
		u_int64_t *pd;

		if ((pdpt[pi] & PTE_P) == 0)
			continue;
		pd = P2V_TBL(pdpt[pi]);
		for (di = 0; di < PT_ENTRIES; di++)
		{
			u_int64_t d = pd[di];
			u_int64_t *pt;

			if ((d & PTE_P) == 0)
				continue;
			if (d & PTE_PS)
			{ /* 2 MB block */
				if (d & PTE_US)
					count += PD_BLOCK_PAGES;
				continue;
			}
			pt = P2V_TBL(d);
			for (ti = 0; ti < PT_ENTRIES; ti++)
			{
				u_int64_t p = pt[ti];
				if ((p & PTE_P) && (p & PTE_US))
					count++;
			}
		}
	}
	return count;
}
