/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 EL1 exception vector table (QEMU `virt` bring-up).
 *
 * The table is 2 KB-aligned with 16 entries of 0x80 bytes; each entry can hold
 * only a short stub, so every slot just branches to a handler that saves the
 * GPRs, calls the C dispatcher aarch64_exception(kind, frame), and ERETs.
 * We run at EL1 on SP_EL1, so EL1 traps land in the "Current EL with SPx" group
 * (sync @0x200, irq @0x280).  Everything else funnels to an "invalid" dump.
 */

/* Save x0-x30 into a 256-byte frame (16-aligned); SP then points at the frame. */
	.macro KERNEL_ENTRY
	sub	sp, sp, #256
	stp	x0, x1, [sp, #0]
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	stp	x8, x9, [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x16, x17, [sp, #128]
	stp	x18, x19, [sp, #144]
	stp	x20, x21, [sp, #160]
	stp	x22, x23, [sp, #176]
	stp	x24, x25, [sp, #192]
	stp	x26, x27, [sp, #208]
	stp	x28, x29, [sp, #224]
	str	x30, [sp, #240]
	.endm

	.macro KERNEL_EXIT
	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	ldp	x8, x9, [sp, #64]
	ldp	x10, x11, [sp, #80]
	ldp	x12, x13, [sp, #96]
	ldp	x14, x15, [sp, #112]
	ldp	x16, x17, [sp, #128]
	ldp	x18, x19, [sp, #144]
	ldp	x20, x21, [sp, #160]
	ldp	x22, x23, [sp, #176]
	ldp	x24, x25, [sp, #192]
	ldp	x26, x27, [sp, #208]
	ldp	x28, x29, [sp, #224]
	ldr	x30, [sp, #240]
	add	sp, sp, #256
	eret
	.endm

/* One 0x80-aligned table slot: just branch to the real handler. */
	.macro VENTRY label
	.balign 0x80
	b	\label
	.endm

	.section .text
	.balign 0x800
	.global vectors_el1
vectors_el1:
	VENTRY	el1_invalid	/* 0x000 Current EL SP0  sync */
	VENTRY	el1_invalid	/* 0x080 Current EL SP0  irq  */
	VENTRY	el1_invalid	/* 0x100 Current EL SP0  fiq  */
	VENTRY	el1_invalid	/* 0x180 Current EL SP0  serr */
	VENTRY	el1_sync	/* 0x200 Current EL SPx  sync */
	VENTRY	el1_irq		/* 0x280 Current EL SPx  irq  */
	VENTRY	el1_invalid	/* 0x300 Current EL SPx  fiq  */
	VENTRY	el1_invalid	/* 0x380 Current EL SPx  serr */
	VENTRY	el0_sync	/* 0x400 Lower EL aarch64 sync (SVC syscalls) */
	VENTRY	el1_invalid	/* 0x480 Lower EL aarch64 irq  */
	VENTRY	el1_invalid	/* 0x500 Lower EL aarch64 fiq  */
	VENTRY	el1_invalid	/* 0x580 Lower EL aarch64 serr */
	VENTRY	el1_invalid	/* 0x600 Lower EL aarch32 sync */
	VENTRY	el1_invalid	/* 0x680 Lower EL aarch32 irq  */
	VENTRY	el1_invalid	/* 0x700 Lower EL aarch32 fiq  */
	VENTRY	el1_invalid	/* 0x780 Lower EL aarch32 serr */

el1_sync:
	KERNEL_ENTRY
	mov	x0, #1		/* kind = sync */
	mov	x1, sp
	bl	aarch64_exception
	KERNEL_EXIT

el1_irq:
	KERNEL_ENTRY
	mov	x0, #2		/* kind = irq */
	mov	x1, sp
	bl	aarch64_exception
	KERNEL_EXIT

el0_sync:
	KERNEL_ENTRY
	mov	x0, #3		/* kind = EL0 sync (SVC) */
	mov	x1, sp
	bl	aarch64_exception
	KERNEL_EXIT

el1_invalid:
	KERNEL_ENTRY
	mov	x0, #0		/* kind = invalid */
	mov	x1, sp
	bl	aarch64_exception
	KERNEL_EXIT
