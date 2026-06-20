/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 SYSCALL instruction entry (Phase 5d step C-2) — the fast syscall path
 * musl uses.  On `syscall` the CPU saves the return RIP in RCX and RFLAGS in R11,
 * loads CS/SS from STAR, masks the SFMASK RFLAGS bits, and jumps here — but it
 * does NOT switch RSP, which is still the user stack.  The kernel owns %gs (musl
 * keeps its TLS in %fs), so no swapgs is needed: per-CPU state, including this
 * task's kernel-stack top, is read straight through %gs.
 *
 * The stub builds the same struct x86_64_trapframe the IDT path uses (so the C
 * handler x86_64_syscall is shared), then returns with SYSRETQ (RIP <- RCX,
 * RFLAGS <- R11, CS/SS from STAR, RSP restored to the user stack).
 *
 * struct pcpu offsets (x86_64/pcpu.h): kernel_rsp @40, user_rsp_scratch @48.
 */

	.text
	.code64
	.globl x86_64_syscall_entry
	.type x86_64_syscall_entry, @function
x86_64_syscall_entry:
	movq %rsp, %gs:48 /* stash the user RSP */
	movq %gs:40, %rsp /* switch to this task's kernel stack */

	/* Build the IRET-shaped tail of struct x86_64_trapframe (ss, rsp, rflags, cs,
	 * rip) from the syscall-saved registers, then a dummy error + a sentinel
	 * vector, then the GP registers (same order as isr.S's isr_common). */
	pushq $0x1b      /* ss = user data | RPL 3 */
	pushq %gs:48     /* user rsp */
	pushq %r11       /* rflags (saved by syscall) */
	pushq $0x23      /* cs = user code | RPL 3 */
	pushq %rcx       /* rip (saved by syscall) */
	pushq $0         /* error code */
	pushq $0x80      /* vector sentinel (syscall) */

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

	movq %rsp, %rdi /* arg0 = &trapframe */
	call x86_64_syscall

	/* fork()'s child is dispatched here: switch_to lands in ret_from_fork (the
	 * seeded ctx frame's return address) with RSP pointing at the copied trapframe
	 * (rax already 0), so it shares this exact pop-and-SYSRETQ return tail. */
	.globl ret_from_fork
	.type ret_from_fork, @function
ret_from_fork:
	/* Release the run-queue lock held across the dispatch switch (SMP-safe sched).
	 * call/ret is RSP-balanced, so the trapframe below is untouched; the clobbered
	 * caller-saved regs are about to be overwritten by the pops anyway. */
	call sched_resume_unlock
	popq %rax /* rax = return value the handler stored (0 for the forked child) */
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
	addq $16, %rsp /* drop vector + error code */

	/* Stack now: [0]rip [8]cs [16]rflags [24]user_rsp [32]ss.  SYSRETQ wants
	 * RCX=rip, R11=rflags, and the user RSP reloaded (it does not restore RSP). */
	movq 0(%rsp), %rcx
	movq 16(%rsp), %r11
	movq 24(%rsp), %rsp
	sysretq
	.size x86_64_syscall_entry, . - x86_64_syscall_entry
