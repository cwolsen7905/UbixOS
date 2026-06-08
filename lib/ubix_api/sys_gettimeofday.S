/* _sys_gettimeofday(tv, tz) — POSIX gettimeofday, syscall 116 (int $0x80 /
 * plain SVC table). */
#ifdef __aarch64__
.text
.globl _sys_gettimeofday
.type  _sys_gettimeofday, %function
_sys_gettimeofday:
	mov	x8, #116	/* tv in x0, tz in x1 already */
	svc	#0
	ret
.size _sys_gettimeofday, . - _sys_gettimeofday
#else
.text
.globl _sys_gettimeofday
.type  _sys_gettimeofday, @function
_sys_gettimeofday:
	movl $116, %eax
	int  $0x80
	ret
#endif
