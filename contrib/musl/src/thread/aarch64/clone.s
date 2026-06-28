// __clone(func, stack, flags, arg, ptid, tls, ctid)
//         x0,   x1,    w2,    x3,  x4,   x5,  x6

// syscall(SYS_clone, flags, stack, ptid, tls, ctid)
//         x8,        x0,    x1,    x2,   x3,  x4

.global __clone
.hidden __clone
.type   __clone,%function
__clone:
	// align stack and save func,arg
	and x1,x1,#-16
	stp x0,x3,[x1,#-16]!

	// syscall
	uxtw x0,w2
	mov x2,x4
	mov x3,x5
	mov x4,x6
	// uBixOS is FreeBSD-ABI: Linux clone (#220) is __semctl here.  Route __clone to
	// the FreeBSD rfork slot (#251); the kernel reads x0=flags/x1=stack and either
	// shares the address space (CLONE_THREAD: pthreads) or COW-copies it (posix_spawn).
	mov x8,#251 // SYS_rfork (was #220 SYS_clone — Linux number collides with __semctl)
	svc #0

	cbz x0,1f
	// parent
	ret
	// child
1:	mov x29, 0
	ldp x1,x0,[sp],#16
	blr x1
	// uBixOS is FreeBSD-ABI: Linux SYS_exit (#93) is select() here, so the original
	// `mov x8,#93` made an exiting thread call select() instead of terminating — it
	// returned and ran off the end of this stub into garbage (wild-PC SIGSEGV on
	// every pthread that returns from its start routine, e.g. ld.lld's pool workers).
	// uBixOS exit (#1) routes to endTask, which reaps just this thread and leaves the
	// shared address space mapped for its still-running siblings (tgid-gated).
	mov x8,#1 // SYS_exit (FreeBSD/uBixOS number; #93 = Linux exit = select here)
	svc #0
