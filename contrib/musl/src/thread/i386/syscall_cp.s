/*
 * syscall_cp.s — UbixOS i386 cancellable syscall shim
 *
 * Signature (matches musl internal calling convention):
 *   long __syscall_cp_asm(volatile int *cancel,
 *                         long n, long a1, long a2,
 *                         long a3, long a4, long a5, long a6)
 *
 * Uses the UbixOS FreeBSD stack-based ABI for the actual int $0x80.
 *
 * __cp_begin / __cp_end mark the window where a pending cancellation
 * signal may be delivered; the signal handler checks whether EIP is in
 * [__cp_begin, __cp_end) and redirects to __cp_cancel if so.
 */
.text
.global __cp_begin
.hidden __cp_begin
.global __cp_end
.hidden __cp_end
.global __cp_cancel
.hidden __cp_cancel
.hidden __cancel
.global __syscall_cp_asm
.hidden __syscall_cp_asm
.type   __syscall_cp_asm,@function

__syscall_cp_asm:
	/* Save callee-saved registers we will use for arg loading */
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp
	/* Stack after 4 saves (+16 bytes):
	 *  20(%esp) = cancel ptr  (was 4(%esp))
	 *  24(%esp) = n
	 *  28(%esp) = a1
	 *  32(%esp) = a2
	 *  36(%esp) = a3
	 *  40(%esp) = a4
	 *  44(%esp) = a5
	 *  48(%esp) = a6
	 */
	movl 20(%esp), %ecx	/* cancel ptr */
__cp_begin:
	movl (%ecx), %eax
	testl %eax, %eax
	jnz  __cp_cancel

	/* Load syscall number and all args into registers */
	movl 24(%esp), %eax	/* n */
	movl 28(%esp), %ecx	/* a1 */
	movl 32(%esp), %edx	/* a2 */
	movl 36(%esp), %esi	/* a3 */
	movl 40(%esp), %edi	/* a4 */
	movl 44(%esp), %ebx	/* a5 */
	movl 48(%esp), %ebp	/* a6 */

	/* Build FreeBSD syscall stack frame (push in reverse order) */
	pushl %ebp		/* a6 */
	pushl %ebx		/* a5 */
	pushl %edi		/* a4 */
	pushl %esi		/* a3 */
	pushl %edx		/* a2 */
	pushl %ecx		/* a1 */
	pushl $0		/* fake return address */
	int  $0x80
__cp_end:
	addl $28, %esp		/* pop the 7 words we pushed */
	jnc  1f
	negl %eax		/* CF=1: negate errno for musl */
1:	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	ret

__cp_cancel:
	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	jmp  __cancel
.size __syscall_cp_asm, .-__syscall_cp_asm
