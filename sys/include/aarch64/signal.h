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

#include <sys/types.h>

/*
 * Magic EL0 return address planted in LR (x30) when a signal handler is
 * invoked.  The handler `ret`s to it; it sits outside the 39-bit EL0 VA range,
 * so the instruction fetch faults and the EL0 sync handler recognises it as
 * "the handler returned" and performs sigreturn.  This avoids needing any
 * executable user memory — aarch64 user stacks are mapped execute-never, so the
 * i386 trick of running a trampoline on the stack is not available.
 */
#define AARCH64_SIGTRAMP_RETADDR 0x0000008000005160UL

/*
 * Saved EL0 CPU context pushed onto the user stack when delivering a signal.
 * sys_sigreturn restores the trapframe from it.  It is placed at the (16-byte
 * aligned) new user SP, so scp == SP — sigreturn recovers the pointer directly
 * from the user SP at the trampoline fault, with no register or kernel state to
 * thread through (and therefore nesting-safe: each frame lives at its own SP).
 */
struct ubx_sigcontext
{
	u_int64_t sc_x[31];  /* x0..x30 */
	u_int64_t sc_sp;     /* user stack pointer (sp_el0) */
	u_int64_t sc_pc;     /* return PC (elr_el1) */
	u_int64_t sc_pstate; /* saved PSTATE (spsr_el1) */
	sigset_t sc_mask;    /* saved signal mask, restored by sigreturn */
};

/* The frame planted on the user stack at delivery (sigcontext only — the
 * handler takes its signo argument in x0, not from the frame). */
struct ubx_sigframe
{
	struct ubx_sigcontext sf_sc;
};

#endif /* _AARCH64_SIGNAL_H */
