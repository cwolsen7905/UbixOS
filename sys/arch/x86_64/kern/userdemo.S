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

	/* --- syscall-instruction variant: mmap + open/read/write ---------------- *
	 * (1) mmap an anonymous RW page, copy a message into it, and write that page to
	 * fd 1 — proves the mmap/VM glue gives a usable user page.  (2) open + read the
	 * first bytes of a FAT file and write them — proves the real sys_open/sys_read
	 * via the table.  `syscall` clobbers RCX/R11; args RDI/RSI/RDX/R10/R8/R9, nr in
	 * RAX; RBX/RBP/R12-R15 survive (the kernel entry stub preserves them). */
	.globl x86_64_user_demo_sc_start
x86_64_user_demo_sc_start:
	/* p = mmap(0, 4096, PROT_RW, MAP_ANON|MAP_PRIVATE, -1, 0) */
	movq $477, %rax          /* SYS_mmap */
	xorq %rdi, %rdi          /* addr = 0 (kernel picks) */
	movq $4096, %rsi         /* len */
	movq $3, %rdx            /* prot = READ|WRITE */
	movq $0x1002, %r10       /* flags = MAP_PRIVATE|MAP_ANON (FreeBSD) */
	movq $-1, %r8            /* fd */
	xorq %r9, %r9            /* offset */
	syscall
	movq %rax, %r12          /* mapped base (survives syscalls) */

	/* copy the message into the mmap'd page, then write(1, p, len) */
	leaq .Lmm(%rip), %rsi
	movq %r12, %rdi
	movq $(.Lmm_end - .Lmm), %rcx
	rep movsb
	movq $4, %rax            /* SYS_write(1, p, len) */
	movq $1, %rdi
	movq %r12, %rsi
	movq $(.Lmm_end - .Lmm), %rdx
	syscall

	/* fd = open("/kernel/kernel"); read 16 bytes into the page; write them */
	movq $5, %rax
	leaq .Lpath(%rip), %rdi
	xorq %rsi, %rsi
	syscall
	movq %rax, %r13
	movq $3, %rax            /* SYS_read(fd, p, 16) */
	movq %r13, %rdi
	movq %r12, %rsi
	movq $16, %rdx
	syscall
	movq %rax, %rdx
	movq $4, %rax            /* SYS_write(1, p, n) */
	movq $1, %rdi
	movq %r12, %rsi
	syscall

	movq $1, %rax            /* SYS_exit(0) */
	xorq %rdi, %rdi
	syscall
.Lspin1:
	jmp .Lspin1
.Lmm:
	.ascii "[mmap'd page OK] "
.Lmm_end:
.Lpath:
	.asciz "/kernel/kernel"
	.globl x86_64_user_demo_sc_end
x86_64_user_demo_sc_end:
