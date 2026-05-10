/*
 * UbixOS i386 syscall stubs for TCC — POSIX ABI (int $0x80)
 * Syscall numbers from sys/kernel/syscalls_posix.c
 */
.text

/* read(int fd, void *buf, size_t count) — syscall 3 */
.globl read
.type  read,@function
read:
	movl	$3, %eax
	int	$0x80
	ret

/* lseek(int fd, off_t offset, int whence) — syscall 478
 * off_t is 64-bit; FreeBSD i386 ABI inserts a pad dword before offset.
 * Caller stack: [ret][fd][offset_lo][offset_hi][whence]
 * We need to push: [fd][pad=0][offset_lo][offset_hi][whence]
 * Trick: rebuild the call frame with the extra pad word.
 */
.globl lseek
.type  lseek,@function
lseek:
	/* args on stack (past ret addr): fd(4), off_lo(4), off_hi(4), whence(4) */
	pushl	%ebp
	movl	%esp, %ebp
	/* push args in FreeBSD order: whence, off_hi, off_lo, pad, fd */
	pushl	20(%ebp)	/* whence   */
	pushl	16(%ebp)	/* off_hi   */
	pushl	12(%ebp)	/* off_lo   */
	pushl	$0		/* pad      */
	pushl	8(%ebp)		/* fd       */
	movl	$478, %eax
	int	$0x80
	movl	%ebp, %esp
	popl	%ebp
	ret

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
