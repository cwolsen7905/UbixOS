/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Generic, table-driven syscall dispatch engine.
 *
 * The two syscall vectors — native (systemCalls[], int $0x81) and POSIX
 * (systemCalls_posix[], int $0x80) — are tables of {sc_args, name, handler,
 * status}.  A handler is sys_foo(struct thread *td, struct foo_args *uap); the
 * uap is just the arguments laid out as register-width slots (ARG_COUNT =
 * sizeof(uap)/sizeof(register_t)), so an arch trampoline that has gathered the
 * raw argument words into args[] can dispatch any entry by copying sc_args words
 * into a uap buffer and calling the handler.
 *
 * This is the architecture-neutral core; each arch keeps only a thin trampoline
 * (i386 gathers args off the user stack in asm; aarch64 reads x0-x7).  It lets
 * both arches share one syscall table instead of hand-mapping per arch.
 */

#include <sys/types.h>
#include <ubixos/syscalls.h>
#include <ubixos/sched.h>
#include <ubixos/errno.h>
#include <lib/kprintf.h>

#define SYSCALL_MAX_ARGS 8

/**
 * Dispatch syscall @number against table @tbl (of @count entries) on thread
 * @td, with the raw argument words in @args (at least the entry's sc_args of
 * them).  Copies the args into a register-width uap buffer, invokes the handler,
 * and returns the value the handler left in td->td_retval[0].
 *
 * @return td_retval[0] from the handler, or -ENOSYS for an out-of-range / unset
 *         / invalid entry.
 */
register_t ksyscall_dispatch(
    struct thread *td, struct syscall_entry *tbl, int count, u_int32_t number, register_t *args)
{
	register_t uap[SYSCALL_MAX_ARGS];
	int nargs, i;

	if ((int)number < 0 || (int)number >= count)
	{
		kprintf("syscall: number %u out of range (max %d)\n", number, count);
		td->td_retval[0] = -ENOSYS;
		return (-ENOSYS);
	}
	if (tbl[number].sc_status != SYSCALL_VALID || tbl[number].sc_entry == 0)
	{
		kprintf("syscall: %u (%s) not implemented\n", number, tbl[number].sc_name ? tbl[number].sc_name : "?");
		td->td_retval[0] = -ENOSYS;
		return (-ENOSYS);
	}

	nargs = tbl[number].sc_args;
	if (nargs > SYSCALL_MAX_ARGS)
		nargs = SYSCALL_MAX_ARGS;
	for (i = 0; i < nargs; i++)
		uap[i] = args[i];

	td->td_retval[0] = 0;
	((int (*)(struct thread *, void *))tbl[number].sc_entry)(td, uap);
	return (td->td_retval[0]);
}
