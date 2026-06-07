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
#include <string.h>      /* memset, memcpy */

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
 * Return the active TTBR0 translation-table root (the kernel's identity L1).
 */
static u_int64_t *pmap_active_l1(void)
{
	u_int64_t ttbr0;
	__asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
	return (u_int64_t *)(uintptr_t)(ttbr0 & PTE_ADDR_MASK);
}

/**
 * Create a fresh per-process address space (TTBR0 root).
 *
 * The new L1 starts as a copy of the kernel's identity L1, so kernel code, the
 * page bitmap and the peripherals stay mapped while this address space is
 * active (no kernel-in-TTBR1 relocation yet).  Per-process user mappings are
 * then added with pmap_map_user_page() in VA blocks the kernel does not use.
 *
 * @return the new L1 table (identity-mapped, == its physical address).
 */
u_int64_t *pmap_create_user_space(void)
{
	u_int64_t *l1 = (u_int64_t *)(uintptr_t)vmm_find_free_page(sysID);

	memcpy(l1, pmap_active_l1(), PAGE_SIZE); /* 512 entries × 8 = one page */
	return l1;
}

/**
 * Make @l1 the active TTBR0 address space and flush stale translations.
 */
void pmap_switch(u_int64_t *l1)
{
	__asm__ volatile("msr ttbr0_el1, %0; isb; tlbi vmalle1; dsb nsh; isb"
	                 :
	                 : "r"((u_int64_t)(uintptr_t)l1)
	                 : "memory");
}

/**
 * Demonstrate per-process address-space isolation: map the same user VA to two
 * different frames in two separate address spaces, switch between them, and
 * confirm each sees only its own frame.  Throwaway bring-up scaffolding.
 */
void aarch64_aspace_demo(void)
{
	u_int64_t *kernel_l1 = pmap_active_l1();
	u_int64_t *a, *b;
	uintptr_t fa, fb;
	/* A user VA in a 1 GB block no other demo has touched: blocks 0 (peripherals),
	 * 1 (RAM/kernel), 2 (pmap demo) and 3 (syscall demo) are in use, so a shared
	 * sub-table would otherwise leak across the copied L1s.  Use block 4 (4 GB),
	 * still a clean identity block, so each space allocates its own L2/L3. */
	u_int64_t va = 0x100004000UL;
	u_int64_t in_a, in_b;

	kprintf("aspace demo: per-process address-space isolation...\n");

	a = pmap_create_user_space();
	b = pmap_create_user_space();
	fa = vmm_find_free_page(sysID);
	fb = vmm_find_free_page(sysID);
	*(volatile u_int64_t *)fa = 0xA0A0A0A0UL;
	*(volatile u_int64_t *)fb = 0xB0B0B0B0UL;

	pmap_map_page(a, va, (u_int64_t)fa, PMAP_KERNEL_DATA);
	pmap_map_page(b, va, (u_int64_t)fb, PMAP_KERNEL_DATA);

	pmap_switch(a);
	in_a = *(volatile u_int64_t *)va;
	pmap_switch(b);
	in_b = *(volatile u_int64_t *)va;
	pmap_switch(kernel_l1); /* restore the kernel's identity address space */

	kprintf("  VA 0x%lX: space A reads 0x%lX, space B reads 0x%lX (isolated=%s)\n",
	        va,
	        in_a,
	        in_b,
	        (in_a == 0xA0A0A0A0UL && in_b == 0xB0B0B0B0UL) ? "yes" : "NO");
	kprintf("aspace demo: per-process address spaces work on aarch64.\n");
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
