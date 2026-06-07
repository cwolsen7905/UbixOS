/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 physical-allocator demo (QEMU `virt` bring-up).
 *
 * Proves the arch-neutral page bitmap allocator (sys/vmm/vmm_memory.c) runs on
 * aarch64 atop the md layout in vmm_machdep.c: initialise the bitmap, allocate a
 * few real RAM frames, read/write one (they are identity-mapped, so the physical
 * address is directly usable), free one, and confirm it is handed back out.
 * Throwaway scaffolding.
 */

#include "bringup.h"
#include <vmm/vmm.h>
#include <lib/kmalloc.h> /* sysID */

/**
 * Bring up the physical allocator and exercise it.
 */
void aarch64_vmm_demo(void)
{
	uintptr_t p1, p2, p3, p4;

	kprintf("vmm demo: bringing up the physical page allocator...\n");
	vmm_mem_map_init();

	p1 = vmm_find_free_page(sysID);
	p2 = vmm_find_free_page(sysID);
	p3 = vmm_find_free_page(sysID);
	kprintf("  allocated frames: 0x%lX 0x%lX 0x%lX\n", (u_int64_t)p1, (u_int64_t)p2, (u_int64_t)p3);

	/* All RAM is identity-mapped, so the physical address is a usable pointer. */
	*(volatile u_int64_t *)p1 = 0xDEADBEEFCAFEUL;
	kprintf("  frame 0x%lX holds 0x%lX (read/write ok)\n", (u_int64_t)p1, *(volatile u_int64_t *)p1);

	free_page(p2);
	p4 = vmm_find_free_page(sysID);
	kprintf("  freed 0x%lX, next alloc gave 0x%lX (reuse=%s)\n",
	        (u_int64_t)p2,
	        (u_int64_t)p4,
	        (p2 == p4) ? "yes" : "no");

	kprintf("vmm demo: physical allocator works on aarch64.\n");
}
