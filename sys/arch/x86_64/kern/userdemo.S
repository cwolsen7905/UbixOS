/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 ring-3 demo payload (Phase 5a).  A position-independent blob the kernel
 * copies into a user page and runs at ring 3 to prove the privilege transition +
 * syscall path: it writes a string with the bring-up `write` syscall (nr 1) then
 * terminates with `exit` (nr 2), both via `int $0x80`.  RIP-relative addressing
 * keeps it relocatable, so it runs correctly at whatever user VA it is mapped at.
 * Sibling of aarch64's user_demo_code (bringup el0 payload).
 */

	.text
	.globl x86_64_user_demo_start
x86_64_user_demo_start:
	/* write(1, msg, msglen) */
	movq $1, %rax            /* bring-up SYS_write */
	movq $1, %rdi            /* fd = 1 */
	leaq msg(%rip), %rsi     /* buf (RIP-relative -> correct at any map VA) */
	movq $(msg_end - msg), %rdx
	int $0x80

	/* exit(0) */
	movq $2, %rax            /* bring-up SYS_exit */
	xorq %rdi, %rdi
	int $0x80

	/* exit should not return; spin defensively. */
1:
	jmp 1b

msg:
	.ascii "hello from ring 3 (x86_64 user mode)\n"
msg_end:

	.globl x86_64_user_demo_end
x86_64_user_demo_end:
