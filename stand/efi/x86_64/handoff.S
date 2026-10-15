/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86_64 hand-off trampoline.  UEFI leaves the CPU in 64-bit long mode; the
 * kernel's physical entry (_start) expects 32-bit protected mode, paging off.
 * This block is position independent: md.c copies it to a page below 4 GB and
 * calls it as  tramp(rcx = bootinfo, rdx = entry)  (Microsoft x64 convention).
 */

	.text
	.globl	ubix_tramp_start
	.globl	ubix_tramp_end

	.code64
ubix_tramp_start:
	cli
	movl	%ecx, %esi		/* esi = boot-info (below 4 GB) */
	movl	%edx, %edi		/* edi = kernel physical entry */
	leaq	tgdt(%rip), %rax
	movq	%rax, tgdtr+2(%rip)	/* gdtr base = this copy's table */
	lgdt	tgdtr(%rip)
	leaq	tpm32(%rip), %rax
	pushq	$0x08			/* 32-bit code selector */
	pushq	%rax
	lretq

	.code32
tpm32:
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	%ax, %fs
	movw	%ax, %gs
	movl	%cr0, %eax
	andl	$0x7FFFFFFF, %eax	/* PG off (EFER.LMA clears) */
	movl	%eax, %cr0
	movl	$0xC0000080, %ecx	/* EFER */
	rdmsr
	andl	$0xFFFFFEFF, %eax	/* LME off */
	wrmsr
	xorl	%eax, %eax
	movl	%eax, %cr4		/* PAE etc. off */
	movl	$0x55424958, %eax	/* UBIX_BOOTINFO_MAGIC */
	movl	%esi, %ebx
	jmp	*%edi

	.balign 8
tgdt:
	.quad	0
	.quad	0x00CF9A000000FFFF	/* 0x08: 32-bit code, flat */
	.quad	0x00CF92000000FFFF	/* 0x10: 32-bit data, flat */
tgdtr:
	.word	3*8 - 1
	.quad	0
ubix_tramp_end:
