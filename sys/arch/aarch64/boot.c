/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 bring-up C entry (QEMU `virt`).
 *
 * Phase 11: print a banner over the PL011.  Phase 12a: install the EL1
 * exception vectors so faults are visible.  GIC + generic timer (interrupts)
 * are Phase 12b; MMU is Phase 13.
 */

#include "bringup.h"

/**
 * Return the current exception level (0-3) from CurrentEL[3:2].
 */
static u_int64_t current_el(void)
{
	u_int64_t v;
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
	return (v >> 2) & 0x3;
}

/**
 * First C code on aarch64: banner, EL report, exception vectors.  Returns to the
 * park loop in start.S (Phase 12b will instead enable IRQs and idle on `wfi`).
 */
void kmain_aarch64(void)
{
	kprintf("\nuBixOS aarch64 (QEMU virt) - boot OK\n");
	kprintf("CurrentEL=%lu\n", current_el());

	aarch64_vbar_init();
	kprintf("EL1 exception vectors installed (VBAR_EL1).\n");

	aarch64_mmu_init();
	kprintf("MMU enabled: TTBR0 identity map (39-bit VA), caches on.\n");

	aarch64_vmm_demo();
	aarch64_pmap_demo();
	aarch64_ctx_demo();
	aarch64_sched_demo();

	gic_init();
	timer_init();

	/* Unmask IRQs (clear DAIF.I); timer ticks now arrive via the GIC. */
	__asm__ volatile("msr daifclr, #2");
	kprintf("IRQs enabled; idling on wfi (expect periodic timer ticks).\n");

	for (;;)
		__asm__ volatile("wfi");
}
