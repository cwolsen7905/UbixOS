.text
.global __clone
.hidden __clone
.type   __clone,@function
__clone:
	xor %eax,%eax
	# uBixOS is FreeBSD-ABI: Linux clone (56) collides; route __clone to the FreeBSD
	# rfork slot (251). The kernel reads rdi=flags/rsi=stack and either shares the
	# address space (CLONE_THREAD: pthreads) or COW-copies it (posix_spawn).
	mov $251,%al
	mov %rdi,%r11
	mov %rdx,%rdi
	mov %r8,%rdx
	mov %r9,%r8
	mov 8(%rsp),%r10
	mov %r11,%r9
	and $-16,%rsi
	sub $8,%rsi
	mov %rcx,(%rsi)
	syscall
	test %eax,%eax
	jnz 1f
	xor %ebp,%ebp
	pop %rdi
	call *%r9
	mov %eax,%edi
	xor %eax,%eax
	# uBixOS is FreeBSD-ABI: Linux SYS_exit (60) is not exit here, so the original
	# `mov $60,%al` made an exiting thread mis-syscall and run off this stub into
	# garbage.  uBixOS exit (1) routes to endTask, which reaps just this thread and
	# leaves the shared address space mapped for its siblings (tgid-gated).
	mov $1,%al
	syscall
	hlt
1:	ret
