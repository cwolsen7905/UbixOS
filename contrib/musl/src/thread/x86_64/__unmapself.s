/* Copyright 2011-2012 Nicholas J. Kain, licensed under standard MIT license */
/* uBixOS: FreeBSD-numbered syscalls.  Upstream's Linux numbers were 11 (munmap;
 * an obsolete slot here) and 60 (exit; umask here, which returns, so the thread
 * ran off the end of this function). */
.text
.global __unmapself
.type   __unmapself,@function
__unmapself:
	movl $73,%eax   /* SYS_munmap */
	syscall         /* munmap(arg2,arg3) */
	xor %rdi,%rdi   /* exit() args: always return success */
	movl $1,%eax    /* SYS_exit */
	syscall         /* exit(0) */
