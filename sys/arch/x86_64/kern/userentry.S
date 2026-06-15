/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 ring-3 entry/exit (Phase 5a, one-shot bring-up).  x86_64_enter_user()
 * drops to ring 3 at a given RIP/RSP via IRETQ, after stashing the kernel's
 * callee-saved registers + RSP; the "exit" syscall calls x86_64_leave_user(),
 * which restores that kernel context so control resumes as if enter_user had
 * returned (a coroutine swap, like the scheduler's context switch).  Superseded
 * by the scheduler-driven user_trampoline once tasks own address spaces (5b).
 */

	.text

	/* void x86_64_enter_user(u64 user_rip /​* rdi *​/, u64 user_rsp /​* rsi *​/); */
	.globl x86_64_enter_user
	.type x86_64_enter_user, @function
x86_64_enter_user:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq %rsp, g_kernel_resume_sp(%rip) /* where leave_user resumes */

	/* Build the IRETQ frame: SS, RSP, RFLAGS, CS, RIP (top-down). */
	pushq $0x1b      /* SS  = user data | RPL 3 */
	pushq %rsi       /* RSP = user stack */
	pushq $0x202     /* RFLAGS: IF set (bit 9) + reserved bit 1 */
	pushq $0x23      /* CS  = user code | RPL 3 */
	pushq %rdi       /* RIP = user entry */

	/* Point the data segments at the user data selector before leaving. */
	movw $0x1b, %ax
	movw %ax, %ds
	movw %ax, %es
	iretq
	.size x86_64_enter_user, . - x86_64_enter_user

	/* void x86_64_leave_user(void) — called in ring 0 from the exit syscall;
	 * does not return to its caller, returns to x86_64_enter_user's caller. */
	.globl x86_64_leave_user
	.type x86_64_leave_user, @function
x86_64_leave_user:
	movq g_kernel_resume_sp(%rip), %rsp
	movw $0x10, %ax /* restore kernel data selector */
	movw %ax, %ds
	movw %ax, %es
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	ret
	.size x86_64_leave_user, . - x86_64_leave_user

	/* void x86_64_iret_to_user(u64 rip /​* rdi *​/, u64 rsp /​* rsi *​/);
	 * Bare IRETQ to ring 3 — no kernel context saved (the caller is a scheduled
	 * user task's first-switch trampoline; it never returns to the kernel except
	 * through a syscall/trap, which lands on the task's own kernel stack). */
	.globl x86_64_iret_to_user
	.type x86_64_iret_to_user, @function
x86_64_iret_to_user:
	pushq $0x1b      /* SS  = user data | RPL 3 */
	pushq %rsi       /* RSP = user stack */
	pushq $0x202     /* RFLAGS: IF + reserved bit 1 */
	pushq $0x23      /* CS  = user code | RPL 3 */
	pushq %rdi       /* RIP = user entry */
	movw $0x1b, %ax
	movw %ax, %ds
	movw %ax, %es
	iretq
	.size x86_64_iret_to_user, . - x86_64_iret_to_user

	.bss
	.align 8
	.globl g_kernel_resume_sp
g_kernel_resume_sp:
	.skip 8
