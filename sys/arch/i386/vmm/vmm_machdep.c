/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * i386 machine-dependent physical-memory detection and page-bitmap layout.
 *
 * The arch-neutral bitmap allocator lives in sys/vmm/vmm_memory.c
 * (vmm_find_free_page / free_page / COW / share / audit).  This file supplies
 * the two machine-dependent pieces it needs — count_memory() (probe installed
 * RAM via the legacy CR0-cache-disable walk) and vmm_mem_map_init() (lay the
 * bitmap over the i386 physical map: 1 MB ISA/VGA/BIOS hole, kernel at
 * 0x300000).  The aarch64 counterparts live in sys/arch/aarch64/vmm_machdep.c.
 */

#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <sys/io.h>
#include <lib/kprintf.h>
#include <ubixos/vitals.h>
#include <machine/cpu.h>

/* Linker symbols bracketing the kernel image. */
extern char _start[]; // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
extern char _end[];   // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)

/************************************************************************

 Function: int count_memory();
 Description: This Function Counts The Systems Physical Memory
 Notes:

 02/20/2004 - Inspect For Quality And Approved

 ************************************************************************/
u_int32_t count_memory()
{
	register u_int32_t *mem = NULL;
	unsigned long mem_count = -1, temp_memory = 0;
	unsigned short mem_kb = 8;
	unsigned char irq1_state, irq2_state;
	unsigned long cr0 = 0;

	/*
	 * Save The States Of Both IRQ 1 And 2 So We Can Turn Them Off And Restore
	 * Them Later
	 */
	irq1_state = inportByte(0x21);
	irq2_state = inportByte(0xA1);

	/* Turn Off IRQ 1 And 2 To Prevent Chances Of Faults While Examining Memory */
	outportByte(0x21, 0xFF);
	outportByte(0xA1, 0xFF);

	/* Save The State Of Register CR0 */
	cr0 = rcr0();

	asm volatile("wbinvd");

	load_cr0(cr0 | 0x00000001 | 0x40000000 | 0x20000000);

	while (mem_kb < 4096 && mem_count != 0)
	{
		mem_kb++;

		if (mem_count == -1)
		{
			mem_count = 8388608;
		}
		else
		{
			mem_count += 1024 * 1024;
		}

		mem = (u_int32_t *)mem_count;

		temp_memory = *mem; // NOLINT(clang-analyzer-core.FixedAddressDereference)

		*mem = 0x55AA55AA;

		asm("" : : : "memory");

		if (*mem != 0x55AA55AA)
		{
			mem_count = 0;
		}
		else
		{
			*mem = 0xAA55AA55;
			asm("" : : : "memory");
			if (*mem != 0xAA55AA55)
			{
				mem_count = 0;
			}
		}
		asm("" : : : "memory");
		*mem = temp_memory;
	}

	asm("nop");

	// MrOlsen (2016-01-10) NOTE: I don't like this but I start incrementing form the start.
	mem_kb--;

	asm("nop");

	load_cr0(cr0);

	asm("nop");

	/* Restore States For Both IRQ 1 And 2 */
	outportByte(0x21, irq1_state);
	outportByte(0xA1, irq2_state);

	asm("nop");

	/* Return Amount Of Memory In Pages */
	return ((mem_kb * 1024 * 1024) / PAGE_SIZE);
}

/************************************************************************

 Function: void vmm_mem_map_init();
 Description: This Function Initializes The Memory Map For the System
 Notes:

 02/20/2004 - Made It Report Real And Available Memory

 ************************************************************************/
int vmm_mem_map_init()
{
	uintptr_t bitmap_phys;
	u_int32_t bitmap_size, bitmap_end_page, kernel_start_page;
	u_int32_t num = count_memory();

	/*
	 * Place the page bitmap immediately after the kernel image (page-aligned).
	 * This makes the layout RAM-size-independent: with N pages the bitmap is
	 * N*sizeof(vmm_page_info_t) bytes, and free pages begin right after it
	 * regardless of how much RAM is installed.
	 */
	bitmap_phys = ((uintptr_t)_end + PAGE_SIZE - 1) & ~((uintptr_t)PAGE_SIZE - 1);
	vmm_mem_bitmap_init(bitmap_phys, num);

	bitmap_size = num * sizeof(vmm_page_info_t);
	bitmap_end_page = (u_int32_t)((bitmap_phys + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE);

	/*
	 * Free page ranges:
	 *  [0x100000, kernel_start): RAM below the kernel (former bitmap staging area)
	 *  [bitmap_end, numPages*PAGE_SIZE): all RAM above the bitmap
	 *
	 * The first 1 MB (0x0–0xFFFFF) stays reserved: ISA devices, VGA, BIOS.
	 */
	kernel_start_page = ((u_int32_t)_start & ~(PAGE_SIZE - 1)) / PAGE_SIZE;
	vmm_mem_mark_available(0x100, kernel_start_page);
	vmm_mem_mark_available(bitmap_end_page, num);

	/* Print Out Amount Of Memory */
	kprintf("Real Memory:      %uKB\n", num * 4);
	kprintf("Available Memory: %uKB\n", vmm_mem_free_pages() * 4);
	kprintf("vmm: bitmap phys=0x%X pages=%u end_page=%u\n", vmm_bitmap_phys, numPages, bitmap_end_page);

	return (0);
}
