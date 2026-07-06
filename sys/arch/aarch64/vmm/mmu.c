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

/* PHYSMAP_BASE (the TTBR1 physmap base / VMA-LMA delta) comes from bringup.h. */

/* Level-1 table: 512 entries, each a 1 GB block.  4 KB aligned (one page). */
static u_int64_t l1_table[512] __attribute__((aligned(4096)));

/* TTBR1 physmap L1: the same 1 GB identity blocks, reached at PHYSMAP_BASE. */
static u_int64_t physmap_l1[512] __attribute__((aligned(4096)));

#ifdef BOARD_RPI3
/* Raspberry Pi 3: physical RAM (0..0x3F000000) and the peripheral window
 * (0x3F000000..0x40000000) share the first 1 GB, so block 0 is split into 2 MB
 * blocks via a shared level-2 table — RAM Normal cacheable (the kernel runs here
 * and needs working atomics), peripherals Device.  Both translation roots' entry 0
 * point at this one L2 (the block-0 identity mapping is the same for each). */
#define MMU_2MB (2UL << 20)
#define DESC_TABLE 0x3UL             /* [1:0] = 11: table descriptor */
#define RPI_PERIPH_BASE 0x3F000000UL /* BCM2837 peripheral window base */
static u_int64_t l2_block0[512] __attribute__((aligned(4096)));
#endif

#ifdef BOARD_RPI4
/* Raspberry Pi 4 / Pi 400 (BCM2711): the peripheral window is up at 0xFE000000 (low-
 * peripheral mode) — in the 3-4 GB block (block 3), alongside RAM below it and the
 * GIC-400 (0xFF84xxxx) above.  So block 3 is split into 2 MB blocks (RAM Normal,
 * peripherals + GIC Device); blocks 0-2 and 4+ are Normal-RAM 1 GB blocks (the kernel
 * loads at 0x80000 in block 0). */
#define MMU_2MB (2UL << 20)
#define DESC_TABLE 0x3UL
#define RPI4_PERIPH_BASE 0xFE000000UL /* BCM2711 low-peripheral window base */
/* The BCM2711 also has MMIO below the 0xFE000000 legacy window: the GENET gigabit
 * MAC (0xFD580000) and the PCIe root complex (0xFD500000).  Map everything from
 * 0xFC000000 up as Device so those register blocks aren't cached as Normal RAM
 * (there is no RAM up here — the Pi's RAM lives in blocks 0-2). */
#define RPI4_DEVICE_BASE 0xFC000000UL
static u_int64_t l2_block3[512] __attribute__((aligned(4096)));
#endif

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
/*
 * Program MAIR/TCR/TTBR0_EL1 to point at the shared kernel l1_table and enable
 * the MMU + caches on the *calling* CPU.  Split out of aarch64_mmu_init so an AP
 * can reuse byte-identical config (aarch64_mmu_enable_secondary) without
 * rebuilding the page tables — the BSP already built l1_table, and its entries
 * were written with the MMU off (straight to RAM), so an AP's table walk reads
 * them coherently.
 */
static void mmu_program_regs(void)
{
	/* MAIR_EL1: attr0 = Normal WB, attr1 = Device-nGnRE. */
	u_int64_t mair = (MAIR_NORMAL << (8 * ATTR_NORMAL_IDX)) | (MAIR_DEVICE << (8 * ATTR_DEVICE_IDX));
	__asm__ volatile("msr mair_el1, %0" : : "r"(mair));

	/*
	 * TCR_EL1: 39-bit VAs both halves (T0SZ=T1SZ=25, level-1 start), 4 KB granule
	 * (TG0=0b00, TG1=0b10), inner-shareable WB cacheable table walks, IPS=40-bit PA.
	 * TTBR1 walks are ENABLED (EPD1=0) — the kernel physmap at PHYSMAP_BASE.  Higher-
	 * half Phase 1: TTBR0 low identity stays up too, so behaviour is unchanged.
	 */
	u_int64_t tcr = (25UL << 0)    /* T0SZ  = 39-bit (TTBR0, user/low) */
	                | (1UL << 8)   /* IRGN0 = WB */
	                | (1UL << 10)  /* ORGN0 = WB */
	                | (3UL << 12)  /* SH0   = inner */
	                | (0UL << 14)  /* TG0   = 4KB */
	                | (25UL << 16) /* T1SZ  = 39-bit (TTBR1, kernel physmap) */
	                | (1UL << 24)  /* IRGN1 = WB */
	                | (1UL << 26)  /* ORGN1 = WB */
	                | (3UL << 28)  /* SH1   = inner */
	                | (2UL << 30)  /* TG1   = 4KB */
	                | (2UL << 32); /* IPS   = 40-bit */
	__asm__ volatile("msr tcr_el1, %0" : : "r"(tcr));

	__asm__ volatile("msr ttbr0_el1, %0" : : "r"((u_int64_t)(uintptr_t)l1_table));
	__asm__ volatile("msr ttbr1_el1, %0" : : "r"((u_int64_t)(uintptr_t)physmap_l1));

	__asm__ volatile("isb");
	__asm__ volatile("tlbi vmalle1; dsb nsh; isb");

	/* Allow FP/SIMD at EL0 and EL1 (CPACR_EL1.FPEN = 0b11, no trap).  The kernel
	 * itself is built -mgeneral-regs-only, but userland (musl) uses FP, so EL0
	 * must not trap it. */
	__asm__ volatile("msr cpacr_el1, %0; isb" : : "r"((u_int64_t)(3UL << 20)));

	u_int64_t sctlr;
	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
	sctlr |= (1UL << 0)     /* M — MMU enable */
	         | (1UL << 2)   /* C — data/unified cache */
	         | (1UL << 12); /* I — instruction cache */
	__asm__ volatile("msr sctlr_el1, %0; isb" : : "r"(sctlr));
}

/*
 * Build the identity map (BSP only), then program the registers + enable the MMU.
 */
void aarch64_mmu_init(void)
{
#if defined(BOARD_RPI4)
	/* Block 3 (0xC0000000): 2 MB granularity — Normal below 0xFC000000 (unused, no
	 * RAM), Device at and above it (GENET 0xFD580000, PCIe 0xFD500000, the legacy
	 * peripheral window 0xFE000000, and the GIC-400 0xFF84xxxx). */
	for (unsigned j = 0; j < 512; j++)
	{
		u_int64_t pa = 0xC0000000UL + (u_int64_t)j * MMU_2MB;
		u_int64_t attr =
		    (pa < RPI4_DEVICE_BASE) ? (DESC_ATTR(ATTR_NORMAL_IDX) | DESC_SH_INNER) : DESC_ATTR(ATTR_DEVICE_IDX);
		l2_block3[j] = pa | attr | DESC_AF | DESC_BLOCK;
	}
	/* Blocks 0-2 + 4..511: Normal-RAM 1 GB blocks (the kernel loads in block 0); block
	 * 3 points at the split L2 above. */
	for (unsigned i = 0; i < 512; i++)
	{
		u_int64_t blk;
		if (i == 3)
			blk = (u_int64_t)(uintptr_t)l2_block3 | DESC_TABLE;
		else
			blk = ((u_int64_t)i * MMU_1GB) | DESC_ATTR(ATTR_NORMAL_IDX) | DESC_SH_INNER | DESC_AF | DESC_BLOCK;
		l1_table[i] = blk;
		physmap_l1[i] = blk;
	}
#elif defined(BOARD_RPI3)
	/* Block 0: 2 MB granularity — RAM Normal up to the peripheral window, Device
	 * above.  (uintptr_t)l2_block0 resolves to its low PA here (adrp-relative, MMU
	 * off), which is what the table descriptor needs. */
	for (unsigned j = 0; j < 512; j++)
	{
		u_int64_t pa = (u_int64_t)j * MMU_2MB;
		u_int64_t attr =
		    (pa < RPI_PERIPH_BASE) ? (DESC_ATTR(ATTR_NORMAL_IDX) | DESC_SH_INNER) : DESC_ATTR(ATTR_DEVICE_IDX);
		l2_block0[j] = pa | attr | DESC_AF | DESC_BLOCK;
	}
	l1_table[0] = (u_int64_t)(uintptr_t)l2_block0 | DESC_TABLE;
	physmap_l1[0] = (u_int64_t)(uintptr_t)l2_block0 | DESC_TABLE;
	/* Blocks 1+ : Device 1 GB (the BCM "ARM local" block @0x40000000 + unused;
	 * the Pi has no RAM above 1 GB). */
	for (unsigned i = 1; i < 512; i++)
	{
		u_int64_t blk = ((u_int64_t)i * MMU_1GB) | DESC_ATTR(ATTR_DEVICE_IDX) | DESC_AF | DESC_BLOCK;
		l1_table[i] = blk;
		physmap_l1[i] = blk;
	}
#else
	for (unsigned i = 0; i < 512; i++)
	{
		u_int64_t pa = (u_int64_t)i * MMU_1GB;
		u_int64_t attr;

		if (i == 0)
			attr = DESC_ATTR(ATTR_DEVICE_IDX); /* peripherals: no shareability */
		else
			attr = DESC_ATTR(ATTR_NORMAL_IDX) | DESC_SH_INNER; /* RAM */

		l1_table[i] = pa | attr | DESC_AF | DESC_BLOCK;
		physmap_l1[i] = pa | attr | DESC_AF | DESC_BLOCK; /* same blocks, reached at PHYSMAP_BASE */
	}
#endif

	mmu_program_regs();
}

/*
 * smp-plan M1: bring an application processor's MMU up on the page tables the BSP
 * already built (l1_table).  No table build — just the register program + enable.
 */
void aarch64_mmu_enable_secondary(void)
{
	mmu_program_regs();
}

/**
 * Higher-half Phase 1 sanity check: confirm the TTBR1 physmap aliases the low
 * identity.  Reads a known physical location (the kernel base) through its low VA
 * (== phys) and through PHYSMAP_BASE + phys and reports whether they agree.
 * Temporary scaffolding — removed once the kernel runs from the physmap.
 */
void aarch64_physmap_verify(void)
{
	volatile u_int32_t *lo = (volatile u_int32_t *)(uintptr_t)0x40200000UL;
	volatile u_int32_t *hi = (volatile u_int32_t *)(PHYSMAP_BASE + 0x40200000UL);

	kprintf("physmap: lo[0x40200000]=0x%X hi[PHYSMAP_BASE+0x40200000]=0x%X -> %s\n",
	        *lo,
	        *hi,
	        (*lo == *hi) ? "MATCH" : "MISMATCH");
}
