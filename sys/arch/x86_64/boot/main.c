/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 bring-up C entry.  Reached from start.S in 64-bit long mode with paging
 * on (identity map of the low 1 GB).  Phase 1: COM1 banner (proof the long-mode
 * transition + 64-bit C ABI work).  Phase 2: install the IDT so CPU faults are
 * visible instead of a silent triple-fault.  The rest of the kernel grows from
 * here (paging allocator, then the i386 drivers widened to 64-bit).
 */

#include "x86_64.h"

/**
 * x86-64 kernel C entry.  @mb_magic / @mb_info are the boot magic + info pointer
 * start.S preserved (zero-extended) in the SysV arg registers.
 */
void kmain_x86_64(u32 mb_magic, u32 mb_info)
{
	serial_init();
	serial_puts("\nuBixOS x86_64 (long mode) - boot OK\n");
	serial_puts("x86_64 bring-up: COM1 up, PAE+LME+paging on, 64-bit C ABI live.\n");
	serial_puts("  boot magic=");
	serial_puthex(mb_magic);
	serial_puts(" info=");
	serial_puthex(mb_info);
	serial_puts("\n");

	idt_init();
	serial_puts("IDT installed: 256 gates, 32 CPU-exception handlers (faults now visible).\n");

	serial_puts("x86_64 Phase 2 up. Idle.\n");
	for (;;)
		__asm__ __volatile__("hlt");
}
