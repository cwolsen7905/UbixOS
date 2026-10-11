.global __unmapself
.type   __unmapself,%function
__unmapself:
	// uBixOS (FreeBSD-numbered) syscalls.  Upstream's Linux numbers were #215
	// (munmap; unimplemented here) and #93 (exit; select here): an exiting
	// detached thread called select(fd=x0, in=x1...) and faulted on the bogus
	// pointer, killing the whole process (ld.lld at the end of every link).
	mov x8,#73 // SYS_munmap
	svc 0
	mov x8,#1 // SYS_exit
	svc 0
