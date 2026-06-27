/*
 * spawn_thread — minimal clone() wrapper for futextest (mirrors clonetest).
 *
 *   int spawn_thread(int (*fn)(void *), void *stack_top, void *arg);
 *
 * Stashes fn/arg on the child stack, invokes rfork (slot 251) with
 * (flags=0, child_stack, tls=0); the child runs fn(arg) on its own stack in
 * the shared address space, then SYS_exit.
 */
.text
.global spawn_thread
.type   spawn_thread,@function
spawn_thread:
	push %ebp
	mov %esp, %ebp
	push %ebx
	push %esi
	push %edi

	mov 12(%ebp), %ecx	/* stack top */
	and $-16, %ecx
	sub $16, %ecx
	mov 16(%ebp), %eax	/* arg */
	mov %eax, 4(%ecx)
	mov 8(%ebp), %eax	/* fn */
	mov %eax, 0(%ecx)

	push $0			/* tls */
	push %ecx		/* child stack */
	push $0			/* flags */
	push $0			/* fake return address */
	mov $251, %eax		/* SYS_rfork */
	int $0x80

	test %eax, %eax
	jz 1f

	add $16, %esp
	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
	ret

1:	pop %eax		/* fn; esp -> arg */
	xor %ebp, %ebp
	call *%eax		/* fn(arg) */
	mov %eax, %ebx
	push %ebx
	push $0
	mov $1, %eax		/* SYS_exit */
	int $0x80
	hlt
