/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 SMP secondary-core entry (smp-plan M1).
 *
 * PSCI CPU_ON lands an application processor here with the MMU off, caches off,
 * at EL1, interrupts (firmware-)masked.  PSCI passes the context_id we handed it
 * in x0 — we use the AP's logical cpu id.
 *
 * We must NOT read any data the BSP wrote with caches on while our MMU is off
 * (it may sit in the BSP's cache, not RAM).  So the only inputs here are x0 and
 * link-time SYMBOL ADDRESSES (literals): the per-CPU stack is carved from the
 * g_ap_stack[] symbol by arithmetic on id, no memory load of a BSP-written
 * pointer.  c_ap_boot_arm() then enables the MMU (via the l1_table symbol
 * address + constants) before it touches any shared state.
 */

#define AP_STACK_SIZE 16384  /* must match g_ap_stack[][] in apsmp.c */

	.section .text.boot, "ax"
	.global secondary_entry
	.type secondary_entry, %function
secondary_entry:
	msr	daifset, #0xf		/* mask all interrupts */
	mov	x19, x0			/* x19 = context_id = logical cpu id */

	/* sp = &g_ap_stack + (id + 1) * AP_STACK_SIZE  (top of this AP's slot). */
	ldr	x1, =g_ap_stack
	add	x2, x19, #1
	mov	x3, #AP_STACK_SIZE
	mul	x2, x2, x3
	add	x1, x1, x2
	mov	sp, x1

	mov	x0, x19			/* arg0 = cpu id */
	bl	c_ap_boot_arm

	/* c_ap_boot_arm() should not return; park if it does. */
1:	wfi
	b	1b
	.size secondary_entry, . - secondary_entry
