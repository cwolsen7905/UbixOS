/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Raspberry Pi 3 (BCM2837) - M0.75 EL1 exception vector table.
 *
 * 16 entries, 0x80 bytes each, 0x800 (2 KB) aligned (VBAR_EL1 requirement).
 * Only "Current EL with SPx -> IRQ" is wired to a real handler (the timer IRQ we
 * take while sitting in WFI at EL1h); every other vector reports a bad exception
 * and halts.  See timer_init()/irq_handler() in hello.c.
 */

	.macro VENTRY label
	.align 7			/* 0x80-byte slot */
	b	\label
	.endm

	.section .text
	.align 11			/* 0x800-aligned table base */
	.globl vectors
vectors:
	/* Current EL with SP0 */
	VENTRY bad_exc			/* synchronous */
	VENTRY bad_exc			/* IRQ        */
	VENTRY bad_exc			/* FIQ        */
	VENTRY bad_exc			/* SError     */
	/* Current EL with SPx */
	VENTRY bad_exc			/* synchronous */
	VENTRY el1_irq			/* IRQ  <-- timer */
	VENTRY bad_exc			/* FIQ        */
	VENTRY bad_exc			/* SError     */
	/* Lower EL using AArch64 */
	VENTRY bad_exc
	VENTRY bad_exc
	VENTRY bad_exc
	VENTRY bad_exc
	/* Lower EL using AArch32 */
	VENTRY bad_exc
	VENTRY bad_exc
	VENTRY bad_exc
	VENTRY bad_exc

el1_irq:
	stp	x0, x1, [sp, #-16]!
	stp	x2, x3, [sp, #-16]!
	stp	x4, x5, [sp, #-16]!
	stp	x6, x7, [sp, #-16]!
	stp	x8, x9, [sp, #-16]!
	stp	x10, x11, [sp, #-16]!
	stp	x12, x13, [sp, #-16]!
	stp	x14, x15, [sp, #-16]!
	stp	x16, x17, [sp, #-16]!
	stp	x18, x30, [sp, #-16]!
	bl	irq_handler
	ldp	x18, x30, [sp], #16
	ldp	x16, x17, [sp], #16
	ldp	x14, x15, [sp], #16
	ldp	x12, x13, [sp], #16
	ldp	x10, x11, [sp], #16
	ldp	x8, x9, [sp], #16
	ldp	x6, x7, [sp], #16
	ldp	x4, x5, [sp], #16
	ldp	x2, x3, [sp], #16
	ldp	x0, x1, [sp], #16
	eret

bad_exc:
	bl	bad_exception
1:	wfe
	b	1b
