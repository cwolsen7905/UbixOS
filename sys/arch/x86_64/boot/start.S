/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 kernel entry (long-mode bring-up).
 *
 * GRUB/QEMU multiboot loads us in 32-bit protected mode at the ELF entry (_start,
 * below), MMU off-ish (flat segments, no paging), %eax = 0x2BADB002, %ebx ->
 * multiboot info.  Long mode REQUIRES paging, so the 32-bit stub builds an
 * identity map of the low 1 GB with 2 MB pages, enables PAE + EFER.LME + paging,
 * loads a 64-bit GDT, and far-jumps into 64-bit code, which calls kmain_x86_64().
 * This is the x86_64 analogue of i386's start.S + the aarch64 start.S.
 */

	/*
	 * PVH boot note.  QEMU's multiboot loader refuses a 64-bit ELF ("give a 32bit
	 * one"), so we boot via the PVH protocol instead: QEMU reads this ELF note,
	 * finds a 32-bit physical entry point (PHYS32_ENTRY = type 18), and enters it
	 * in 32-bit protected mode (paging off, flat segments) — exactly what the
	 * long-mode trampoline below expects.  Works with `qemu -kernel`.  (GRUB-on-
	 * disk multiboot, which CAN load ELF64, is a separate later boot path.)
	 */
	.section .note.Xen, "a", @note
	.align 4
	.long 4    /* namesz = sizeof("Xen\0") */
	.long 4    /* descsz = 4-byte entry address */
	.long 18   /* type = XEN_ELFNOTE_PHYS32_ENTRY */
	.asciz "Xen"
	.align 4
	.long _start /* 32-bit physical entry point */
	.align 4

	.section .text.boot, "ax"
	.code32
	.globl _start
	.type _start, @function
_start:
	cli
	movl $_stack_top, %esp /* 32-bit stack (kernel linked < 4 GB) */
	movl %eax, %edi        /* stash multiboot magic -> arg0 (rdi) later */
	movl %ebx, %esi        /* stash multiboot info  -> arg1 (rsi) later */

	/* Zero BSS (covers the page tables below). */
	movl $__bss_start, %ecx
	movl $__bss_end, %edx
	subl %ecx, %edx
	pushl %edi
	pushl %esi
	movl %ecx, %edi
	movl %edx, %ecx
	xorl %eax, %eax
	rep stosb
	popl %esi
	popl %edi

	/* PML4[0] = PDPT | present|write. */
	movl $pdpt, %eax
	orl $0x3, %eax
	movl %eax, pml4
	/* PDPT[0] = PD | present|write. */
	movl $pd, %eax
	orl $0x3, %eax
	movl %eax, pdpt
	/* PD[i] = (i*2 MB) | present|write|PS  for i = 0..511  (identity-map 1 GB). */
	movl $pd, %ebx
	xorl %eax, %eax /* physical address, low dword */
	movl $512, %ecx
1:
	movl %eax, %edx
	orl $0x83, %edx /* present|write|PS(2 MB page) */
	movl %edx, (%ebx)
	movl $0, 4(%ebx) /* phys < 4 GB -> high dword 0 */
	addl $0x200000, %eax
	addl $8, %ebx
	loop 1b

	/* CR4.PAE (bit 5). */
	movl %cr4, %eax
	orl $0x20, %eax
	movl %eax, %cr4

	/* CR3 = PML4. */
	movl $pml4, %eax
	movl %eax, %cr3

	/* EFER.LME (MSR 0xC0000080, bit 8). */
	movl $0xC0000080, %ecx
	rdmsr
	orl $0x100, %eax
	wrmsr

	/* CR0.PG (bit 31) + PE (bit 0) -> activate long mode (compatibility submode). */
	movl %cr0, %eax
	orl $0x80000001, %eax
	movl %eax, %cr0

	/* Load the 64-bit GDT, then far-jump into the 64-bit code segment. */
	lgdt gdt64_ptr
	ljmp $0x08, $long_mode_entry

	.code64
long_mode_entry:
	movw $0x10, %ax /* 64-bit data selector */
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw %ax, %fs
	movw %ax, %gs
	movq $_stack_top, %rsp

	/* %edi/%esi still hold the multiboot magic/info (zero-extended). */
	call kmain_x86_64

	/* kmain should not return; park if it does. */
2:
	hlt
	jmp 2b
	.size _start, . - _start

	/* Minimal long-mode GDT: null, 64-bit ring-0 code (L=1), 64-bit ring-0 data. */
	.section .rodata
	.align 16
gdt64:
	.quad 0x0000000000000000 /* null */
	.quad 0x00209A0000000000 /* code: P, DPL0, S, exec/read, L */
	.quad 0x0000920000000000 /* data: P, DPL0, S, read/write */
gdt64_ptr:
	.word gdt64_ptr - gdt64 - 1
	.quad gdt64

	/* Page tables (4 KB each, identity map of the low 1 GB). */
	.section .bss
	.align 4096
pml4:
	.skip 4096
pdpt:
	.skip 4096
pd:
	.skip 4096
