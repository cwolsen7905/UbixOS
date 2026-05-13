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
asm(
  ".text                           \n"
  ".globl _do_ttyctrl              \n"
  ".type  _do_ttyctrl, @function   \n"
  "_do_ttyctrl:                    \n"
  "  movl $42, %eax                \n"
  "  int  $0x81                    \n"
  "  ret                           \n"
);

static int _do_ttyctrl(int cmd, int val);

int tty_setraw(int val)  { return _do_ttyctrl(0, val); }
int tty_setecho(int val) { return _do_ttyctrl(1, val); }
