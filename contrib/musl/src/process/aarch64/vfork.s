.global vfork
.type vfork,%function
vfork:
	// uBixOS: alias vfork to fork.  Upstream musl issues a raw clone syscall
	// here (Linux aarch64 SYS_clone=220), which collides with FreeBSD's
	// __semctl under uBixOS's FreeBSD ABI.  Tail-calling fork() reuses the
	// proven fork path (syscall 2 -> aarch64_fork, which sets up the child's
	// trapframe correctly); COW makes the copy cheap, so we don't need vfork's
	// shared-address-space semantics — callers only do async-signal-safe ops
	// before exec.
	b fork
