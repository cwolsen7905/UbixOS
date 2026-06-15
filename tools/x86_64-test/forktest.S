	.text
	.globl _start
_start:
	movq $2, %rax            /* SYS_fork */
	syscall
	testq %rax, %rax
	jz 1f
	/* parent: rax = child pid */
	movq $4, %rax            /* write */
	movq $1, %rdi
	leaq pmsg(%rip), %rsi
	movq $(pmsg_end - pmsg), %rdx
	syscall
	jmp 2f
1:	/* child: rax = 0 */
	movq $4, %rax
	movq $1, %rdi
	leaq cmsg(%rip), %rsi
	movq $(cmsg_end - cmsg), %rdx
	syscall
2:	movq $1, %rax            /* exit */
	xorq %rdi, %rdi
	syscall
3:	jmp 3b
pmsg:
	.ascii "  fork: PARENT here (child pid in rax)\n"
pmsg_end:
cmsg:
	.ascii "  fork: CHILD here (fork returned 0)\n"
cmsg_end:
