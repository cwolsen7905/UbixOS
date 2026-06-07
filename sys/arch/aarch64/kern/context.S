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
