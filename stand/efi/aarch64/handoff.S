/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 hand-off trampoline.  UEFI leaves the CPU with the MMU and caches on;
 * the kernel's physical entry (_start, start.S) expects the MMU off with x0 =
 * DTB.  This block is position independent: md.c copies it to a page below 4 GB
 * (identity mapped) and calls it as  tramp(x0 = bootinfo, x1 = entry, x2 = dtb).
 * It cleans the kernel image and itself to the point of coherency, turns the
 * MMU and caches off at the current EL, and enters the kernel with x0 = DTB,
 * x1 = bootinfo, x2 = UBIX_BOOTINFO_MAGIC.
 */

	.text
	.globl	ubix_tramp_start
	.globl	ubix_tramp_end

	/* struct ubix_bootinfo: kernel_start @88, kernel_end @96 */
	.equ	BI_KSTART, 88
	.equ	BI_KEND, 96

ubix_tramp_start:
	msr	daifset, #0xf
	mov	x19, x0			/* bootinfo */
	mov	x20, x1			/* entry */
	mov	x21, x2			/* dtb */

	/* D-cache line size from CTR_EL0.DminLine (log2 words). */
	mrs	x3, ctr_el0
	ubfx	x3, x3, #16, #4
	mov	x4, #4
	lsl	x4, x4, x3		/* x4 = line size in bytes */
	sub	x5, x4, #1

	/* Clean+invalidate the kernel image [kernel_start, kernel_end). */
	ldr	x0, [x19, #BI_KSTART]
	ldr	x1, [x19, #BI_KEND]
	bic	x0, x0, x5
1:	dc	civac, x0
	add	x0, x0, x4
	cmp	x0, x1
	b.lo	1b

	/* Clean+invalidate the boot-info block and this trampoline. */
	mov	x0, x19
	add	x1, x19, #4096
	bic	x0, x0, x5
2:	dc	civac, x0
	add	x0, x0, x4
	cmp	x0, x1
	b.lo	2b
	adr	x0, ubix_tramp_start
	adr	x1, ubix_tramp_end
	bic	x0, x0, x5
3:	dc	civac, x0
	add	x0, x0, x4
	cmp	x0, x1
	b.lo	3b
	dsb	sy
	ic	iallu
	dsb	sy
	isb

	/* MMU, D-cache and I-cache off at the current exception level. */
	mrs	x0, CurrentEL
	lsr	x0, x0, #2
	mov	x1, #0x1005		/* M | C | I */
	cmp	x0, #2
	b.eq	4f
	mrs	x0, sctlr_el1
	bic	x0, x0, x1
	msr	sctlr_el1, x0
	isb
	tlbi	vmalle1
	b	5f
4:	mrs	x0, sctlr_el2
	bic	x0, x0, x1
	msr	sctlr_el2, x0
	isb
	tlbi	alle2
5:	dsb	sy
	isb

	mov	x0, x21			/* DTB physical address */
	mov	x1, x19			/* boot-info */
	movz	x2, #0x4958		/* UBIX_BOOTINFO_MAGIC 0x55424958 */
	movk	x2, #0x5542, lsl #16
	br	x20
	.balign 8
ubix_tramp_end:
