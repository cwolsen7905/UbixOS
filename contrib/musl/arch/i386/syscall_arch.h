/*
 * syscall_arch.h — UbixOS i386 port (FreeBSD stack-based ABI)
 *
 * UbixOS POSIX syscalls use int $0x80 with the FreeBSD i386 stack calling
 * convention: syscall number in eax, arguments on the user stack starting
 * at esp+4 (past a dummy "return address" word at esp+0).
 *
 * musl's normal i386 port uses Linux register ABI (ebx=a1, ecx=a2, …).
 * We replace the inline asm with calls to assembly shims that build the
 * right stack frame before issuing int $0x80.
 *
 * On return from int $0x80:
 *   CF=0 → eax is the return value (success)
 *   CF=1 → eax is the errno (positive); shims negate it so musl sees –errno
 */

#define __SYSCALL_LL_E(x) \
((union { long long ll; long l[2]; }){ .ll = x }).l[0], \
((union { long long ll; long l[2]; }){ .ll = x }).l[1]
#define __SYSCALL_LL_O(x) __SYSCALL_LL_E((x))

/*
 * Always use int $0x80 — never the TLS dispatch (call *%%gs:16).
 * SYSCALL_NO_TLS=1 keeps the fallback "int $128" path, which is int $0x80.
 */
#define SYSCALL_NO_TLS 1

/* Implemented in src/thread/i386/ubixos_syscall.S */
long __syscall0(long n);
long __syscall1(long n, long a1);
long __syscall2(long n, long a1, long a2);
long __syscall3(long n, long a1, long a2, long a3);
long __syscall4(long n, long a1, long a2, long a3, long a4);
long __syscall5(long n, long a1, long a2, long a3, long a4, long a5);
long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6);

#define VDSO_USEFUL
#define VDSO_CGT32_SYM "__vdso_clock_gettime"
#define VDSO_CGT32_VER "LINUX_2.6"
#define VDSO_CGT_SYM "__vdso_clock_gettime64"
#define VDSO_CGT_VER "LINUX_2.6"
