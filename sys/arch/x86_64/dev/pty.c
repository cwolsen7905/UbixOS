/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 pseudo-terminal syscalls — the kernel half of the graphical terminal.
 *
 * bin/views/term runs the user's shell on a kernel pty (the VT100 cell-grid line
 * discipline in sys/posix/tty.c): pty_alloc() hands out a slot, the term app feeds
 * keystrokes with tty_inject_user() and reads the rendered 80x25 grid back with
 * tty_snapshot(), and SIGWINCH/resize flow through tty_resize().  These thin
 * syscall wrappers delegate to that arch-neutral engine — the i386 equivalents
 * live in sys/kern/fb.c (which x86_64 does not compile, having split the display
 * syscalls into dev/display.c + dev/input.c).  Sibling of aarch64's pty path.
 */

#include "../x86_64.h"
#include <sys/types.h>
#include <sys/sysproto.h>
#include <sys/thread.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>

/**
 * sys_settty (native 48) — bind the calling process to a tty slot (the text
 * console path; the GUI terminal uses the pty pool instead).  Slots 0-3 are VGA
 * virtual consoles, 4+ are serial.  @return 0 on success, -1 for a bad slot.
 */
int sys_settty(struct thread *td, struct sys_settty_args *args)
{
	tty_term *t;

	if (args->slot < 0 || args->slot >= TTY_MAX_TERMS)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	t = tty_find((u_int16_t)args->slot);
	t->t_type = (args->slot <= 3) ? TTY_TYPE_VGA : TTY_TYPE_SERIAL;
	t->t_echo = 1;
	t->t_raw = 0;
	t->stdinSize = 0;
	t->t_linelen = 0;

	_current->term = t;

	td->td_retval[0] = 0;
	return (0);
}

/**
 * sys_ptyalloc (native 55) — allocate a pseudo-terminal slot from the pty pool.
 * @return the slot index, or -1 if the pool is exhausted.
 */
int sys_ptyalloc(struct thread *td, struct sys_ptyalloc_args *args)
{
	(void)args;
	td->td_retval[0] = pty_alloc();
	return (0);
}

/** sys_ptyfree (native 56) — release a pseudo-terminal slot back to the pool. */
int sys_ptyfree(struct thread *td, struct sys_ptyfree_args *args)
{
	pty_free(args->slot);
	td->td_retval[0] = 0;
	return (0);
}

/**
 * sys_ptyinject (native 57) — push a userland keystroke buffer through a pty's
 * line discipline (keyboard -> shell stdin).  @return bytes injected, or -1.
 */
int sys_ptyinject(struct thread *td, struct sys_ptyinject_args *args)
{
	td->td_retval[0] = tty_inject_user(args->slot, args->buf, args->n);
	return (0);
}

/**
 * sys_ptysnap (native 58) — copy a pty's cell grid + cursor to userland.
 * @return 0 on success, -1 for an invalid slot.
 */
int sys_ptysnap(struct thread *td, struct sys_ptysnap_args *args)
{
	td->td_retval[0] = tty_snapshot(args->slot, args->dst, args->x, args->y);
	return (0);
}

/**
 * sys_ptyresize (native 61) — set a pty's grid to cols x rows + update its
 * winsize.  The caller SIGWINCHes its shell afterwards.
 */
int sys_ptyresize(struct thread *td, struct sys_ptyresize_args *args)
{
	td->td_retval[0] = tty_resize(args->slot, args->cols, args->rows);
	return (0);
}
