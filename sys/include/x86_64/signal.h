/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 signal-frame layout (Phase 5e).  Reached via <machine/signal.h>.
 * Like aarch64, delivery uses a "magic" return address rather than an on-stack
 * trampoline: the handler's return pops it, the fetch faults, and the ring-3
 * #PF handler turns that into sys_sigreturn.  Sibling of aarch64/signal.h.
 */
#ifndef _X86_64_SIGNAL_H
#define _X86_64_SIGNAL_H

/* Signal numbers are part of the (arch-independent) syscall ABI — must match
 * i386/signal.h + aarch64/signal.h exactly. */
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

/* An unmapped user VA the handler "returns" to; its fetch faults -> sigreturn. */
#define X86_64_SIGTRAMP_RETADDR 0x0000700000000B16UL

/*
 * Saved ring-3 CPU context pushed onto the user stack at signal delivery.  Placed
 * 16-byte aligned just above the magic return address, so on the handler's `ret`
 * the user RSP lands exactly at the sigcontext — sys_sigreturn recovers the
 * pointer from that RSP, with nothing to thread through the kernel (nesting-safe).
 */
struct ubx_sigcontext
{
	u_int64_t sc_rax, sc_rbx, sc_rcx, sc_rdx, sc_rsi, sc_rdi, sc_rbp;
	u_int64_t sc_r8, sc_r9, sc_r10, sc_r11, sc_r12, sc_r13, sc_r14, sc_r15;
	u_int64_t sc_rip;    /* return RIP */
	u_int64_t sc_rflags; /* saved RFLAGS */
	u_int64_t sc_rsp;    /* user RSP */
	sigset_t sc_mask;    /* saved signal mask, restored by sigreturn */
};

/* The frame planted at the new user SP: a return slot (= the magic address) the
 * handler's `ret` pops, then the sigcontext.  The handler takes its signo arg in
 * %rdi, not from the frame. */
struct ubx_sigframe
{
	u_int64_t sf_retaddr;        /* = X86_64_SIGTRAMP_RETADDR */
	struct ubx_sigcontext sf_sc; /* user RSP lands here after `ret` */
};

#endif /* _X86_64_SIGNAL_H */
