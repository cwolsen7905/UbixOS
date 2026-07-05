/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UBIXOS_SIGNAL_H
#define _UBIXOS_SIGNAL_H

#include <sys/types.h>
#include <sys/trap.h>
#include <sys/signal.h>
#include <sys/thread.h>
#include <ubixos/tty.h>
#include <machine/signal.h> /* signal numbers + (aarch64) arch sigcontext/sigframe */

struct sys_sigsuspend_args;

#if !defined(__aarch64__) && !defined(__x86_64__)
/*
 * Saved CPU context pushed onto the user stack when delivering a signal.
 * sys_sigreturn receives a pointer to this struct and restores the registers.
 */
struct ubx_sigcontext
{
	u_int32_t sc_eax;
	u_int32_t sc_ecx;
	u_int32_t sc_edx;
	u_int32_t sc_ebx;
	u_int32_t sc_esp;
	u_int32_t sc_ebp;
	u_int32_t sc_esi;
	u_int32_t sc_edi;
	u_int32_t sc_eip;
	u_int32_t sc_eflags;
	sigset_t sc_mask; /* saved sigmask, restored by sys_sigreturn */
};

/*
 * Signal frame layout on the user stack (grows downward; new_esp points here).
 *
 * [new_esp +  0]  sf_retaddr  → &sf_trampoline[0] (cdecl return from handler)
 * [new_esp +  4]  sf_signum                        (arg 1 to handler)
 * [new_esp +  8]  sf_trampoline[14]                (machine code → sys_sigreturn)
 * [new_esp + 22]  sf_pad[2]
 * [new_esp + 24]  sf_sc                            (struct ubx_sigcontext)
 */
struct ubx_sigframe
{
	u_int32_t sf_retaddr;        /*  0 */
	u_int32_t sf_signum;         /*  4 */
	u_int8_t sf_trampoline[14];  /*  8 */
	u_int8_t sf_pad[2];          /* 22 */
	struct ubx_sigcontext sf_sc; /* 24 */
};

/*
 * musl-compatible siginfo_t for user-stack signal frames (128 bytes).
 * The union payload starts at offset 12, matching musl's __si_fields layout.
 * si_pid/si_uid (kill-origin) and si_addr (fault-origin) alias at offset 12
 * inside the union — same as musl's __si_common vs __sigfault overlap.
 */
struct ubx_musl_siginfo
{
	int si_signo; /* [0]  signal number */
	int si_errno; /* [4]  errno (usually 0) */
	int si_code;  /* [8]  SI_USER / SI_KERNEL / ... */
	union
	{
		u_int8_t __pad[116]; /* [12] pad to 128 bytes total */
		struct
		{
			pid_t si_pid;  /* [12] sender pid (kill-origin) */
			uid_t si_uid;  /* [16] sender uid */
			int si_status; /* [20] exit status (SIGCHLD) */
		} __kill;
		void *si_addr; /* [12] fault address (SIGSEGV/SIGBUS) */
	} u;
};

/*
 * SA_SIGINFO signal frame (handler called with 3 args).
 *
 * [+0]   sf_retaddr   → &sf_trampoline
 * [+4]   sf_signo     arg1: signal number
 * [+8]   sf_info_ptr  arg2: &sf_info (pointer into this frame)
 * [+12]  sf_uctx_ptr  arg3: NULL (no ucontext for now)
 * [+16]  sf_info      siginfo_t, musl layout (128 bytes)
 * [+144] sf_trampoline[14]
 * [+158] sf_pad[2]
 * [+160] sf_sc        saved CPU context
 */
struct ubx_sigframe_info
{
	u_int32_t sf_retaddr;
	u_int32_t sf_signo;
	u_int32_t sf_info_ptr;
	u_int32_t sf_uctx_ptr;
	struct ubx_musl_siginfo sf_info; /* 128 bytes */
	u_int8_t sf_trampoline[14];
	u_int8_t sf_pad[2];
	struct ubx_sigcontext sf_sc;
};
#endif /* !__aarch64__ */

struct thread;

/*
 * Post signal `sig` to the task with pid `pid`.
 * Safe to call from interrupt context — only sets a bit; no locks.
 */
void signal_post(int pid, int sig);

/*
 * Post signal `sig` from sender_pid to target_pid with SI_USER metadata.
 * Used by sys_kill / sys_tkill so siginfo_t.si_pid is populated correctly.
 */
void signal_post_kill(int sender_pid, int target_pid, int sig);

/*
 * Record a synchronous fault signal for _current (SIGSEGV/SIGBUS) with fault
 * address and si_code.  The caller must call signal_check(frame) after
 * releasing any spinlocks — that delivers the frame or terminates the task.
 */
void signal_post_fault(int sig, void *fault_addr, int fault_code);

/*
 * Post signal `sig` to the highest-PID live task using terminal `term`.
 * Finds the actual foreground job when tty_foreground->owner is stale.
 */
void signal_post_tty(tty_term *term, int sig);
void signal_post_pgrp(pid_t pgrp, int sig);

/*
 * Check for pending unblocked signals and deliver one before returning
 * to user mode.  Called at the end of sys_call_posix().
 */
void signal_check(struct trapframe *frame);

/*
 * Build a signal frame on the user stack and redirect the trapframe so that
 * iret enters the user-space handler.  Called by signal_check for custom
 * handlers.
 */
void signal_deliver_frame(int sig, struct sigaction *sa, struct trapframe *frame, struct thread *td);
void signal_exec_reset(struct thread *td); /* execve: reset caught handlers to SIG_DFL (POSIX) */

/* Non-zero if @td has a pending, unblocked default-terminate signal — an
 * in-kernel blocking wait consults this to abort and let the process die
 * (so a hung socket recv is Ctrl-C / kill-able instead of wedging forever). */
int signal_fatal_pending(struct thread *td);

/*
 * Deliver pending signals when returning from a timer interrupt to ring-3.
 * Called from timerInt assembly after sched() when interrupted CS had RPL=3.
 * Handles SIG_DFL terminate and SIG_IGN; custom handlers stay pending.
 */
void signal_ast_check(void);

/*
 * Atomically replace the signal mask and sleep until an unblocked signal
 * arrives.  Always returns EINTR; restores the original mask on wakeup.
 */
int sys_sigsuspend(struct thread *td, struct sys_sigsuspend_args *args);

#endif /* _UBIXOS_SIGNAL_H */
