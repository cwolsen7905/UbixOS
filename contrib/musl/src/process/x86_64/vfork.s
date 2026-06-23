.global vfork
.type vfork,@function
vfork:
	# uBixOS: alias vfork to fork.  Upstream musl issues a raw clone syscall
	# here (Linux x86_64 SYS_clone=58), which collides with FreeBSD's readlink
	# under uBixOS's FreeBSD ABI.  Tail-calling fork() reuses the proven fork
	# path; COW makes the copy cheap, so vfork's shared-address-space semantics
	# aren't needed (callers only do async-signal-safe ops before exec).
	jmp fork
