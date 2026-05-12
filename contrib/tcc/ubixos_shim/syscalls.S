/*
 * UbixOS i386 syscall stubs for TCC — POSIX ABI (int $0x80)
 * Syscall numbers from sys/kernel/syscalls_posix.c
 */
.text

/* mprotect(void *addr, size_t len, int prot) — syscall 74 (NOTIMP → returns 0) */
.globl mprotect
.type  mprotect,@function
mprotect:
	xorl	%eax, %eax
	ret

/* munmap(void *addr, size_t len) — syscall 73 */
.globl munmap
.type  munmap,@function
munmap:
	movl	$73, %eax
	int	$0x80
	ret

/* sigaction(int sig, const struct sigaction *act, struct sigaction *oact) */
.globl sigaction
.type  sigaction,@function
sigaction:
	xorl	%eax, %eax	/* stub — always succeeds */
	ret

/* sigemptyset(sigset_t *set) */
.globl sigemptyset
.type  sigemptyset,@function
sigemptyset:
	movl	4(%esp), %eax
	movl	$0, (%eax)
	xorl	%eax, %eax
	ret

/* execvp(const char *file, char *const argv[]) — stub via execve */
.globl execvp
.type  execvp,@function
execvp:
	xorl	%eax, %eax
	decl	%eax		/* return -1 */
	ret
