/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 register-level context switch (bring-up Phase 4b).
 *
 * Saves the callee-saved registers (SysV AMD64: rbx, rbp, r12-r15) + RSP of the
 * outgoing context to *save_rsp, loads the incoming RSP, restores its callee-saved
 * set, and RETs into its saved return address.  The MD core the scheduler's
 * switch_to() wraps; the x86_64 analogue of aarch64_ctx_switch.  Call with
 * interrupts disabled.
 */

	.section .text
	.code64
	.globl x86_64_ctx_switch
	.type x86_64_ctx_switch, @function
/* void x86_64_ctx_switch(uint64_t *save_rsp /rdi/, uint64_t next_rsp /rsi/) */
x86_64_ctx_switch:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq %rsp, (%rdi) /* save outgoing RSP */
	movq %rsi, %rsp   /* load incoming RSP */
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	ret /* resume incoming context at its saved return address */
	.size x86_64_ctx_switch, . - x86_64_ctx_switch
