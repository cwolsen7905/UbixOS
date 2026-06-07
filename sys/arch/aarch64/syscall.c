/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 syscall dispatch (QEMU `virt` bring-up).
 *
 * Minimal SVC dispatcher: the EL1 sync handler (exceptions.c) decodes an EL0
 * SVC and calls aarch64_syscall(number, args) with the saved x0-x5.  Numbers
 * follow the FreeBSD ABI the i386 POSIX table already uses (write=4, exit=1,
 * getpid=20), so this dovetails with the generic syscall table once the rest of
 * the kernel is ported — for now it is a hand-rolled handful routed straight to
 * the console.  A real process exits via aarch64_el0_return().
 */

#include "bringup.h"
#include <sys/types.h>
#include <ubixos/sched.h>   /* _current, sched_yield */
#include <ubixos/endtask.h> /* endTask */

#define SYS_EXIT 1
#define SYS_FORK 2
#define SYS_WRITE 4
#define SYS_GETPID 20
#define SYS_SET_TID_ADDRESS 258

/* musl routes the Linux-compat primitives (exit_group, set_thread_area, futex)
 * through the UbixOS-native int $0x81 table by ORing in this flag (matches the
 * i386 port; see project_native_abi_threading). */
#define NATIVE_FLAG 0x8000
#define NATIVE_EXIT_GROUP 65

/**
 * Terminate the current task (shared by exit / exit_group): a scheduled user
 * task is reaped + rescheduled; a bring-up enter_el0 demo longjmps back.
 */
static void do_exit(u_int64_t code)
{
	kprintf("[kernel] EL0 process exit(%lu)\n", code);
	if (_current != 0 && _current->md.md_usp != 0)
	{
		endTask(_current->id);
		sched_yield();
	}
	else
	{
		aarch64_el0_return();
	}
}

/**
 * write(fd, buf, len): copy @len bytes from the (currently-mapped) user buffer
 * to the console.  Bring-up only — ignores @fd and does no buffer validation.
 *
 * @return number of bytes written.
 */
static u_int64_t sys_write(u_int64_t fd, u_int64_t buf, u_int64_t len)
{
	const char *p = (const char *)(uintptr_t)buf;

	(void)fd;
	for (u_int64_t i = 0; i < len; i++)
		uart_putc(p[i]);
	return len;
}

/**
 * Dispatch an EL0 syscall.  @args points at the saved x0..x5 (x0 = arg0).
 *
 * @return the value to place in the caller's x0 (exit does not return).
 */
u_int64_t aarch64_syscall(u_int64_t number, u_int64_t *args)
{
	/* Native (int $0x81-equivalent) syscalls musl ORs with NATIVE_FLAG. */
	if (number & NATIVE_FLAG)
	{
		u_int64_t n = number & ~(u_int64_t)NATIVE_FLAG;
		switch (n)
		{
			case NATIVE_EXIT_GROUP:
				do_exit(args[0]);
				return 0; /* unreachable */
			default:
				kprintf("[kernel] unimplemented native EL0 syscall #%lu\n", n);
				return (u_int64_t)-1;
		}
	}

	switch (number)
	{
		case SYS_WRITE:
			return sys_write(args[0], args[1], args[2]);

		case SYS_FORK:
			/* args is the trapframe; the child resumes here returning 0. */
			return (u_int64_t)aarch64_fork(args);

		case SYS_GETPID:
			return (_current != 0) ? (u_int64_t)_current->id : 0;

		case SYS_SET_TID_ADDRESS:
			/* musl registers a clear-on-exit TID address at startup; the return
			 * value is the caller's thread id. */
			return (_current != 0) ? (u_int64_t)_current->id : 1;

		case SYS_EXIT:
			do_exit(args[0]);
			return 0; /* unreachable */

		default:
			kprintf("[kernel] unknown EL0 syscall #%lu\n", number);
			return (u_int64_t)-1;
	}
}
