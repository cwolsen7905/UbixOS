/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * ubix_set_hostname(const char *name) - set the machine hostname (uname
 * nodename, what gethostname() reads) via native syscall 70 (int $0x81 / svc).
 * Bare asm thunk: the name pointer is already in the first arg register, so the
 * kernel reads it straight from struct sys_set_hostname_args.  Returns 0 on
 * success, -1 on error.
 */

#include <sys/ubix_syscall.h>
UBIX_NATIVE_THUNK(ubix_set_hostname, 70);
