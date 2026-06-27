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
	mov x8,#93 // SYS_exit
	svc #0
