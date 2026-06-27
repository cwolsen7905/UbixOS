/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Raspberry Pi 3 Model B v1.2 (BCM2837) - M0 bare-metal serial hello.
 *
 * The VideoCore firmware loads kernel8.img as a raw flat binary at 0x80000 and
 * enters core 0 at EL2 with x0 = DTB physical address; cores 1-3 are held in the
 * firmware spin-table.  No U-Boot, no arm64 Image header needed - just objcopy to
 * a flat binary.  This is the smallest board test: gate to core 0, set a stack,
 * print a banner over PL011, halt.  EL2->EL1 + the BCM interrupt controller come
 * with the real kernel in M1.  Build: tools/build-rpi3-hello.sh.
 */
	.section .text.boot, "ax"
	.globl _start
_start:
	mrs	x1, mpidr_el1
	and	x1, x1, #0xff		/* Aff0 = core id within cluster 0 */
	cbnz	x1, park		/* only core 0 runs M0; park 1-3 defensively */
	mov	x19, x0			/* preserve the DTB pointer for later milestones */
	ldr	x1, =_stack_top
	mov	sp, x1
	bl	m0_main
hang:
	wfe
	b	hang
park:
	wfe
	b	park
