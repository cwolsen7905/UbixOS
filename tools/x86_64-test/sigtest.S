	.text
	.globl _start
_start:
	/* sigaction(SIGINT=2, &act, NULL) */
	movq $416, %rax
	movq $2, %rdi
	leaq act(%rip), %rsi
	xorq %rdx, %rdx
	syscall
	/* pid = getpid() */
	movq $20, %rax
	syscall
	movq %rax, %r12
	/* kill(pid, SIGINT) — delivered on this syscall's return */
	movq $37, %rax
	movq %r12, %rdi
	movq $2, %rsi
	syscall
	/* handler has run + returned via sigreturn by now */
	movq $4, %rax
	movq $1, %rdi
	leaq amsg(%rip), %rsi
	movq $(amsg_end - amsg), %rdx
	syscall
	movq $1, %rax            /* exit */
	xorq %rdi, %rdi
	syscall
1:	jmp 1b

handler:
	movq $4, %rax
	movq $1, %rdi
	leaq hmsg(%rip), %rsi
	movq $(hmsg_end - hmsg), %rdx
	syscall
	ret                      /* -> magic return addr -> #PF -> sigreturn */

	.align 8
act:
	.quad handler            /* sa_handler */
	.long 0                  /* sa_flags */
	.long 0, 0, 0, 0         /* sa_mask (sigset_t) */
hmsg:
	.ascii "  signal: HANDLER ran (caught SIGINT)\n"
hmsg_end:
amsg:
	.ascii "  signal: resumed after handler (sigreturn ok)\n"
amsg_end:
