/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 machine-dependent physical-memory setup (bring-up Phase 3).  Stages the
 * machine-independent page bitmap (sys/vmm/vmm_memory.c) over RAM and provides
 * the kernel-heap page source kmalloc() needs.  All RAM is identity-mapped by
 * start.S's 1 GB map, so a physical frame is directly usable as a 64-bit pointer
 * (like aarch64; unlike i386's scattered-frame kernel VA).  Sibling of
 * aarch64/vmm/vmm_machdep.c.
 */

#include "../x86_64.h"
#include <vmm/vmm.h>
#include <lib/kmalloc.h> /* sysID */
#include <string.h>

extern char _end[]; /* end of the kernel image (ldscript) */

/*
 * RAM size.  Bring-up: assume QEMU's default `-m 256`.  The low 1 GB is
 * identity-mapped, so 256 MB is well within it.  Parsing the real PVH/e820 memory
 * map (the analogue of aarch64's DTB /memory parse) is a later step.
 */
#define X86_64_RAM_BYTES (256UL * 1024 * 1024)

/**
 * Initialise the physical page allocator: stage the bitmap at _end, mark every
 * frame unavailable, then free RAM above the bitmap.
 */
void x86_64_mem_init(void)
{
	u32 num = (u32)(X86_64_RAM_BYTES / PAGE_SIZE);
	uintptr_t bitmap_phys = ((uintptr_t)_end + PAGE_SIZE - 1) & ~((uintptr_t)PAGE_SIZE - 1);
	u32 bitmap_bytes;
	u32 bitmap_end_page;

	vmm_mem_bitmap_init(bitmap_phys, num);
	bitmap_bytes = num * (u32)sizeof(vmm_page_info_t);
	bitmap_end_page = (u32)((bitmap_phys + bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE);
	vmm_mem_mark_available(bitmap_end_page, num);

	serial_puts("x86_64: phys page allocator up (RAM 256 MB, bitmap @ ");
	serial_puthex(bitmap_phys);
	serial_puts(", free pages ");
	serial_putdec(vmm_mem_free_pages());
	serial_puts(")\n");
}

/**
 * Allocate @count contiguous physical pages for the kernel heap, zeroed.  The
 * identity map makes the frame address a usable kernel pointer directly.
 */
void *vmm_get_free_malloc_page(u_int16_t count)
{
	uintptr_t base = vmm_find_free_pages_contig(count, sysID);

	if (base == 0)
		return 0;
	memset((void *)base, 0, (unsigned long)count * PAGE_SIZE);
	return (void *)base;
}

/** Page-eviction hook — no swap device yet, so eviction always fails. */
int swap_evict_page(void)
{
	return 0;
}
