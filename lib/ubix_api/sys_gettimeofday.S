/* gettime() — returns Unix timestamp via gettimeofday (POSIX syscall 116).
 * Calls _sys_gettimeofday(tv, tz) with both args on the stack. */

.text
.globl _sys_gettimeofday
.type  _sys_gettimeofday, @function
_sys_gettimeofday:
	movl $116, %eax
	int  $0x80
	ret
