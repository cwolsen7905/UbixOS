/*
 * spawn_thread — a minimal, TLS-free rfork()/clone() wrapper for testing
 * sys_rfork on x86_64 (independent of musl pthreads/futex/TLS).
 *
 *   long spawn_thread(int (*fn)(void *), void *stack_top, void *arg);
 *     rdi = fn   rsi = stack_top   rdx = arg
 *
 * Stash fn/arg on the child stack, invoke rfork (FreeBSD slot 251) with
 * (flags=0, child_stack, tls=0).  The child resumes here with rax==0 on its own
 * stack in the SHARED address space, runs fn(arg), then SYS_exit(rc).  The parent
 * gets the new tid in rax.
 */
	.text
	.global spawn_thread
	.type spawn_thread,@function
spawn_thread:
	andq $-16, %rsi          /* align child stack */
	subq $16, %rsi
	movq %rdi, 0(%rsi)       /* [child_sp+0] = fn */
	movq %rdx, 8(%rsi)       /* [child_sp+8] = arg */

	movq $251, %rax          /* SYS_rfork */
	xorq %rdi, %rdi          /* arg1: flags = 0 */
	/* rsi already = child_stack (arg2) */
	xorq %rdx, %rdx          /* arg3: tls = 0 */
	syscall

	testq %rax, %rax
	jz 1f
	ret                      /* parent: rax = new tid */

1:	movq 0(%rsp), %rax       /* child: fn */
	movq 8(%rsp), %rdi       /* arg */
	xorq %rbp, %rbp
	call *%rax               /* fn(arg) */
	movq %rax, %rdi          /* exit code = fn's return */
	movq $1, %rax            /* SYS_exit */
	syscall
	hlt
