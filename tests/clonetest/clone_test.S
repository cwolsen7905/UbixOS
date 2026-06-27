/*
 * spawn_thread — a minimal, TLS-free rfork()/clone() wrapper for testing
 * sys_rfork (independent of musl pthreads/futex/TLS).
 *
 *   long spawn_thread(int (*fn)(void *), void *stack_top, void *arg);
 *
 * Stash fn/arg on the child stack, invoke rfork (FreeBSD slot 251) with
 * (flags=0, child_stack, tls=0).  The child resumes here with the syscall
 * return == 0 on its own stack in the SHARED address space, runs fn(arg), then
 * SYS_exit(rc).  The parent gets the new tid.
 */
#if defined(__x86_64__)
	.text
	.global spawn_thread
	.type spawn_thread,@function
/* rdi = fn   rsi = stack_top   rdx = arg */
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

#elif defined(__aarch64__)
	.text
	.global spawn_thread
	.type spawn_thread,%function
/* x0 = fn   x1 = stack_top   x2 = arg */
spawn_thread:
	and x1, x1, #-16         /* align child stack */
	sub x1, x1, #16
	str x0, [x1, #0]         /* [child_sp+0] = fn */
	str x2, [x1, #8]         /* [child_sp+8] = arg */

	mov x8, #251             /* SYS_rfork */
	mov x0, #0               /* arg0: flags = 0 */
	/* x1 already = child_stack (arg1) */
	mov x2, #0               /* arg2: tls = 0 */
	svc #0

	cbz x0, 1f
	ret                      /* parent: x0 = new tid */

1:	ldr x9, [sp, #0]         /* child: fn  (sp == child_stack) */
	ldr x0, [sp, #8]         /* arg */
	blr x9                   /* fn(arg) */
	mov x8, #1               /* SYS_exit */
	svc #0
	b .
#endif
