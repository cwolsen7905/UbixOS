/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 EL0 (user) entry/return + a tiny EL0 demo payload (QEMU `virt`
 * bring-up).
 *
 * aarch64_enter_el0() drops to EL0 to run user code, having stashed the kernel's
 * callee-saved state so aarch64_el0_return() can longjmp back as if enter_el0()
 * had returned (used by the SVC "exit" path).  This is the embryonic form of the
 * exec/return-to-user mechanism; per-process trapframes replace it later.
 */

	.section .text

/*
 * void aarch64_enter_el0(uint64_t entry [x0], uint64_t ustack [x1]);
 *   Save kernel callee-saved + SP into g_el0_save, set SP_EL0/ELR/SPSR, ERET.
 */
	.global aarch64_enter_el0
	.type aarch64_enter_el0, %function
aarch64_enter_el0:
	adrp	x9, g_el0_save
	add	x9, x9, :lo12:g_el0_save
	stp	x19, x20, [x9, #0]
	stp	x21, x22, [x9, #16]
	stp	x23, x24, [x9, #32]
	stp	x25, x26, [x9, #48]
	stp	x27, x28, [x9, #64]
	stp	x29, x30, [x9, #80]
	mov	x10, sp
	str	x10, [x9, #96]

	msr	sp_el0, x1		/* user stack */
	msr	elr_el1, x0		/* user entry PC */
	mov	x10, #0x3C0		/* SPSR: M=EL0t, DAIF masked */
	msr	spsr_el1, x10
	isb
	eret
	.size aarch64_enter_el0, . - aarch64_enter_el0

/*
 * void aarch64_eret_to_el0(uint64_t entry [x0], uint64_t usp [x1]);
 *   ERET to EL0 at @entry with SP_EL0=@usp, WITHOUT stashing kernel context.
 *   Used to launch a scheduled user task: it lives at EL0 until it exits (which
 *   terminates the task), so there is no kernel caller to return to.  SP_EL1 is
 *   left as-is (the task's kernel stack), so its later traps land there.
 */
	.global aarch64_eret_to_el0
	.type aarch64_eret_to_el0, %function
aarch64_eret_to_el0:
	msr	sp_el0, x1
	msr	elr_el1, x0
	mov	x10, #0x3C0		/* SPSR: M=EL0t, DAIF masked (no preemption yet) */
	msr	spsr_el1, x10
	isb
	eret
	.size aarch64_eret_to_el0, . - aarch64_eret_to_el0

/*
 * void aarch64_el0_return(void);
 *   Restore the state saved by aarch64_enter_el0 and `ret` into its caller —
 *   abandoning the current (exception-handler) stack.  Never returns normally.
 */
	.global aarch64_el0_return
	.type aarch64_el0_return, %function
aarch64_el0_return:
	adrp	x9, g_el0_save
	add	x9, x9, :lo12:g_el0_save
	ldp	x19, x20, [x9, #0]
	ldp	x21, x22, [x9, #16]
	ldp	x23, x24, [x9, #32]
	ldp	x25, x26, [x9, #48]
	ldp	x27, x28, [x9, #64]
	ldp	x29, x30, [x9, #80]
	ldr	x10, [x9, #96]
	mov	sp, x10
	ret
	.size aarch64_el0_return, . - aarch64_el0_return

/*
 * EL0 demo payload: write(1, msg, len) then exit(0) via real syscalls.
 * Position-independent (svc, relative branch, PC-relative `adr` for the
 * message), so it can be copied into any EL0-mapped page.  Syscall convention:
 * x8 = number (FreeBSD: write=4, exit=1), x0-x2 = args.
 */
	.global user_demo_code_start, user_demo_code_end
user_demo_code_start:
	mov	x0, #1					/* fd = stdout */
	adr	x1, user_demo_msg			/* buf (runtime VA) */
	mov	x2, #(user_demo_msg_end - user_demo_msg)	/* len */
	mov	x8, #4					/* SYS_write */
	svc	#0
	mov	x0, #0					/* exit code 0 */
	mov	x8, #1					/* SYS_exit */
	svc	#0
1:	b	1b					/* safety: never reached */
user_demo_msg:
	.ascii	"Hello from EL0 — aarch64 user mode via syscall!\n"
user_demo_msg_end:
user_demo_code_end:

	.section .bss
	.balign 16
g_el0_save:
	.skip 112			/* x19-x30 (96) + saved SP (8), padded */
