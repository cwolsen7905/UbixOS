/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 ring-3 demo payloads.  Position-independent blobs the kernel copies into
 * a user page and runs at ring 3 to prove the privilege transition + the syscall
 * path.  Both use the FreeBSD amd64 numbers (write = 4, exit = 1 — what musl
 * emits).  Each payload's message lives BETWEEN its start/end labels so it travels
 * with the code when copied, keeping `leaq msg(%rip)` correct at any map VA.
 * Sibling of aarch64's user_demo_code.
 *
 *   x86_64_user_demo_*     — invokes the kernel via `int $0x80` (the early gate)
 *   x86_64_user_demo_sc_*  — invokes the kernel via the `syscall` instruction
 *                            (the real fast path musl uses)
 */

	.text

	/* --- int 0x80 variant --------------------------------------------------- */
	.globl x86_64_user_demo_start
x86_64_user_demo_start:
	movq $4, %rax            /* SYS_write (FreeBSD) */
	movq $1, %rdi            /* fd = 1 */
	leaq .Lmsg(%rip), %rsi
	movq $(.Lmsg_end - .Lmsg), %rdx
	int $0x80

	movq $1, %rax            /* SYS_exit (FreeBSD) */
	xorq %rdi, %rdi
	int $0x80
.Lspin0:
	jmp .Lspin0
.Lmsg:
	.ascii "hello from ring 3 (int 0x80, x86_64)\n"
.Lmsg_end:
	.globl x86_64_user_demo_end
x86_64_user_demo_end:

	/* --- syscall-instruction variant ---------------------------------------- *
	 * `syscall` clobbers RCX (return RIP) and R11 (RFLAGS); the FreeBSD/SysV amd64
	 * ABI passes args in RDI/RSI/RDX/R10/R8/R9, nr in RAX. */
	.globl x86_64_user_demo_sc_start
x86_64_user_demo_sc_start:
	movq $4, %rax            /* SYS_write */
	movq $1, %rdi            /* fd = 1 */
	leaq .Lscmsg(%rip), %rsi
	movq $(.Lscmsg_end - .Lscmsg), %rdx
	syscall

	movq $1, %rax            /* SYS_exit */
	xorq %rdi, %rdi
	syscall
.Lspin1:
	jmp .Lspin1
.Lscmsg:
	.ascii "hello from ring 3 (syscall insn, x86_64)\n"
.Lscmsg_end:
	.globl x86_64_user_demo_sc_end
x86_64_user_demo_sc_end:
