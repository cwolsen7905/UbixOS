/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 kernel entry (QEMU `virt`).
 *
 * QEMU enters here (the ELF entry) on the boot CPU with the MMU off, caches off,
 * at EL1 (or EL2 with virtualization=on — this stub is EL-agnostic).
 *
 * HIGHER-HALF MIGRATION (docs/design/aarch64-higher-half-plan.md): the kernel is
 * linked to EXECUTE at high VAs but LOADED low (ldscript.aarch64 VIRT_OFFSET).
 * This `.text.boot` stub runs at the low physical load address with the MMU off,
 * so it is position-correct: a high-linked symbol's physical address is its VMA
 * minus VIRT_OFFSET.  It zeroes BSS, enables the MMU (TTBR0 identity + TTBR1
 * physmap), then branches to the high half (_start_high), after which everything
 * runs at the high VMA through the TTBR1 physmap.  VIRT_OFFSET is 0 until the
 * high flip, so for now this is a plain low boot.
 */

	.section .text.boot, "ax"
	.global _start
	.type _start, %function
_start:
	mov	x19, x0			/* DTB physical address — preserved across the C call */

	/* If the firmware entered at EL2 (e.g. the Raspberry Pi), drop to EL1 — the
	 * kernel runs at EL1.  QEMU virt enters at EL1, so this is skipped there and
	 * the boot is unchanged.  Mirrors the HW-confirmed M0.5 drop. */
	mrs	x0, CurrentEL
	lsr	x0, x0, #2
	cmp	x0, #2
	b.ne	.Lat_el1
	mrs	x0, cnthctl_el2
	orr	x0, x0, #3		/* EL1PCEN | EL1PCTEN: EL1 may use the phys timer/counter */
	msr	cnthctl_el2, x0
	msr	cntvoff_el2, xzr	/* zero the virtual-timer offset */
	movz	x0, #0x8000, lsl #16	/* HCR_EL2.RW = 1: EL1 is AArch64 */
	msr	hcr_el2, x0
	ldr	x0, =0x30d00800		/* SCTLR_EL1: RES1 bits, MMU/caches off */
	msr	sctlr_el1, x0
	mov	x0, #0x3c5		/* SPSR_EL2: EL1h + DAIF masked */
	msr	spsr_el2, x0
	adr	x0, .Lat_el1
	msr	elr_el2, x0
	eret
.Lat_el1:

	/* Mask all interrupts until we have handlers. */
	msr	daifset, #0xf

	/* VMA->LMA delta: a high-linked symbol's physical address is (VMA - x20).
	 * VIRT_OFFSET is 0 until the higher-half flip, so this is identity for now. */
	ldr	x20, =VIRT_OFFSET

	/* Temporary physical stack for the MMU-enable C call. */
	ldr	x0, =_stack_top
	sub	sp, x0, x20

	/* Zero BSS [__bss_start, __bss_end) at its PHYSICAL addresses (MMU still off).
	 * Must precede the MMU build: the page tables (l1_table/physmap_l1) live in
	 * BSS, so zeroing after the build would wipe the live translation tables. */
	ldr	x0, =__bss_start
	sub	x0, x0, x20
	ldr	x1, =__bss_end
	sub	x1, x1, x20
1:	cmp	x0, x1
	b.hs	2f
	str	xzr, [x0], #8
	b	1b

	/* Build TTBR0 identity + TTBR1 physmap and enable the MMU + caches, at the
	 * physical address of aarch64_mmu_init (it only touches adrp-relative globals
	 * and immediates, so it is position-correct here). */
2:	ldr	x0, =aarch64_mmu_init
	sub	x0, x0, x20
	blr	x0

	/* MMU on (both TTBRs).  Branch to the high half — from here the PC is a high
	 * VMA mapped by the TTBR1 physmap. */
	ldr	x0, =_start_high
	br	x0
	/* Keep this stub's literal pool (incl. the high VMA of _start_high and the
	 * VIRT_OFFSET value) inside .text.boot so the MMU-off code can reach it. */
	.ltorg
	.size _start, . - _start

	/* High-half continuation: linked + executed at the high VMA. */
	.section .text, "ax"
	.type _start_high, %function
_start_high:
	/* Switch to the high-VA stack, then enter the C bring-up. */
	ldr	x0, =_stack_top
	mov	sp, x0

	mov	x0, x19			/* DTB physical address */
	bl	kmain_aarch64

	/* kmain_aarch64 should not return; if it does, park the CPU. */
3:	wfi
	b	3b
	.size _start_high, . - _start_high
