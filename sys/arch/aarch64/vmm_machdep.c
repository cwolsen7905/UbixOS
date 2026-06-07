/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 machine-dependent physical-memory detection and page-bitmap layout
 * for the QEMU `virt` platform.
 *
 * The arch-neutral bitmap allocator lives in sys/vmm/vmm_memory.c
 * (vmm_find_free_page / free_page / COW / share / audit); this file supplies the
 * two machine-dependent pieces it needs — count_memory() and
 * vmm_mem_map_init() — the aarch64 counterparts of the i386 versions guarded out
 * of vmm_memory.c.
 *
 * QEMU `virt` lays RAM contiguously at 0x40000000 (no low ISA/VGA/BIOS hole).
 * The kernel is linked at the RAM base, so free RAM is simply everything above
 * the page bitmap (which is staged at _end).  The page-index convention is the
 * same as i386: physical address = index * PAGE_SIZE, so RAM pages start at
 * index 0x40000 and the low indices below the RAM base stay permanently
 * unavailable.  All RAM falls inside the 1 GB identity block the MMU maps
 * (mmu.c), so a returned physical page address is directly usable as a kernel
 * pointer — no separate mapping step for the kernel heap.
 */

#include "bringup.h"
#include <vmm/vmm.h>
#include <vmm/paging.h>

/* QEMU `virt` physical RAM window.  -m 512 is what the bring-up runs use; DTB
 * parsing for the real size is a later refinement. */
#define AARCH64_RAM_BASE 0x40000000UL
#define AARCH64_RAM_SIZE (512UL * 1024 * 1024)
#define AARCH64_RAM_TOP (AARCH64_RAM_BASE + AARCH64_RAM_SIZE)

extern char _end[]; /* end of the kernel image (ldscript.aarch64), page-aligned */

/**
 * Report total physical page count, using the i386 index convention (page
 * index = phys / PAGE_SIZE), so the count spans index 0 .. top-of-RAM.
 *
 * @return number of page-frame slots the bitmap must cover.
 */
u_int32_t count_memory(void)
{
	return (u_int32_t)(AARCH64_RAM_TOP / PAGE_SIZE);
}

/**
 * Initialise the physical page bitmap for QEMU `virt`.
 *
 * Stages the bitmap at _end (page-aligned), marks every frame unavailable, then
 * frees the RAM above the bitmap.  The kernel image + bitmap (RAM base .. bitmap
 * end) and everything below the RAM base stay reserved.
 *
 * @return 0 on success.
 */
int vmm_mem_map_init(void)
{
	uintptr_t bitmap_phys;
	u_int32_t bitmap_size, bitmap_end_page;
	u_int32_t num = count_memory();

	bitmap_phys = ((uintptr_t)_end + PAGE_SIZE - 1) & ~((uintptr_t)PAGE_SIZE - 1);
	vmm_mem_bitmap_init(bitmap_phys, num);

	bitmap_size = num * sizeof(vmm_page_info_t);
	bitmap_end_page = (u_int32_t)((bitmap_phys + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE);

	/* Free RAM = everything from the end of the bitmap to the top of RAM.  The
	 * kernel + bitmap (RAM base .. bitmap end) and everything below the RAM base
	 * stay reserved. */
	vmm_mem_mark_available(bitmap_end_page, num);

	kprintf("vmm(aarch64): RAM 0x%lX..0x%lX, bitmap phys=0x%lX end_page=%u pages=%u free=%u\n",
	        (u_int64_t)AARCH64_RAM_BASE,
	        (u_int64_t)AARCH64_RAM_TOP,
	        (u_int64_t)bitmap_phys,
	        bitmap_end_page,
	        num,
	        vmm_mem_free_pages());

	return 0;
}

/**
 * Page-eviction hook — no swap device on aarch64 yet, so eviction always fails.
 *
 * @return 0 (no page evicted).
 */
int swap_evict_page(void)
{
	return 0;
}
