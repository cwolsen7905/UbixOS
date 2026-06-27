/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Orange Pi Zero 2W (Allwinner H618) - M0 bare-metal serial hello.
 *
 * U-Boot's `booti` hands off in EL2 with the DTB physical address in x0 and
 * requires a flat arm64 `Image`, which begins with the 64-byte Image header
 * (Documentation/arm64/booting.rst).  This is the smallest possible board test:
 * emit that header, set a stack, print a banner over UART0, and halt.  No EL2->
 * EL1 drop yet (a banner runs fine at EL2); that arrives with the real kernel in
 * M1.  Build with tools/build-opi-hello.sh; flash + `booti` per the board plan.
 */
	.section .text.boot, "ax"
	.globl _start
_start:
	b	reset			/* code0: branch past the 64-byte Image header   */
	.long	0			/* code1                                          */
	.quad	0			/* text_offset (we load at the link addr)        */
	.quad	_image_size		/* image_size (effective)                        */
	.quad	0			/* flags: LE, 4 KB pages                          */
	.quad	0			/* res2                                           */
	.quad	0			/* res3                                           */
	.quad	0			/* res4                                           */
	.long	0x644d5241		/* magic "ARM\x64" (LE) at offset 0x38           */
	.long	0			/* res5 (PE/COFF header offset; 0 = no EFI)       */
reset:
	mov	x19, x0			/* preserve the DTB pointer for later milestones */
	ldr	x1, =_stack_top
	mov	sp, x1
	bl	m0_main
hang:
	wfe
	b	hang
