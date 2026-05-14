/*
 * __set_thread_area.s — UbixOS i386 TLS setup
 *
 * Calls UbixOS POSIX syscall slot 351 (set_thread_area) using the
 * FreeBSD stack-based ABI: push the user_desc pointer, push a fake
 * return address, set eax=351, int $0x80.
 *
 * The kernel (sys_set_thread_area in gen_calls.c):
 *   1. Reads base_addr from the user_desc
 *   2. Writes an LDT[1] data segment descriptor with that base
 *   3. Loads %gs = 0xF (LDT entry 1, TI=1, RPL=3)
 *   4. Sets entry_number = 1 in the user_desc
 *
 * Because the kernel already loads %gs, we do not need to compute
 * and reload the selector here.  We just call the syscall and return
 * 0 on success or a negative errno on failure.
 */
.text
.global __set_thread_area
.hidden __set_thread_area
.type   __set_thread_area,@function
__set_thread_area:
	movl 4(%esp), %ecx	/* user_desc ptr (arg from caller) */
	pushl %ecx		/* FreeBSD ABI arg1 → lands at esp+4 */
	pushl $0		/* fake return address at esp+0 */
	movl $351, %eax		/* UbixOS slot 351 = set_thread_area */
	int  $0x80
	addl $8, %esp		/* restore stack */
	jnc  1f			/* CF=0: success */
	negl %eax		/* CF=1: negate errno for musl */
	ret
1:	xorl %eax, %eax		/* return 0 on success */
	ret
.size __set_thread_area, .-__set_thread_area
