/*
 * __unmapself(base, size) — unmap the exiting (DETACHED) thread's own
 * stack+TLS mapping, then exit, without touching the stack in between.
 *
 * NOT yet functional on UbixOS and only reached by detached threads (joinable
 * threads are freed by their joiner, so the pthread_create+join path never gets
 * here).  The UbixOS syscall ABI passes arguments ON THE STACK, but this routine
 * is unmapping the very stack it runs on — after munmap there is no stack to
 * push the exit() argument onto, so the stack-based ABI cannot express
 * "munmap then exit".  The proper fix is a single kernel-assisted syscall that
 * unmaps the region and terminates the thread in one trap.  TODO(threads-F/G):
 * add that native call and rewrite this.  munmap=73 (not Linux 91) noted for
 * when it lands.
 */
.text
.global __unmapself
.type   __unmapself,@function
__unmapself:
	movl 4(%esp),%ebx	/* base */
	movl 8(%esp),%ecx	/* size */
	pushl %ecx		/* munmap arg2 = size  (tf_esp+8) */
	pushl %ebx		/* munmap arg1 = base  (tf_esp+4) */
	pushl $0		/* fake return address (tf_esp+0) */
	movl $73,%eax		/* SYS_munmap (FreeBSD/UbixOS) */
	int $0x80
	/* If we reach here the stack we just unmapped may be gone; exit anyway.
	 * This is the broken window documented above — fine until a detached
	 * thread actually exercises it. */
	pushl $0		/* exit code */
	pushl $0		/* fake return address */
	movl $1,%eax		/* SYS_exit */
	int $0x80
