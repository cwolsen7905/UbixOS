/*
 * Raw, TLS-free assembly helpers for tlstest — validates per-thread %gs TLS.
 *
 * These deliberately avoid the C runtime's own TLS (no errno/stdio) so the
 * spawned worker threads, which have no musl thread descriptor, can run safely.
 */
.text

/*
 * spawn_thread — minimal clone() wrapper (mirrors bin/clonetest/clone_test.S).
 *
 *   int spawn_thread(int (*fn)(void *), void *stack_top, void *arg);
 *
 * Stashes fn/arg on the child stack, invokes rfork (slot 251) with
 * (flags=0, child_stack, tls=0); the child runs fn(arg) on its own stack in the
 * shared address space, then SYS_exit.
 */
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

	push $0			/* tf_esp+12 = tls (none) */
	push %ecx		/* tf_esp+8  = child stack */
	push $0			/* tf_esp+4  = flags */
	push $0			/* tf_esp+0  = fake return address */
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

/*
 * set_tls — install this thread's userland TLS base via set_thread_area.
 *
 *   void set_tls(void *base);
 *
 * set_thread_area lives in the UbixOS-native table (int $0x81, slot 63), not the
 * FreeBSD POSIX table — it is a Linux primitive with no FreeBSD number.  The
 * syscall layer reads args at user_esp + 4 (it skips the return address), so
 * push the base after a fake return address.
 */
.global set_tls
.type   set_tls,@function
set_tls:
	mov 4(%esp), %ecx	/* base */
	push %ecx		/* args+4 = u_info (the TLS base) */
	push $0			/* args+0 = fake return address */
	mov $63, %eax		/* UbixOS-native slot 63 = set_thread_area */
	int $0x81
	add $8, %esp
	ret

/* int read_gs0(void) — read the word at %gs:0 (the TLS self-slot). */
.global read_gs0
.type   read_gs0,@function
read_gs0:
	mov %gs:0, %eax
	ret

/* void write_gs0(int v) — write v to the word at %gs:0. */
.global write_gs0
.type   write_gs0,@function
write_gs0:
	mov 4(%esp), %eax
	mov %eax, %gs:0
	ret
