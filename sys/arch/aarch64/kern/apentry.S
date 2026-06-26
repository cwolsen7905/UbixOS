/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 SMP secondary-core entry (smp-plan M1).
 *
 * PSCI CPU_ON lands an application processor here with the MMU off, caches off,
 * at EL1, interrupts (firmware-)masked.  PSCI passes the context_id we handed it
 * in x0 — the AP's logical cpu id.
 *
 * We must NOT read any data the BSP wrote with caches on while our MMU is off
 * (it may sit in the BSP's cache, not RAM).  So the only inputs here are x0 and
 * link-time SYMBOL ADDRESSES (literals).
 *
 * HIGHER-HALF (docs/design/aarch64-higher-half-plan.md): like the BSP _start,
 * this stub runs at the low physical load address with the MMU off, so a high-
 * linked symbol's physical address is its VMA minus VIRT_OFFSET.  It enables the
 * MMU on the BSP's tables, then branches to the high half before touching any
 * high-VA C.  VIRT_OFFSET is 0 until the high flip.
 */

#define AP_STACK_SIZE 16384  /* must match g_ap_stack[][] in apsmp.c */

	.section .text.boot, "ax"
	.global secondary_entry
	.type secondary_entry, %function
secondary_entry:
	msr	daifset, #0xf		/* mask all interrupts */
	mov	x19, x0			/* x19 = context_id = logical cpu id */

	/* VMA->LMA delta (0 until the higher-half flip). */
	ldr	x20, =VIRT_OFFSET

	/* Temporary PHYSICAL stack: phys(&g_ap_stack) + (id + 1) * AP_STACK_SIZE. */
	ldr	x1, =g_ap_stack
	sub	x1, x1, x20		/* physical g_ap_stack base */
	add	x2, x19, #1
	mov	x3, #AP_STACK_SIZE
	mul	x2, x2, x3
	add	x1, x1, x2
	mov	sp, x1

	/* Enable the MMU + caches on the BSP's tables (l1_table/physmap_l1), at the
	 * physical address of the helper.  After this both TTBRs are live. */
	ldr	x0, =aarch64_mmu_enable_secondary
	sub	x0, x0, x20
	blr	x0

	/* Branch to the high half — the PC is now a high VMA via the TTBR1 physmap. */
	ldr	x0, =secondary_entry_high
	br	x0
	/* Keep this stub's literal pool in .text.boot so the MMU-off code can reach it. */
	.ltorg
	.size secondary_entry, . - secondary_entry

	/* High-half continuation: linked + executed at the high VMA. */
	.section .text, "ax"
	.type secondary_entry_high, %function
secondary_entry_high:
	/* Switch to the high-VA per-CPU stack, then enter the C bring-up. */
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
	.size secondary_entry_high, . - secondary_entry_high
