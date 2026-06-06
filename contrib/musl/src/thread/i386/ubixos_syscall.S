/*
 * ubixos_syscall.S — UbixOS i386 syscall shims
 *
 * Translates musl's C-function calling convention to the UbixOS POSIX
 * syscall ABI: int $0x80 with the syscall number in eax and arguments
 * pushed on the user stack, where the kernel reads them as:
 *   frame->tf_esp + 4 = arg1
 *   frame->tf_esp + 8 = arg2
 *   ...
 *
 * On return: CF=0 → eax is the return value; CF=1 → eax is positive errno.
 * We negate errno so musl's __syscall_ret() sees the expected –errno value.
 *
 * Register save rules (i386 System V ABI):
 *   Caller-saved: eax, ecx, edx — we clobber these freely.
 *   Callee-saved: ebx, esi, edi, ebp — saved/restored when used.
 *
 * Native-ABI routing: a syscall number with bit 0x8000 set is a UbixOS-native
 * call (int $0x81), not a FreeBSD POSIX call (int $0x80) — used for Linux-compat
 * primitives (futex, exit_group, …) that have no FreeBSD number.  UBIX_INT masks
 * the flag off and dispatches to the right vector; the return convention (CF set
 * on error) is identical for both tables, so the surrounding code is unchanged.
 */

.text

.macro UBIX_INT
	testl $0x8000, %eax
	jnz   2f
	int   $0x80
	jmp   3f
2:	andl  $0x7FFF, %eax
	int   $0x81
3:
.endm

/* ------------------------------------------------------------------ */
/* __syscall0(n)                                                        */
/* ------------------------------------------------------------------ */
.global __syscall0
.type   __syscall0,@function
__syscall0:
	movl 4(%esp), %eax
	pushl $0		/* fake return addr — no args follow */
	UBIX_INT
	leal 4(%esp), %esp	/* restore stack — leal preserves CF from int $0x80 */
	jnc  1f
	negl %eax
1:	ret
.size __syscall0, .-__syscall0

/* ------------------------------------------------------------------ */
/* __syscall1(n, a1)                                                    */
/* ------------------------------------------------------------------ */
.global __syscall1
.type   __syscall1,@function
__syscall1:
	movl 4(%esp), %eax
	movl 8(%esp), %ecx	/* a1 */
	pushl %ecx
	pushl $0
	UBIX_INT
	leal 8(%esp), %esp
	jnc  1f
	negl %eax
1:	ret
.size __syscall1, .-__syscall1

/* ------------------------------------------------------------------ */
/* __syscall2(n, a1, a2)                                                */
/* ------------------------------------------------------------------ */
.global __syscall2
.type   __syscall2,@function
__syscall2:
	movl  4(%esp), %eax
	movl  8(%esp), %ecx	/* a1 */
	movl 12(%esp), %edx	/* a2 */
	pushl %edx
	pushl %ecx
	pushl $0
	UBIX_INT
	leal 12(%esp), %esp
	jnc  1f
	negl %eax
1:	ret
.size __syscall2, .-__syscall2

/* ------------------------------------------------------------------ */
/* __syscall3(n, a1, a2, a3)                                            */
/* ------------------------------------------------------------------ */
.global __syscall3
.type   __syscall3,@function
__syscall3:
	pushl %esi
	movl  8(%esp), %eax	/* n   (shifted +4 from push) */
	movl 12(%esp), %ecx	/* a1 */
	movl 16(%esp), %edx	/* a2 */
	movl 20(%esp), %esi	/* a3 */
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl $0
	UBIX_INT
	leal 16(%esp), %esp
	jnc  1f
	negl %eax
1:	popl %esi
	ret
.size __syscall3, .-__syscall3

/* ------------------------------------------------------------------ */
/* __syscall4(n, a1, a2, a3, a4)                                        */
/* ------------------------------------------------------------------ */
.global __syscall4
.type   __syscall4,@function
__syscall4:
	pushl %esi
	pushl %edi
	movl 12(%esp), %eax	/* n   (shifted +8) */
	movl 16(%esp), %ecx	/* a1 */
	movl 20(%esp), %edx	/* a2 */
	movl 24(%esp), %esi	/* a3 */
	movl 28(%esp), %edi	/* a4 */
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl $0
	UBIX_INT
	leal 20(%esp), %esp
	jnc  1f
	negl %eax
1:	popl %edi
	popl %esi
	ret
.size __syscall4, .-__syscall4

/* ------------------------------------------------------------------ */
/* __syscall5(n, a1, a2, a3, a4, a5)                                   */
/* ------------------------------------------------------------------ */
.global __syscall5
.type   __syscall5,@function
__syscall5:
	pushl %esi
	pushl %edi
	pushl %ebx
	movl 16(%esp), %eax	/* n   (shifted +12) */
	movl 20(%esp), %ecx	/* a1 */
	movl 24(%esp), %edx	/* a2 */
	movl 28(%esp), %esi	/* a3 */
	movl 32(%esp), %edi	/* a4 */
	movl 36(%esp), %ebx	/* a5 */
	pushl %ebx
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl $0
	UBIX_INT
	leal 24(%esp), %esp
	jnc  1f
	negl %eax
1:	popl %ebx
	popl %edi
	popl %esi
	ret
.size __syscall5, .-__syscall5

/* ------------------------------------------------------------------ */
/* __syscall6(n, a1, a2, a3, a4, a5, a6)                               */
/* ------------------------------------------------------------------ */
.global __syscall6
.type   __syscall6,@function
__syscall6:
	pushl %esi
	pushl %edi
	pushl %ebx
	pushl %ebp
	movl 20(%esp), %eax	/* n   (shifted +16) */
	movl 24(%esp), %ecx	/* a1 */
	movl 28(%esp), %edx	/* a2 */
	movl 32(%esp), %esi	/* a3 */
	movl 36(%esp), %edi	/* a4 */
	movl 40(%esp), %ebx	/* a5 */
	movl 44(%esp), %ebp	/* a6 */
	pushl %ebp
	pushl %ebx
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl $0
	UBIX_INT
	leal 28(%esp), %esp
	jnc  1f
	negl %eax
1:	popl %ebp
	popl %ebx
	popl %edi
	popl %esi
	ret
.size __syscall6, .-__syscall6
