/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Raspberry Pi 3 Model B v1.2 (BCM2837) - M0.5 bring-up entry.
 *
 * The VideoCore firmware loads kernel8.img (raw flat binary) at 0x80000 and
 * enters core 0 at EL2 with x0 = DTB physical address; cores 1-3 sit in a
 * firmware spin-table.  M0.5 gates to core 0, records the entry EL, drops EL2->
 * EL1 (the real kernel runs at EL1), sets a stack, and calls m0_main(dtb, el)
 * which prints the EL + the firmware DTB's /memory map over PL011.
 */
	.section .text.boot, "ax"
	.globl _start
_start:
	mrs	x1, mpidr_el1
	and	x1, x1, #0xff		/* Aff0 = core id within cluster 0 */
	cbnz	x1, park		/* only core 0 runs; park 1-3 */
	mov	x19, x0			/* preserve the DTB pointer */
	mrs	x20, CurrentEL
	lsr	x20, x20, #2		/* x20 = entry EL (2 from the firmware) */

	cmp	x20, #2
	b.ne	at_el1			/* already EL1? skip the drop */

	/* --- drop EL2 -> EL1 --- */
	mrs	x2, cnthctl_el2
	orr	x2, x2, #3		/* EL1PCEN | EL1PCTEN: let EL1 use the phys timer/counter */
	msr	cnthctl_el2, x2
	msr	cntvoff_el2, xzr	/* zero the virtual offset */
	movz	x2, #0x8000, lsl #16	/* HCR_EL2.RW (bit 31) = 1: EL1 is AArch64 */
	msr	hcr_el2, x2
	ldr	x2, =0x30d00800		/* SCTLR_EL1: RES1 bits, MMU/caches off */
	msr	sctlr_el1, x2
	mov	x2, #0x3c5		/* SPSR_EL2: EL1h + DAIF masked */
	msr	spsr_el2, x2
	adr	x2, at_el1
	msr	elr_el2, x2
	eret

at_el1:
	ldr	x1, =_stack_top
	mov	sp, x1
	mov	x0, x19			/* arg0 = DTB pointer */
	mov	x1, x20			/* arg1 = entry EL */
	bl	m0_main
hang:
	wfe
	b	hang
park:
	wfe
	b	park
