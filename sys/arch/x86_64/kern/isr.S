/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 interrupt/exception stubs (long-mode bring-up Phase 2).
 *
 * One stub per CPU exception vector (0..31).  Vectors whose exception pushes an
 * error code (8,10,11,12,13,14,17,21) leave it; the rest push a dummy 0 so the
 * trapframe layout is uniform.  Each stub pushes its vector number and jumps to a
 * common path that saves the GP registers, calls the C handler with a pointer to
 * the trapframe, then restores and IRETQs.  Mirrors i386's vector stubs + the
 * aarch64 exception entry.
 */

	.section .text
	.code64

	.macro ISR_NOERR vec
	.globl isr\vec
isr\vec:
	pushq $0      /* dummy error code */
	pushq $\vec   /* vector number */
	jmp isr_common
	.endm

	.macro ISR_ERR vec
	.globl isr\vec
isr\vec:
	pushq $\vec   /* vector number (CPU already pushed the error code) */
	jmp isr_common
	.endm

	ISR_NOERR 0   /* #DE divide error            */
	ISR_NOERR 1   /* #DB debug                   */
	ISR_NOERR 2   /* NMI                         */
	ISR_NOERR 3   /* #BP breakpoint              */
	ISR_NOERR 4   /* #OF overflow                */
	ISR_NOERR 5   /* #BR bound range             */
	ISR_NOERR 6   /* #UD invalid opcode          */
	ISR_NOERR 7   /* #NM device not available    */
	ISR_ERR   8   /* #DF double fault            */
	ISR_NOERR 9   /* coprocessor seg overrun     */
	ISR_ERR   10  /* #TS invalid TSS             */
	ISR_ERR   11  /* #NP segment not present     */
	ISR_ERR   12  /* #SS stack-segment fault     */
	ISR_ERR   13  /* #GP general protection      */
	ISR_ERR   14  /* #PF page fault              */
	ISR_NOERR 15  /* reserved                    */
	ISR_NOERR 16  /* #MF x87 FP error            */
	ISR_ERR   17  /* #AC alignment check         */
	ISR_NOERR 18  /* #MC machine check           */
	ISR_NOERR 19  /* #XM SIMD FP                 */
	ISR_NOERR 20  /* #VE virtualization          */
	ISR_ERR   21  /* #CP control protection      */
	ISR_NOERR 22
	ISR_NOERR 23
	ISR_NOERR 24
	ISR_NOERR 25
	ISR_NOERR 26
	ISR_NOERR 27
	ISR_NOERR 28
	ISR_NOERR 29
	ISR_NOERR 30
	ISR_NOERR 31

	/* Hardware IRQ vectors (PIC-remapped to 32..47).  No error code; the common
	 * path routes vector >= 32 to the IRQ handler + EOI (idt.c). */
	ISR_NOERR 32  /* IRQ0  PIT timer   */
	ISR_NOERR 33  /* IRQ1  keyboard    */
	ISR_NOERR 34
	ISR_NOERR 35
	ISR_NOERR 36
	ISR_NOERR 37
	ISR_NOERR 38
	ISR_NOERR 39
	ISR_NOERR 40
	ISR_NOERR 41
	ISR_NOERR 42
	ISR_NOERR 43
	ISR_NOERR 44
	ISR_NOERR 45
	ISR_NOERR 46
	ISR_NOERR 47

	/*
	 * Common entry: save GP regs (rax pushed last -> lowest address -> rsp), pass
	 * the trapframe pointer in rdi, call the C handler, restore, pop vector+error,
	 * IRETQ.  struct x86_64_trapframe (idt.c) matches this push order.
	 */
isr_common:
	pushq %r15
	pushq %r14
	pushq %r13
	pushq %r12
	pushq %r11
	pushq %r10
	pushq %r9
	pushq %r8
	pushq %rbp
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %rcx
	pushq %rbx
	pushq %rax

	movq %rsp, %rdi   /* arg0 = &trapframe */
	call x86_64_exception

	popq %rax
	popq %rbx
	popq %rcx
	popq %rdx
	popq %rsi
	popq %rdi
	popq %rbp
	popq %r8
	popq %r9
	popq %r10
	popq %r11
	popq %r12
	popq %r13
	popq %r14
	popq %r15

	addq $16, %rsp    /* discard vector + error code */
	iretq

	/* Table of stub entry points (48 = 32 exceptions + 16 IRQs) for idt_init(). */
	.section .rodata
	.align 8
	.globl isr_stub_table
isr_stub_table:
	.quad isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
	.quad isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
	.quad isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
	.quad isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
	.quad isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
	.quad isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
