/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 cooperative context switch (QEMU `virt` bring-up, Phase 13b).
 *
 * The core scheduler primitive: save the AArch64 callee-saved state (x19-x28,
 * fp=x29, lr=x30) of the running thread onto its stack, stash its SP, load the
 * next thread's SP, restore its callee-saved state, and `ret`.  Because the
 * AAPCS64 caller already preserved x0-x18 around the call, saving the
 * callee-saved set + SP is a complete cooperative switch.  A fresh thread's
 * stack is seeded with a frame whose saved lr is its entry point (see
 * ctxdemo.c), so the first switch-in `ret`s straight into the thread.
 *
 * This is the standalone primitive; the generic sched_core port will drive it
 * (and an interrupt-driven variant will also swap TTBR0 per address space).
 */

	.section .text
	.global aarch64_ctx_switch
	.type aarch64_ctx_switch, %function

/* void aarch64_ctx_switch(uint64_t *save_sp, uint64_t next_sp)
 *   x0 = save_sp (where to stash the outgoing SP), x1 = next_sp (SP to load). */
aarch64_ctx_switch:
	/* Push callee-saved x19-x30 (12 regs = 96 bytes, 16-aligned). */
	sub	sp, sp, #96
	stp	x19, x20, [sp, #0]
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	x25, x26, [sp, #48]
	stp	x27, x28, [sp, #64]
	stp	x29, x30, [sp, #80]

	/* Stash the outgoing SP, load the incoming SP. */
	mov	x2, sp
	str	x2, [x0]
	mov	sp, x1

	/* Restore the incoming thread's callee-saved state and return into it. */
	ldp	x19, x20, [sp, #0]
	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	x25, x26, [sp, #48]
	ldp	x27, x28, [sp, #64]
	ldp	x29, x30, [sp, #80]
	add	sp, sp, #96
	ret
	.size aarch64_ctx_switch, . - aarch64_ctx_switch

/*
 * EL0 FP/SIMD state save/restore (q0-q31, FPCR, FPSR — 528 bytes).  The kernel
 * is built -mgeneral-regs-only and never touches these registers itself, so the
 * live values always belong to the user task that last ran on this CPU; switch_to
 * saves them for the outgoing task and loads the incoming task's.  Without this,
 * preempted processes resumed with another process's vector registers (musl's
 * memcpy/memset copy through q registers, so data was silently corrupted).
 */
	.arch	armv8-a+fp+simd

	.global aarch64_fpsimd_save
	.type aarch64_fpsimd_save, %function
/* void aarch64_fpsimd_save(u_int64_t *area)  — x0 = 528-byte save area */
aarch64_fpsimd_save:
	stp	q0, q1, [x0, #0]
	stp	q2, q3, [x0, #32]
	stp	q4, q5, [x0, #64]
	stp	q6, q7, [x0, #96]
	stp	q8, q9, [x0, #128]
	stp	q10, q11, [x0, #160]
	stp	q12, q13, [x0, #192]
	stp	q14, q15, [x0, #224]
	stp	q16, q17, [x0, #256]
	stp	q18, q19, [x0, #288]
	stp	q20, q21, [x0, #320]
	stp	q22, q23, [x0, #352]
	stp	q24, q25, [x0, #384]
	stp	q26, q27, [x0, #416]
	stp	q28, q29, [x0, #448]
	stp	q30, q31, [x0, #480]
	mrs	x1, fpcr
	mrs	x2, fpsr
	str	x1, [x0, #512]
	str	x2, [x0, #520]
	ret
	.size aarch64_fpsimd_save, . - aarch64_fpsimd_save

	.global aarch64_fpsimd_restore
	.type aarch64_fpsimd_restore, %function
/* void aarch64_fpsimd_restore(const u_int64_t *area) — x0 = 528-byte save area */
aarch64_fpsimd_restore:
	ldp	q0, q1, [x0, #0]
	ldp	q2, q3, [x0, #32]
	ldp	q4, q5, [x0, #64]
	ldp	q6, q7, [x0, #96]
	ldp	q8, q9, [x0, #128]
	ldp	q10, q11, [x0, #160]
	ldp	q12, q13, [x0, #192]
	ldp	q14, q15, [x0, #224]
	ldp	q16, q17, [x0, #256]
	ldp	q18, q19, [x0, #288]
	ldp	q20, q21, [x0, #320]
	ldp	q22, q23, [x0, #352]
	ldp	q24, q25, [x0, #384]
	ldp	q26, q27, [x0, #416]
	ldp	q28, q29, [x0, #448]
	ldp	q30, q31, [x0, #480]
	ldr	x1, [x0, #512]
	ldr	x2, [x0, #520]
	msr	fpcr, x1
	msr	fpsr, x2
	ret
	.size aarch64_fpsimd_restore, . - aarch64_fpsimd_restore
