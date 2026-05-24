/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * settty — claim the serial TTY as the calling process's controlling terminal.
 * Native UbixOS syscall 48 (int $0x81).
 *
 * Analogous to FreeBSD login_tty(): setsid() + ioctl(fd, TIOCSCTTY).
 * In UbixOS, because we lack /dev/ttyS0 device nodes, this single syscall
 * sets _current->term to the specified tty_term[] slot and switches the COM1
 * ISR into ring-drain mode.
 */

/*
 * _do_settty(slot) — bare syscall thunk; slot is the first (and only) arg.
 */
asm(
  ".text                           \n"
  ".globl _do_settty               \n"
  ".type  _do_settty, @function    \n"
  "_do_settty:                     \n"
  "  movl $48, %eax                \n"
  "  int  $0x81                    \n"
  "  ret                           \n"
);

static int _do_settty(int slot);

int settty(int slot) { return _do_settty(slot); }
