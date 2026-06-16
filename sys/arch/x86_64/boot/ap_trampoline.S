/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 application-processor (AP) startup trampoline (smp-plan, x86_64 AP boot).
 *
 * An AP begins in 16-bit real mode at (SIPI vector << 12) — the BSP copies the
 * bytes between ap_trampoline_start/ap_trampoline_end to a fixed low page
 * (AP_TRAMP_PHYS = 0x8000, vector 0x08) and fills the arg block (the kernel CR3,
 * the AP's stack top, its cpuid, and the 64-bit C entry) at AP_ARGS_PHYS, then
 * sends INIT-SIPI-SIPI.  This blob walks the AP real -> protected -> long mode
 * (mirroring the BSP's start.S) on the SHARED kernel PML4, then jumps to the C
 * entry.  All internal references are relative to AP_TRAMP_PHYS because the code
 * executes at that physical address, not its kernel-image VMA.
 */

	.set AP_TRAMP_PHYS, 0x8000
	.set AP_ARGS_PHYS,  0x8F00      /* arg block within the same page */
	.set ARG_CR3,   AP_ARGS_PHYS + 0x00  /* u32: kernel PML4 physical */
	.set ARG_STACK, AP_ARGS_PHYS + 0x08  /* u64: AP kernel stack top  */
	.set ARG_CPUID, AP_ARGS_PHYS + 0x10  /* u32: logical cpu id       */
	.set ARG_ENTRY, AP_ARGS_PHYS + 0x18  /* u64: 64-bit C entry VA    */

	/* The blob runs at AP_TRAMP_PHYS, so internal targets are
	 * (label - ap_trampoline_start + AP_TRAMP_PHYS).  GAS has no parameterized
	 * .set, so that offset is written out at each use site. */
	.section .ap_trampoline, "ax", @progbits
	.code16
	.globl ap_trampoline_start
ap_trampoline_start:
	cli
	cld
	xorw %ax, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	lgdtl (ap_gdt32_ptr - ap_trampoline_start + AP_TRAMP_PHYS)          /* 32-bit GDT */
	movl %cr0, %eax
	orl $1, %eax                     /* CR0.PE */
	movl %eax, %cr0
	ljmpl $0x08, $(ap_pm32 - ap_trampoline_start + AP_TRAMP_PHYS)       /* -> 32-bit code selector */

	.code32
ap_pm32:
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	/* CR4.PAE (bit5) + OSFXSR/OSXMMEXCPT (SSE), matching the BSP's start.S. */
	movl %cr4, %eax
	orl $0x620, %eax
	movl %eax, %cr4
	/* CR3 = kernel PML4 (physical, < 4 GB). */
	movl (ARG_CR3), %eax
	movl %eax, %cr3
	/* EFER.LME (MSR 0xC0000080, bit 8). */
	movl $0xC0000080, %ecx
	rdmsr
	orl $0x100, %eax
	wrmsr
	/* CR0.PG|PE, clear EM, set MP — activate long mode. */
	movl %cr0, %eax
	orl $0x80000001, %eax
	andl $0xFFFFFFFB, %eax
	orl $0x2, %eax
	movl %eax, %cr0
	lgdtl (ap_gdt64_ptr - ap_trampoline_start + AP_TRAMP_PHYS)          /* 64-bit GDT */
	ljmpl $0x18, $(ap_lm64 - ap_trampoline_start + AP_TRAMP_PHYS)       /* -> 64-bit code selector */

	.code64
ap_lm64:
	movw $0x20, %ax                  /* 64-bit data selector */
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw %ax, %fs
	movw %ax, %gs
	movq (ARG_STACK), %rsp           /* AP kernel stack (physmap/kernel VA) */
	movl (ARG_CPUID), %edi           /* arg 0 = cpuid */
	fninit
	movq (ARG_ENTRY), %rax           /* 64-bit C entry (high kernel VMA) */
	jmp *%rax
	/* x86_64_ap_entry does not return. */

	.align 16
ap_gdt32:
	.quad 0x0000000000000000
	.quad 0x00CF9A000000FFFF         /* 0x08: 32-bit code (G,D, base0, lim4G, exec/read) */
	.quad 0x00CF92000000FFFF         /* 0x10: 32-bit data */
ap_gdt32_end:
ap_gdt32_ptr:
	.word ap_gdt32_end - ap_gdt32 - 1
	.long (ap_gdt32 - ap_trampoline_start + AP_TRAMP_PHYS)

	.align 16
ap_gdt64:
	.quad 0x0000000000000000         /* 0x00 null */
	.quad 0x0000000000000000         /* 0x08 (unused) */
	.quad 0x0000000000000000         /* 0x10 (unused) */
	.quad 0x00209A0000000000         /* 0x18: 64-bit code (P,DPL0,S,exec/read,L) */
	.quad 0x0000920000000000         /* 0x20: 64-bit data (P,DPL0,S,read/write) */
ap_gdt64_end:
ap_gdt64_ptr:
	.word ap_gdt64_end - ap_gdt64 - 1
	.long (ap_gdt64 - ap_trampoline_start + AP_TRAMP_PHYS)

	.globl ap_trampoline_end
ap_trampoline_end:
