/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * tty_setraw / tty_setecho — UbixOS native TTY control syscall (int $0x81, slot 42).
 *
 * sys_ttyctrl_args: { int cmd; int val; }
 * tty_setraw(val)  calls with cmd=0, val=val
 * tty_setecho(val) calls with cmd=1, val=val
 *
 * We route through a common helper whose call frame already has cmd at
 * [esp+4] and val at [esp+8], which is exactly what the kernel reads.
 */

/*
 * do_ttyctrl(cmd, val) — bare syscall thunk, args already on stack.
 */
#include "ubix_syscall.h"
UBIX_NATIVE_THUNK(_do_ttyctrl, 42);

static int _do_ttyctrl(int cmd, int val);

int tty_setraw(int val)
{
	return _do_ttyctrl(0, val);
}
int tty_setecho(int val)
{
	return _do_ttyctrl(1, val);
}
