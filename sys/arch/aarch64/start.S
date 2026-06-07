/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 kernel entry (QEMU `virt` bring-up, Phase 11).
 *
 * QEMU enters here (the ELF entry) on the boot CPU with the MMU off, caches
 * off, at EL1 (or EL2 with virtualization=on — this stub is EL-agnostic: it
 * touches only SP, DAIF, and BSS, which are valid at either EL).  We set up a
 * stack, zero BSS, and branch to the C bring-up (kmain_aarch64).  No multiboot,
 * no GDT — that is all i386.
 */

	.section .text.boot, "ax"
	.global _start
	.type _start, %function
_start:
	/* Mask all interrupts until we have handlers (Phase 12). */
	msr	daifset, #0xf

	/* Initial stack: top of the 16 KB region the linker reserved. */
	ldr	x0, =_stack_top
	mov	sp, x0

	/* Zero BSS [__bss_start, __bss_end) in 8-byte strides (both are
	 * 16-byte aligned by the linker script, so the range is a multiple of 8). */
	ldr	x0, =__bss_start
	ldr	x1, =__bss_end
1:	cmp	x0, x1
	b.hs	2f
	str	xzr, [x0], #8
	b	1b

2:	bl	kmain_aarch64

	/* kmain_aarch64 should not return; if it does, park the CPU. */
3:	wfi
	b	3b
	.size _start, . - _start
