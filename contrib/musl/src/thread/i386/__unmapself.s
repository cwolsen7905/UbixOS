/*
 * __unmapself(base, size) — unmap the exiting (DETACHED) thread's own
 * stack+TLS mapping, then exit, without touching the stack in between.
 *
 * On Linux this is munmap+exit using the register syscall ABI so no stack is
 * needed after the unmap.  UbixOS passes syscall arguments ON THE STACK, so a
 * userspace munmap-then-exit is impossible: once this routine unmaps its own
 * stack there is nowhere to push exit()'s arguments.  Instead we use a single
 * UbixOS-native call, thread_exit_unmap (int $0x81 slot 66), which unmaps the
 * region AND terminates the thread in the kernel (on the kernel stack), so the
 * vanished user stack is harmless.  The kernel reads base/size from the stack
 * during the trap (still valid) and never returns here.
 */
.text
.global __unmapself
.type   __unmapself,@function
__unmapself:
	movl 4(%esp),%ebx	/* base */
	movl 8(%esp),%ecx	/* size */
	pushl %ecx		/* arg2 = size (tf_esp+8) */
	pushl %ebx		/* arg1 = base (tf_esp+4) */
	pushl $0		/* fake return address (tf_esp+0) */
	movl $66,%eax		/* UbixOS-native slot 66 = thread_exit_unmap */
	int $0x81
	hlt			/* never returns */
