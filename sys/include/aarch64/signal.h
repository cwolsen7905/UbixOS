/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 machine signal definitions (stub — arch sigcontext/trapframe added
 * with the aarch64 signal port).  Reached via <machine/signal.h>.
 */
#ifndef _AARCH64_SIGNAL_H
#define _AARCH64_SIGNAL_H

/*
 * Signal numbers are part of the (arch-independent) syscall ABI — these values
 * must match i386/signal.h exactly.  Kept in sync until the numbers are hoisted
 * into a shared header during the aarch64 signal port.
 */
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGIOT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPOLL SIGIO
#define SIGPWR 30
#define SIGUNUSED 31

/* TODO(aarch64): struct sigcontext / mcontext (GPRs, pc, pstate, sp). */

#endif /* _AARCH64_SIGNAL_H */
