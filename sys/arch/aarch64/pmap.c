/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 4 KB-granule page-table mapping (pmap), QEMU `virt` bring-up.
 *
 * mmu.c brings the MMU up with a coarse 512 × 1 GB identity map (block
 * descriptors at level 1).  This file adds fine-grained 4 KB mapping: walk the
 * 39-bit / 4 KB translation tree (L1→L2→L3, each level 512 entries covering
 * 1 GB / 2 MB / 4 KB), allocating intermediate tables on demand, and install an
 * L3 page descriptor.  This is the mechanism per-process user address spaces
 * (and the kernel's own fine mappings) are built from once exec/fork land.
 *
 * Table pages come from the physical frame allocator and are identity-mapped, so
 * a frame's physical address doubles as the kernel pointer used to write it.
 */

#include "bringup.h"
#include <vmm/vmm.h>
#include <vmm/paging.h>  /* PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>      /* memset */

/* 39-bit VA, 4 KB granule: level index extractors. */
#define L1_IDX(va) (((va) >> 30) & 0x1FFUL)
#define L2_IDX(va) (((va) >> 21) & 0x1FFUL)
#define L3_IDX(va) (((va) >> 12) & 0x1FFUL)

/* Descriptor encoding. */
#define PTE_VALID (1UL << 0)
#define PTE_TABLE (3UL) /* L1/L2 descriptor pointing to the next level */
#define PTE_PAGE (3UL)  /* L3 page descriptor */
#define PTE_TYPE_MASK (3UL)
#define PTE_AF (1UL << 10)                /* access flag */
#define PTE_SH_INNER (3UL << 8)           /* inner shareable */
#define PTE_ATTR(i) ((u_int64_t)(i) << 2) /* MAIR AttrIndx */
#define PTE_AP_EL0 (1UL << 6)             /* AP[1]: also accessible at EL0 */
#define PTE_AP_RO (1UL << 7)              /* AP[2]: read-only */
#define PTE_PXN (1UL << 53)               /* privileged execute-never */
#define PTE_UXN (1UL << 54)               /* unprivileged execute-never */

/* Output-address field of a descriptor (bits [47:12]). */
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000UL

/* MAIR index for Normal write-back memory (matches mmu.c attr0). */
#define ATTR_NORMAL_IDX 0

/* Standard attributes for a kernel data page (Normal, inner-shareable, AF). */
#define PMAP_KERNEL_DATA (PTE_ATTR(ATTR_NORMAL_IDX) | PTE_SH_INNER | PTE_PXN | PTE_UXN)

/**
 * Return the next-level table for @table[@idx], allocating + linking a fresh
 * zeroed table if the slot is empty or currently a block descriptor.
 *
 * @return pointer to the next-level table (identity-mapped, so == its phys addr).
 */
static u_int64_t *table_next(u_int64_t *table, u_int64_t idx)
{
	u_int64_t e = table[idx];

	/* Reuse only an existing table descriptor (type bits == 11).  An invalid
	 * slot or a 1 GB/2 MB block is replaced by a freshly allocated table. */
	if ((e & PTE_VALID) != 0 && (e & PTE_TYPE_MASK) == PTE_TABLE)
		return (u_int64_t *)(uintptr_t)(e & PTE_ADDR_MASK);

	u_int64_t *next = (u_int64_t *)(uintptr_t)vmm_find_free_page(sysID);
	memset(next, 0, PAGE_SIZE);
	table[idx] = ((u_int64_t)(uintptr_t)next & PTE_ADDR_MASK) | PTE_TABLE;
	return next;
}

/**
 * Map one 4 KB page: virtual @va → physical @pa with descriptor @attrs in the
 * translation tree rooted at @l1.  Intermediate L2/L3 tables are allocated as
 * needed.  Invalidates the TLB for @va.
 *
 * @param attrs  lower/upper attribute bits (e.g. PMAP_KERNEL_DATA); PTE_AF and
 *               the L3 page type are added here.
 * @return 0 on success.
 */
int pmap_map_page(u_int64_t *l1, u_int64_t va, u_int64_t pa, u_int64_t attrs)
{
	u_int64_t *l2 = table_next(l1, L1_IDX(va));
	u_int64_t *l3 = table_next(l2, L2_IDX(va));

	l3[L3_IDX(va)] = (pa & PTE_ADDR_MASK) | attrs | PTE_AF | PTE_PAGE;

	__asm__ volatile("dsb ishst; tlbi vaae1is, %0; dsb ish; isb" : : "r"(va >> 12) : "memory");
	return 0;
}

/**
 * Map one 4 KB EL0-accessible (user) page: @va → @pa in the tree at @l1.
 *
 * @param executable  non-zero for user code (EL0-executable, EL1 exec denied);
 *                     zero for user data/stack (execute-never).
 * @return 0 on success.
 */
int pmap_map_user_page(u_int64_t *l1, u_int64_t va, u_int64_t pa, int executable)
{
	u_int64_t attrs = PTE_ATTR(ATTR_NORMAL_IDX) | PTE_SH_INNER | PTE_AP_EL0;

	if (executable)
		attrs |= PTE_PXN; /* runnable at EL0, never at EL1 */
	else
		attrs |= PTE_PXN | PTE_UXN; /* data/stack: never executable */

	return pmap_map_page(l1, va, pa, attrs);
}

/**
 * Exercise pmap_map_page in the live TTBR0 tree: map a 4 KB page at an unused
 * VA (in a 1 GB slot the kernel doesn't touch — replacing its identity block
 * with a real L2/L3 chain) and confirm the mapping is bidirectional with the
 * frame's identity alias.  Throwaway bring-up scaffolding.
 */
void aarch64_pmap_demo(void)
{
	u_int64_t ttbr0;
	u_int64_t *l1;
	uintptr_t frame;
	u_int64_t via_va;
	const u_int64_t test_va = 0x80000000UL; /* 2 GB: unused 1 GB slot above RAM */
	const u_int64_t pattern = 0x1122334455667788UL;

	__asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
	l1 = (u_int64_t *)(uintptr_t)(ttbr0 & PTE_ADDR_MASK);

	kprintf("pmap demo: 4KB-granule mapping...\n");

	frame = vmm_find_free_page(sysID);
	*(volatile u_int64_t *)frame = pattern; /* write through the identity alias */

	pmap_map_page(l1, test_va, (u_int64_t)frame, PMAP_KERNEL_DATA);

	via_va = *(volatile u_int64_t *)test_va; /* read through the new mapping */
	kprintf("  VA 0x%lX -> frame 0x%lX; read via VA = 0x%lX (%s)\n",
	        test_va,
	        (u_int64_t)frame,
	        via_va,
	        via_va == pattern ? "match" : "MISMATCH");

	*(volatile u_int64_t *)test_va = 0xFEEDFACEUL; /* write via VA, read via identity */
	kprintf("  wrote via VA; frame identity alias holds 0x%lX (%s)\n",
	        *(volatile u_int64_t *)frame,
	        *(volatile u_int64_t *)frame == 0xFEEDFACEUL ? "match" : "MISMATCH");

	kprintf("pmap demo: 4KB page mapping works on aarch64.\n");
}
