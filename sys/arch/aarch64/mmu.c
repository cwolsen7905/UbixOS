/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 MMU bring-up (QEMU `virt`, Phase 13a).
 *
 * Minimal identity map so the kernel runs translated: a single level-1 table
 * (39-bit VA, 4 KB granule) of 512 × 1 GB block descriptors, VA == PA.  The
 * first 1 GB (peripherals — GICv2 @0x08000000, PL011 @0x09000000) is Device
 * memory; the rest (RAM @0x40000000+) is Normal write-back cacheable.  Enough to
 * enable the MMU + caches and keep executing; real per-process TTBR0 page tables
 * (the high/low split, vmm port) come with the generic-kernel port.
 */

#include "bringup.h"

#define MMU_1GB (1UL << 30)

/* Level-1 table: 512 entries, each a 1 GB block.  4 KB aligned (one page). */
static u_int64_t l1_table[512] __attribute__((aligned(4096)));

/**
 * The kernel's identity L1 root.  New address spaces (pmap_create_user_space,
 * fork) copy this so they all map the kernel + identity-mapped RAM, regardless
 * of which process address space is currently active.
 */
u_int64_t *aarch64_kernel_l1(void)
{
	return l1_table;
}

/* Block/page descriptor bits. */
#define DESC_BLOCK 0x1UL                   /* [1:0] = 01: block at level 1/2 */
#define DESC_ATTR(i) ((u_int64_t)(i) << 2) /* [4:2] = MAIR AttrIndx */
#define DESC_SH_INNER (3UL << 8)           /* [9:8] = 11: inner shareable */
#define DESC_AF (1UL << 10)                /* [10] = access flag */

/* MAIR attribute indices + values. */
#define ATTR_NORMAL_IDX 0
#define ATTR_DEVICE_IDX 1
#define MAIR_NORMAL 0xFFUL /* Normal, inner/outer WB non-transient */
#define MAIR_DEVICE 0x04UL /* Device-nGnRE */

/**
 * Build the identity map, program MAIR/TCR/TTBR0_EL1, and enable the MMU (and
 * the I/D caches).  Identity-mapped, so the PC/SP keep working across the
 * enable — no relocation needed.
 */
void aarch64_mmu_init(void)
{
	for (unsigned i = 0; i < 512; i++)
	{
		u_int64_t pa = (u_int64_t)i * MMU_1GB;
		u_int64_t attr;

		if (i == 0)
			attr = DESC_ATTR(ATTR_DEVICE_IDX); /* peripherals: no shareability */
		else
			attr = DESC_ATTR(ATTR_NORMAL_IDX) | DESC_SH_INNER; /* RAM */

		l1_table[i] = pa | attr | DESC_AF | DESC_BLOCK;
	}

	/* MAIR_EL1: attr0 = Normal WB, attr1 = Device-nGnRE. */
	u_int64_t mair = (MAIR_NORMAL << (8 * ATTR_NORMAL_IDX)) | (MAIR_DEVICE << (8 * ATTR_DEVICE_IDX));
	__asm__ volatile("msr mair_el1, %0" : : "r"(mair));

	/*
	 * TCR_EL1: T0SZ=25 (39-bit VA → start at level 1), TG0=4KB, inner-shareable,
	 * WB cacheable table walks, EPD1=1 (TTBR1 unused for now), IPS=40-bit PA.
	 */
	u_int64_t tcr = (25UL << 0)    /* T0SZ */
	                | (1UL << 8)   /* IRGN0 = WB */
	                | (1UL << 10)  /* ORGN0 = WB */
	                | (3UL << 12)  /* SH0   = inner */
	                | (0UL << 14)  /* TG0   = 4KB */
	                | (1UL << 23)  /* EPD1  = disable TTBR1 walks */
	                | (2UL << 32); /* IPS   = 40-bit */
	__asm__ volatile("msr tcr_el1, %0" : : "r"(tcr));

	__asm__ volatile("msr ttbr0_el1, %0" : : "r"((u_int64_t)(uintptr_t)l1_table));

	__asm__ volatile("isb");
	__asm__ volatile("tlbi vmalle1; dsb nsh; isb");

	u_int64_t sctlr;
	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
	sctlr |= (1UL << 0)     /* M — MMU enable */
	         | (1UL << 2)   /* C — data/unified cache */
	         | (1UL << 12); /* I — instruction cache */
	__asm__ volatile("msr sctlr_el1, %0; isb" : : "r"(sctlr));
}
