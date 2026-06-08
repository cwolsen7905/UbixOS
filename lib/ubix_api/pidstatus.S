/* pidStatus — POSIX syscall 7 (int $0x80 / plain SVC table). */
#ifdef __aarch64__
.text
.globl pidStatus
.type  pidStatus, %function
pidStatus:
	mov	x8, #7		/* args already in x0..x5 */
	svc	#0
	ret
.size pidStatus, . - pidStatus
#else
.text
.globl pidStatus
.type  pidStatus, @function
pidStatus:
	movl	$7, %eax
	int	$0x80
	ret
.size pidStatus, . - pidStatus
#endif
