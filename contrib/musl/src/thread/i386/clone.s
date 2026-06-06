/*
 * __clone for UbixOS — wires musl's pthread_create onto the kernel's
 * rfork(RFMEM) thread-create syscall (FreeBSD slot 251), int $0x80.
 *
 * C signature (i386):
 *   int __clone(int (*fn)(void*), void *stack, int flags,
 *               void *arg, pid_t *ptid, void *tls, pid_t *ctid);
 *   ebp+8=fn  +12=stack  +16=flags  +20=arg  +24=ptid  +28=tls  +32=ctid
 *
 * Kernel ABI (sys_rfork): args read from the user stack at tf_esp+4:
 *   arg1=flags  arg2=child_stack  arg3=tls
 * The kernel starts the new thread on child_stack at this same eip with
 * eax==0; the parent gets the new tid in eax.  fn+arg are stashed on the
 * child stack so the child can retrieve and call them.
 */
.text
.global __clone
.hidden __clone
.type   __clone,@function
__clone:
	push %ebp
	mov %esp, %ebp
	push %ebx
	push %esi
	push %edi

	/* Build the child stack top: 16-byte aligned, with fn at [cs+0] and
	 * arg at [cs+4] for the child path to pick up. */
	mov 12(%ebp), %ecx	/* child stack */
	and $-16, %ecx
	sub $16, %ecx
	mov 20(%ebp), %eax	/* arg */
	mov %eax, 4(%ecx)
	mov 8(%ebp), %eax	/* fn */
	mov %eax, 0(%ecx)

	/* Push the uBixOS syscall args (flags, child_stack, tls) + a fake return
	 * slot, so the kernel reads them at tf_esp+4.. */
	mov 28(%ebp), %edx	/* tls */
	mov 16(%ebp), %eax	/* flags */
	push %edx		/* tf_esp+12 = tls */
	push %ecx		/* tf_esp+8  = child stack */
	push %eax		/* tf_esp+4  = flags */
	push $0			/* tf_esp+0  = fake return address */
	mov $251, %eax		/* SYS_rfork */
	int $0x80

	/* eax==0 -> child (on its own stack); else -> parent (eax = new tid). */
	test %eax, %eax
	jz 1f

	add $16, %esp		/* drop the 4 pushed dwords */
	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
	ret

1:	/* Child: esp = child_stack, [esp]=fn, [esp+4]=arg. */
	pop %eax		/* fn; esp -> arg */
	xor %ebp, %ebp		/* terminate the call-frame chain */
	call *%eax		/* fn(arg) */
	mov %eax, %ebx		/* fn's return = exit status */
	push %ebx
	push $0			/* fake return slot */
	mov $1, %eax		/* SYS_exit */
	int $0x80
	hlt
