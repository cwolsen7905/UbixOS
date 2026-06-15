/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
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

#include <sys/signal.h>
#include <sys/sysproto_posix.h>
#include <sys/thread.h>
#include <sys/trap.h>
#include <sys/errno.h>
#include <ubixos/signal.h>
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <ubixos/endtask.h>
#include <ubixos/tty.h>
#include <machine/signal.h>
#include <lib/kprintf.h>
#include <string.h>
#include <assert.h>

/* -----------------------------------------------------------------------
 * Signals with SIG_DFL action = terminate the process.
 * Bit N-1 represents signal N (same encoding as sig_pending).
 * --------------------------------------------------------------------- */
#define SIGTERM_MASK                                                                                                   \
	((1u << 0) |  /* SIGHUP  1  */                                                                                 \
	 (1u << 1) |  /* SIGINT  2  */                                                                                 \
	 (1u << 2) |  /* SIGQUIT 3  */                                                                                 \
	 (1u << 3) |  /* SIGILL  4  */                                                                                 \
	 (1u << 4) |  /* SIGTRAP 5  */                                                                                 \
	 (1u << 5) |  /* SIGABRT 6  */                                                                                 \
	 (1u << 6) |  /* SIGBUS  7  */                                                                                 \
	 (1u << 7) |  /* SIGFPE  8  */                                                                                 \
	 (1u << 8) |  /* SIGKILL 9  */                                                                                 \
	 (1u << 10) | /* SIGSEGV 11 */                                                                                 \
	 (1u << 12) | /* SIGPIPE 13 */                                                                                 \
	 (1u << 14))  /* SIGTERM 15 */

/* Signals whose SIG_DFL action is to stop the process.
 * SIGTTIN/SIGTTOU are intentionally excluded: they are already handled
 * as EINTR in the TTY read/write paths.  Adding them here would stop
 * boot-time daemons permanently when no job-control shell exists to
 * send SIGCONT.  Re-add once a full job-control shell is wired up. */
#define SIGSTOP_MASK                                                                                                   \
	((1u << (SIGTSTP - 1)) | /* 20 — Ctrl+Z */                                                                     \
	 (1u << (SIGSTOP - 1)))  /* 19 — unconditional stop */

/* Pending stop signals cleared by SIGCONT. */
#define SIGPENDSTOP_MASK ((1u << (SIGTSTP - 1)) | (1u << (SIGSTOP - 1)) | (1u << (SIGTTIN - 1)) | (1u << (SIGTTOU - 1)))

/**
 * signal_post - enqueue a kernel signal for a task
 * @pid: target task ID
 * @sig: signal number to deliver (1-31)
 *
 * This helper posts a signal to the task identified by @pid. It only
 * updates `sig_pending`, `sig_code`, and control bits, so it is safe
 * to call from interrupt context without taking locks or invoking the
 * scheduler.
 */
void signal_post(int pid, int sig)
{
	kTask_t *t;

	if (sig < 1 || sig > 31)
		return;

	t = schedFindTask((u_int32_t)pid);
	if (t == NULL)
		return;

	if (sig == SIGCONT)
	{
		t->td.sig_pending &= ~SIGPENDSTOP_MASK;
		if (t->state == STOPPED)
		{
			t->t_stopped_sig = 0;
			sched_ready(t);
		}
	}
	t->td.sig_code[sig - 1] = SI_KERNEL;
	t->td.sig_pending |= (1u << (sig - 1));
}

/**
 * signal_post_kill - enqueue a user-generated signal for a task
 * @sender_pid: sending process ID
 * @target_pid: receiving process ID
 * @sig: signal number to deliver (1-31)
 *
 * Posts a signal to @target_pid and marks the source as a user signal
 * with `SI_USER`. Used by `sys_kill` and `sys_tkill` to preserve the
 * sending PID in `sig_extra[].si_pid`.
 */
void signal_post_kill(int sender_pid, int target_pid, int sig)
{
	kTask_t *t;

	if (sig < 1 || sig > 31)
		return;

	t = schedFindTask((u_int32_t)target_pid);
	if (t == NULL)
		return;

	t->td.sig_code[sig - 1] = SI_USER;
	t->td.sig_extra[sig - 1].si_pid = sender_pid;
	t->td.sig_pending |= (1u << (sig - 1));
}

/**
 * signal_post_fault - record a synchronous fault signal for the current task
 * @sig: fault signal number (SIGSEGV or SIGBUS)
 * @fault_addr: address that caused the fault
 * @fault_code: architecture-specific fault code to store in `si_code`
 *
 * Marks the current task as having a pending synchronous fault signal.
 * This is used for page faults and other CPU traps that should be raised
 * as a process signal. The caller must release any held locks before
 * calling `signal_check()` so signal delivery can proceed safely.
 */
void signal_post_fault(int sig, void *fault_addr, int fault_code)
{
	struct thread *td;

	if (_current == NULL || sig < 1 || sig > 31)
		return;

	td = &_current->td;
	td->sig_code[sig - 1] = (u_int8_t)fault_code;
	td->sig_extra[sig - 1].si_addr = fault_addr;
	td->sig_pending |= (1u << (sig - 1));
}

/**
 * signal_post_tty - deliver a terminal-generated signal to the fg process group
 * @term: terminal state structure
 * @sig: signal number to deliver
 *
 * Sends `sig` to every task in the terminal's foreground process group.
 * If no foreground process group exists, it falls back to the most recently
 * created live task attached to the terminal.
 */
void signal_post_tty(tty_term *term, int sig)
{
	kTask_t *t, *target = NULL;
	int sent = 0;

	if (sig < 1 || sig > 31 || term == NULL)
		return;

	/* Primary: signal every task in the foreground process group. */
	if (term->t_pgrp != 0)
	{
		for (t = taskList; t != NULL; t = t->next)
		{
			if (t->state == DEAD || (pid_t)t->pgrp != term->t_pgrp)
				continue;
			t->td.sig_code[sig - 1] = SI_KERNEL;
			t->td.sig_pending |= (1u << (sig - 1));
			sent = 1;
		}
		if (sent)
			return;
	}

	/* Fallback: signal the most recently created live task on this terminal.
	 * A task is "on this terminal" if either its inherited term pointer or its
	 * controlling-terminal pointer matches.  Pty children acquire only ct_tty
	 * (via opening /dev/ttyvN after setsid), so checking both is required. */
	for (t = taskList; t != NULL; t = t->next)
	{
		if (t->state == DEAD)
			continue;
		if (t->term != term && t->ct_tty != term)
			continue;
		if (target == NULL || t->id > target->id)
			target = t;
	}
	if (target != NULL)
	{
		target->td.sig_code[sig - 1] = SI_KERNEL;
		target->td.sig_pending |= (1u << (sig - 1));
	}
}

/**
 * signal_post_pgrp - post a signal to all tasks in a process group
 * @pgrp: process group ID
 * @sig: signal number to deliver
 *
 * Sends @sig to every live task whose process group matches @pgrp. If the
 * signal is `SIGCONT`, any stopped tasks are also resumed immediately and
 * pending stop signals are cleared in accordance with POSIX semantics.
 */
void signal_post_pgrp(pid_t pgrp, int sig)
{
	kTask_t *t;

	if (sig < 1 || sig > 31 || pgrp == 0)
		return;
	for (t = taskList; t != NULL; t = t->next)
	{
		if (t->state == DEAD || (pid_t)t->pgrp != pgrp)
			continue;
		if (sig == SIGCONT)
		{
			/* Discard pending stop signals. */
			t->td.sig_pending &= ~SIGPENDSTOP_MASK;
			/* Wake stopped task so it can run again. */
			if (t->state == STOPPED)
			{
				t->t_stopped_sig = 0;
				sched_ready(t);
			}
		}
		t->td.sig_code[sig - 1] = SI_KERNEL;
		t->td.sig_pending |= (1u << (sig - 1));
	}
}

/**
 * signal_check - check pending signals and deliver the highest-priority one
 * @frame: trapframe from the interrupted user context
 *
 * Called before returning to user mode from a syscall. This routine finds
 * the lowest-numbered pending signal that is not masked and delivers it.
 * Default actions for `SIG_DFL`, ignored signals, and custom handlers are
 * handled here; custom handler delivery is deferred to `signal_deliver_frame`.
 */
void signal_check(struct trapframe *frame)
{
	struct thread *td = &_current->td;
	u_int32_t pending, unblocked;
	int sig;
	struct sigaction *sa;

	(void)frame;

	pending = td->sig_pending;
	unblocked = pending & ~td->sigmask.__bits[0];

	if (unblocked == 0)
		return;

	kprintf("signal_check: pid=%d name=%s pending=0x%X mask=0x%X unblocked=0x%X\n",
	        _current->id,
	        _current->name,
	        pending,
	        td->sigmask.__bits[0],
	        unblocked);

	/* Pick the lowest-numbered pending unblocked signal. */
	for (sig = 1; sig <= 31; sig++)
	{
		if (!(unblocked & (1u << (sig - 1))))
			continue;

		/* Clear the pending bit before delivery so we don't re-deliver. */
		td->sig_pending &= ~(1u << (sig - 1));

		sa = &td->sigact[sig];

		if ((void *)sa->sa_handler == (void *)0x1 /* SIG_IGN */)
		{
			/* ignored — discard */
			return;
		}

		kprintf("signal_check: sig=%d handler=0x%X\n", sig, (u_int32_t)sa->sa_handler);

		if ((void *)sa->sa_handler == (void *)0x0 /* SIG_DFL */ || sa->sa_handler == NULL)
		{
			/* Default action: stop for SIGSTOP_MASK, terminate for
			 * SIGTERM_MASK, ignore for the rest (SIGCHLD etc.). */
			if ((1u << (sig - 1)) & SIGSTOP_MASK)
			{
				sched_stop(_current, sig);
				sched_yield();
				/* Resumes here after SIGCONT wakes us via sched_ready. */
				return;
			}
			if ((1u << (sig - 1)) & SIGTERM_MASK)
			{
				kprintf("signal: SIG_DFL terminate sig=%d pid=%d name=%s\n",
				        sig,
				        _current->id,
				        _current->name);
				endTask(_current->id);
				sched_yield();
				/* not reached */
			}
			return;
		}

		/* Custom handler — deliver via signal frame. */
		signal_deliver_frame(sig, sa, frame, td);

		/* SA_RESETHAND: reset to SIG_DFL after first delivery. */
		if (sa->sa_flags & SA_RESETHAND)
			memset(&td->sigact[sig], 0, sizeof(struct sigaction));
		return;
	}
}

#if defined(__i386__)

/**
 * write_trampoline - emit the sigreturn trampoline into a stack frame
 * @buf: buffer to write the trampoline bytes into
 * @sc_addr: address of the saved sigcontext structure
 *
 * The trampoline code pushes the sigcontext address and a zero return
 * value, then executes syscall 417 via `int $0x80` to invoke `sys_sigreturn`.
 */
static void write_trampoline(u_int8_t *buf, u_int32_t sc_addr)
{
	buf[0] = 0x68;
	buf[1] = (u_int8_t)(sc_addr >> 0);
	buf[2] = (u_int8_t)(sc_addr >> 8);
	buf[3] = (u_int8_t)(sc_addr >> 16);
	buf[4] = (u_int8_t)(sc_addr >> 24);
	buf[5] = 0x6A;
	buf[6] = 0x00;
	buf[7] = 0xB8;
	buf[8] = 0xA1;
	buf[9] = 0x01;
	buf[10] = 0x00;
	buf[11] = 0x00;
	buf[12] = 0xCD;
	buf[13] = 0x80;
}

/**
 * save_sigcontext - capture the interrupted user CPU state for signal return
 * @sc: saved context structure to populate
 * @frame: trapframe from the interrupted user context
 * @td: thread state for the current task
 * @sa: signal action being delivered
 * @sig: signal number being delivered
 *
 * Saves registers, flags, and the current signal mask so the signal
 * handler can return to the interrupted syscall or user code correctly.
 * It also applies the handler's mask and deferred signal semantics.
 */
static void save_sigcontext(
    struct ubx_sigcontext *sc, struct trapframe *frame, struct thread *td, struct sigaction *sa, int sig)
{
	u_int32_t sc_num = frame->tf_err & 0xFFFF;
	u_int32_t sc_err = (frame->tf_err >> 16) & 0xFFFF;

	sc->sc_eax = (u_int32_t)frame->tf_eax;
	sc->sc_ecx = (u_int32_t)frame->tf_ecx;
	sc->sc_edx = (u_int32_t)frame->tf_edx;
	sc->sc_ebx = (u_int32_t)frame->tf_ebx;
	sc->sc_esp = (u_int32_t)frame->tf_esp;
	sc->sc_ebp = (u_int32_t)frame->tf_ebp;
	sc->sc_esi = (u_int32_t)frame->tf_esi;
	sc->sc_edi = (u_int32_t)frame->tf_edi;
	sc->sc_eip = (u_int32_t)frame->tf_eip;
	sc->sc_eflags = (u_int32_t)frame->tf_eflags;
	/* When interrupted from sigsuspend, restore the pre-sigsuspend mask. */
	if (td->td_pflags & TDP_OLDMASK)
	{
		memcpy(&sc->sc_mask, &td->td_oldsigmask, sizeof(sigset_t));
		td->td_pflags &= ~TDP_OLDMASK;
	}
	else
	{
		memcpy(&sc->sc_mask, &td->sigmask, sizeof(sigset_t));
	}

	/* SA_RESTART: re-execute the interrupted syscall after the handler. */
	if ((sa->sa_flags & SA_RESTART) && sc_err == EINTR && sc_num != 0)
	{
		sc->sc_eip = (u_int32_t)frame->tf_eip - 2; /* point back to int $0x80 */
		sc->sc_eax = sc_num;                       /* restore original syscall# */
	}

	td->sigmask.__bits[0] |= sa->sa_mask.__bits[0];
	if (!(sa->sa_flags & SA_NODEFER))
		td->sigmask.__bits[0] |= (1u << (sig - 1));
}

/**
 * signal_deliver_frame - construct a user stack frame for a signal handler
 * @sig: signal number being delivered
 * @sa: action describing the handler and flags
 * @frame: trapframe for the interrupted user context
 * @td: task/thread state for the current task
 *
 * Builds either an SA_SIGINFO or standard signal frame on the user stack,
 * writes the sigreturn trampoline, and adjusts `frame->tf_esp` and
 * `frame->tf_eip` so execution enters the signal handler on return.
 */
void signal_deliver_frame(int sig, struct sigaction *sa, struct trapframe *frame, struct thread *td)
{
	u_int32_t new_esp, sc_addr;

	if (sa->sa_flags & SA_SIGINFO)
	{
		struct ubx_sigframe_info *fp;
		u_int32_t info_addr;

		new_esp = ((u_int32_t)frame->tf_esp - (u_int32_t)sizeof(struct ubx_sigframe_info)) & ~3u;
		fp = (struct ubx_sigframe_info *)new_esp;
		sc_addr = new_esp + (u_int32_t) __builtin_offsetof(struct ubx_sigframe_info, sf_sc);
		info_addr = new_esp + (u_int32_t) __builtin_offsetof(struct ubx_sigframe_info, sf_info);

		write_trampoline(fp->sf_trampoline, sc_addr);
		fp->sf_retaddr = new_esp + (u_int32_t) __builtin_offsetof(struct ubx_sigframe_info, sf_trampoline);
		fp->sf_signo = (u_int32_t)sig;
		fp->sf_info_ptr = info_addr;
		fp->sf_uctx_ptr = 0;

		memset(&fp->sf_info, 0, sizeof(fp->sf_info));
		fp->sf_info.si_signo = sig;
		fp->sf_info.si_errno = 0;
		fp->sf_info.si_code = (int)(u_int8_t)td->sig_code[sig - 1];
		if (fp->sf_info.si_code == SI_USER)
		{
			fp->sf_info.u.__kill.si_pid = td->sig_extra[sig - 1].si_pid;
		}
		else
		{
			fp->sf_info.u.si_addr = td->sig_extra[sig - 1].si_addr;
		}

		save_sigcontext(&fp->sf_sc, frame, td, sa, sig);
		frame->tf_esp = (int)new_esp;
		frame->tf_eip = (int)(uintptr_t)sa->sa_sigaction;
	}
	else
	{
		struct ubx_sigframe *fp;

		new_esp = ((u_int32_t)frame->tf_esp - (u_int32_t)sizeof(struct ubx_sigframe)) & ~3u;
		fp = (struct ubx_sigframe *)new_esp;
		sc_addr = new_esp + (u_int32_t) __builtin_offsetof(struct ubx_sigframe, sf_sc);

		write_trampoline(fp->sf_trampoline, sc_addr);
		fp->sf_retaddr = new_esp + (u_int32_t) __builtin_offsetof(struct ubx_sigframe, sf_trampoline);
		fp->sf_signum = (u_int32_t)sig;

		save_sigcontext(&fp->sf_sc, frame, td, sa, sig);
		frame->tf_esp = (int)new_esp;
		frame->tf_eip = (int)(uintptr_t)sa->sa_handler;
	}
}

/**
 * sys_sigreturn - restore user CPU state after a signal handler finishes
 * @td: thread state for the calling task
 * @args: syscall arguments containing the saved sigcontext pointer
 *
 * This syscall is invoked by the signal trampoline generated by
 * `write_trampoline`. It restores saved registers and signal mask, then
 * returns control to the interrupted user context.
 */
int sys_sigreturn(struct thread *td, struct sys_sigreturn_args *args)
{
	struct ubx_sigcontext *scp = args->scp;
	struct trapframe *frame = td->frame;

	kprintf("sys_sigreturn: pid=%d scp=%p eip=0x%X esp=0x%X\n",
	        _current->id,
	        scp,
	        scp ? scp->sc_eip : 0,
	        scp ? scp->sc_esp : 0);

	if (scp == NULL)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	/* Restore saved registers. */
	frame->tf_eax = (int)scp->sc_eax;
	frame->tf_ecx = (int)scp->sc_ecx;
	frame->tf_edx = (int)scp->sc_edx;
	frame->tf_ebx = (int)scp->sc_ebx;
	frame->tf_esp = (int)scp->sc_esp;
	frame->tf_ebp = (int)scp->sc_ebp;
	frame->tf_esi = (int)scp->sc_esi;
	frame->tf_edi = (int)scp->sc_edi;
	frame->tf_eip = (int)scp->sc_eip;
	frame->tf_eflags = (int)(scp->sc_eflags | 0x200u); /* IF must be set for user mode */

	/* Restore sigmask. */
	memcpy(&td->sigmask, &scp->sc_mask, sizeof(sigset_t));

	td->td_retval[0] = (int)scp->sc_eax;
	td->td_retval[1] = (int)scp->sc_edx;
	return (0);
}

#elif defined(__aarch64__)

/**
 * signal_deliver_frame - construct an EL0 signal-handler frame (aarch64)
 *
 * Carves a 16-byte-aligned sigcontext off the user stack, saves the interrupted
 * EL0 register state into it, then redirects the trapframe so KERNEL_EXIT ERETs
 * straight into the handler: x0 = signo (x1/x2 = NULL siginfo/ucontext for
 * SA_SIGINFO), LR = the magic return address (its later fetch faults and the
 * EL0 sync handler turns that into sigreturn), SP = the new frame, PC = handler.
 *
 * SA_RESTART is not yet honoured: an interrupted syscall returns its result
 * (e.g. -EINTR) and the program retries — sufficient for Ctrl-C/SIGWINCH.
 */
void signal_deliver_frame(int sig, struct sigaction *sa, struct trapframe *frame, struct thread *td)
{
	u_int64_t new_sp = (frame->tf_sp - (u_int64_t)sizeof(struct ubx_sigframe)) & ~(u_int64_t)15;
	struct ubx_sigframe *fp = (struct ubx_sigframe *)(uintptr_t)new_sp;
	struct ubx_sigcontext *sc = &fp->sf_sc;

	for (int i = 0; i < 31; i++)
		sc->sc_x[i] = frame->tf_x[i];
	sc->sc_sp = frame->tf_sp;
	sc->sc_pc = frame->tf_elr;
	sc->sc_pstate = frame->tf_spsr;

	/* When interrupted from sigsuspend, embed the pre-sigsuspend mask. */
	if (td->td_pflags & TDP_OLDMASK)
	{
		memcpy(&sc->sc_mask, &td->td_oldsigmask, sizeof(sigset_t));
		td->td_pflags &= ~TDP_OLDMASK;
	}
	else
	{
		memcpy(&sc->sc_mask, &td->sigmask, sizeof(sigset_t));
	}

	/* Block sa_mask (and, unless SA_NODEFER, this signal) for the handler. */
	td->sigmask.__bits[0] |= sa->sa_mask.__bits[0];
	if (!(sa->sa_flags & SA_NODEFER))
		td->sigmask.__bits[0] |= (1u << (sig - 1));

	frame->tf_x[0] = (u_int64_t)(unsigned)sig;
	frame->tf_x[1] = 0;
	frame->tf_x[2] = 0;
	frame->tf_x[30] = AARCH64_SIGTRAMP_RETADDR;
	frame->tf_sp = new_sp;
	frame->tf_elr =
	    (sa->sa_flags & SA_SIGINFO) ? (u_int64_t)(uintptr_t)sa->sa_sigaction : (u_int64_t)(uintptr_t)sa->sa_handler;
}

/**
 * sys_sigreturn - restore the interrupted EL0 context after a handler (aarch64)
 *
 * Invoked from the EL0 sync handler when a returning signal handler faults on
 * the magic LR; @args->scp points at the sigcontext (== the user SP at the
 * fault).  Restores the saved registers/PC/PSTATE/SP into td->frame so the
 * trap-return epilogue ERETs back to the originally-interrupted code.
 */
int sys_sigreturn(struct thread *td, struct sys_sigreturn_args *args)
{
	struct ubx_sigcontext *scp = args->scp;
	struct trapframe *frame = td->frame;

	if (scp == NULL || frame == NULL)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	for (int i = 0; i < 31; i++)
		frame->tf_x[i] = scp->sc_x[i];
	frame->tf_sp = scp->sc_sp;
	frame->tf_elr = scp->sc_pc;
	frame->tf_spsr = scp->sc_pstate;

	memcpy(&td->sigmask, &scp->sc_mask, sizeof(sigset_t));

	/* x0 (tf_x[0]) carries the resumed user return value, restored above. */
	td->td_retval[0] = (int)frame->tf_x[0];
	return (0);
}

#elif defined(__x86_64__)

/**
 * signal_deliver_frame - construct a ring-3 signal-handler frame (x86_64)
 *
 * Carves a 16-byte-aligned sigcontext off the user stack (saving the interrupted
 * GP regs + RIP/RFLAGS/RSP), plants the magic return address just below it, and
 * redirects the trapframe so the SYSRET/IRET return enters the handler: RDI =
 * signo, RSP = the new frame (so the handler's `ret` pops the magic address ->
 * faults -> sigreturn, with the user RSP then pointing at the sigcontext), RIP =
 * handler.  No on-stack trampoline (the user stack need not be executable).
 */
void signal_deliver_frame(int sig, struct sigaction *sa, struct trapframe *frame, struct thread *td)
{
	u_int64_t sc_addr = (frame->tf_rsp - (u_int64_t)sizeof(struct ubx_sigcontext)) & ~(u_int64_t)15;
	u_int64_t new_rsp = sc_addr - 8; /* the magic return address sits here */
	struct ubx_sigcontext *sc = (struct ubx_sigcontext *)(uintptr_t)sc_addr;

	sc->sc_rax = frame->tf_rax;
	sc->sc_rbx = frame->tf_rbx;
	sc->sc_rcx = frame->tf_rcx;
	sc->sc_rdx = frame->tf_rdx;
	sc->sc_rsi = frame->tf_rsi;
	sc->sc_rdi = frame->tf_rdi;
	sc->sc_rbp = frame->tf_rbp;
	sc->sc_r8 = frame->tf_r8;
	sc->sc_r9 = frame->tf_r9;
	sc->sc_r10 = frame->tf_r10;
	sc->sc_r11 = frame->tf_r11;
	sc->sc_r12 = frame->tf_r12;
	sc->sc_r13 = frame->tf_r13;
	sc->sc_r14 = frame->tf_r14;
	sc->sc_r15 = frame->tf_r15;
	sc->sc_rip = frame->tf_rip;
	sc->sc_rflags = frame->tf_rflags;
	sc->sc_rsp = frame->tf_rsp;

	if (td->td_pflags & TDP_OLDMASK)
	{
		memcpy(&sc->sc_mask, &td->td_oldsigmask, sizeof(sigset_t));
		td->td_pflags &= ~TDP_OLDMASK;
	}
	else
	{
		memcpy(&sc->sc_mask, &td->sigmask, sizeof(sigset_t));
	}

	td->sigmask.__bits[0] |= sa->sa_mask.__bits[0];
	if (!(sa->sa_flags & SA_NODEFER))
		td->sigmask.__bits[0] |= (1u << (sig - 1));

	*(u_int64_t *)(uintptr_t)new_rsp = X86_64_SIGTRAMP_RETADDR;

	frame->tf_rdi = (u_int64_t)(unsigned)sig; /* handler's first arg */
	frame->tf_rsp = new_rsp;
	frame->tf_rip =
	    (sa->sa_flags & SA_SIGINFO) ? (u_int64_t)(uintptr_t)sa->sa_sigaction : (u_int64_t)(uintptr_t)sa->sa_handler;
}

/**
 * sys_sigreturn - restore the interrupted ring-3 context after a handler (x86_64)
 *
 * Invoked from the #PF handler when a returning handler faults on the magic
 * return address; @args->scp == the user RSP at the fault (the sigcontext).
 * Restores the saved registers/RIP/RFLAGS/RSP into td->frame so the return
 * epilogue resumes the originally-interrupted code.
 */
int sys_sigreturn(struct thread *td, struct sys_sigreturn_args *args)
{
	struct ubx_sigcontext *scp = args->scp;
	struct trapframe *frame = td->frame;

	if (scp == NULL || frame == NULL)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	frame->tf_rax = scp->sc_rax;
	frame->tf_rbx = scp->sc_rbx;
	frame->tf_rcx = scp->sc_rcx;
	frame->tf_rdx = scp->sc_rdx;
	frame->tf_rsi = scp->sc_rsi;
	frame->tf_rdi = scp->sc_rdi;
	frame->tf_rbp = scp->sc_rbp;
	frame->tf_r8 = scp->sc_r8;
	frame->tf_r9 = scp->sc_r9;
	frame->tf_r10 = scp->sc_r10;
	frame->tf_r11 = scp->sc_r11;
	frame->tf_r12 = scp->sc_r12;
	frame->tf_r13 = scp->sc_r13;
	frame->tf_r14 = scp->sc_r14;
	frame->tf_r15 = scp->sc_r15;
	frame->tf_rip = scp->sc_rip;
	frame->tf_rflags = scp->sc_rflags;
	frame->tf_rsp = scp->sc_rsp;

	memcpy(&td->sigmask, &scp->sc_mask, sizeof(sigset_t));

	td->td_retval[0] = (int)frame->tf_rax;
	return (0);
}

#endif /* arch signal-frame delivery */

/**
 * sys_sigprocmask - manipulate the calling thread's signal mask
 * @td: thread state for the calling task
 * @args: syscall arguments containing the old/new masks and operation
 *
 * Implements the `sigprocmask` syscall. It can return the current mask,
 * replace it, block additional signals, or unblock signals depending on
 * `args->how`.
 */
int sys_sigprocmask(struct thread *td, struct sys_sigprocmask_args *args)
{
	td->td_retval[0] = -1;

	if (args->oset != 0x0)
	{
		memcpy(args->oset, &td->sigmask, sizeof(sigset_t));
		td->td_retval[0] = 0x0;
	}

	if (args->set != 0x0)
	{
		if (args->how == SIG_SETMASK)
		{
			if (args->set != 0x0)
			{
				memcpy(&td->sigmask, args->set, sizeof(sigset_t));
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
		}
		else if (args->how == SIG_BLOCK)
		{
			if (args->set != 0x0)
			{
				td->sigmask.__bits[0] |= args->set->__bits[0];
				td->sigmask.__bits[1] |= args->set->__bits[1];
				td->sigmask.__bits[2] |= args->set->__bits[2];
				td->sigmask.__bits[3] |= args->set->__bits[3];
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
		}
		else if (args->how == SIG_UNBLOCK)
		{
			if (args->set != 0x0)
			{
				td->sigmask.__bits[0] &= ~args->set->__bits[0];
				td->sigmask.__bits[1] &= ~args->set->__bits[1];
				td->sigmask.__bits[2] &= ~args->set->__bits[2];
				td->sigmask.__bits[3] &= ~args->set->__bits[3];
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
		}
		else
		{
			kprintf("SPM: 0x%X", args->how);
			td->td_retval[0] = -1;
		}
	}

	return (0);
}

/*
 * signal_ast_check — deliver pending unblocked signals when returning from a
 * timer interrupt to user mode (ring-3).  Called by timerInt after sched()
 * when the interrupted CS had RPL=3.
 *
 * Handles SIG_DFL terminate and SIG_IGN only.  Custom handlers are left
 * pending; they will be delivered at the next POSIX syscall exit via
 * signal_check().  (Full register state for custom-handler frame delivery
 * from AST context requires additional assembly scaffolding.)
 */
void signal_ast_check(void)
{
	struct thread *td;
	u_int32_t pending, unblocked;
	int sig;
	struct sigaction *sa;

	if (_current == NULL)
		return;

	td = &_current->td;
	pending = td->sig_pending;
	unblocked = pending & ~td->sigmask.__bits[0];
	if (unblocked == 0)
		return;

	for (sig = 1; sig <= 31; sig++)
	{
		if (!(unblocked & (1u << (sig - 1))))
			continue;

		sa = &td->sigact[sig];

		if ((void *)sa->sa_handler == (void *)0x1)
		{ /* SIG_IGN */
			td->sig_pending &= ~(1u << (sig - 1));
			return;
		}

		if ((void *)sa->sa_handler == (void *)0x0 || sa->sa_handler == NULL)
		{
			if ((1u << (sig - 1)) & SIGSTOP_MASK)
			{
				td->sig_pending &= ~(1u << (sig - 1));
				/* sched_stop + yield — returns when SIGCONT wakes us. */
				sched_stop(_current, sig);
				sched_yield();
				return;
			}
			if ((1u << (sig - 1)) & SIGTERM_MASK)
			{
				td->sig_pending &= ~(1u << (sig - 1));
				kprintf("signal: AST SIG_DFL terminate sig=%d pid=%d name=%s\n",
				        sig,
				        _current->id,
				        _current->name);
				endTask(_current->id);
				sched_yield();
				/* not reached */
			}
			/* SIG_DFL ignore (e.g. SIGCHLD) */
			td->sig_pending &= ~(1u << (sig - 1));
			return;
		}

		/* Custom handler: leave pending for syscall delivery. */
		return;
	}
}

/**
 * sys_sigsuspend - atomically replace the current signal mask and wait
 * @td: thread state for the calling task
 * @args: syscall arguments containing the new signal mask
 *
 * Replaces the caller's signal mask with `args->sigmask` and suspends the
 * task until an unblocked signal becomes pending. The original mask is
 * preserved in `td->td_oldsigmask` and restored by `sys_sigreturn`.
 * This syscall always returns `-1` with `EINTR`.
 *
 * FreeBSD slot 179; musl rt_sigsuspend also uses 179.
 */
int sys_sigsuspend(struct thread *td, struct sys_sigsuspend_args *args)
{
	if (args->sigmask == NULL)
	{
		td->td_retval[0] = -1;
		return (EINVAL);
	}

	/*
	 * Save the pre-sigsuspend mask so save_sigcontext can embed it in the
	 * signal frame.  sigreturn will restore it, giving the correct POSIX
	 * atomicity: the original mask is restored after the handler runs, not
	 * before (which would miss signals that arrive between restore and iret).
	 */
	memcpy(&td->td_oldsigmask, &td->sigmask, sizeof(sigset_t));
	td->td_pflags |= TDP_OLDMASK;

	memcpy(&td->sigmask, args->sigmask, sizeof(sigset_t));
	td->sigmask.__bits[0] &= ~(1u << (SIGKILL - 1));

	/* Spin (yielding) until an unblocked signal arrives. */
	while ((td->sig_pending & ~td->sigmask.__bits[0]) == 0)
		sched_yield();

	/*
	 * Do NOT restore td->sigmask here.  We leave the pause mask installed so
	 * that signal_check (called at syscall exit) sees the pending signal as
	 * unblocked and calls signal_deliver_frame.  save_sigcontext will embed
	 * td_oldsigmask in the signal frame; sigreturn restores it, so the
	 * original mask is back in place after the handler returns.
	 */
	td->td_retval[0] = -1; /* sigsuspend always returns -1/EINTR */
	return (EINTR);
}

/**
 * sys_sigaction - examine or change signal action for the calling task
 * @td: thread state for the calling task
 * @args: syscall arguments containing signal number, new action, and old action
 *
 * If `args->oact` is non-null, stores the current handler for `args->sig`
 * into that structure. If `args->act` is non-null, installs the new
 * handler from that structure. Returns 0 on success or -1 on invalid input.
 */
int sys_sigaction(struct thread *td, struct sys_sigaction_args *args)
{
	td->td_retval[0] = -1;

	if (args->sig < 1 || (size_t)args->sig >= (sizeof(td->sigact) / sizeof(td->sigact[0])))
	{
		return (0);
	}

	if (args->oact != 0x0)
	{
		memcpy(args->oact, &td->sigact[args->sig], sizeof(struct sigaction));
		td->td_retval[0] = 0;
	}

	if (args->act != 0x0)
	{
		memcpy(&td->sigact[args->sig], args->act, sizeof(struct sigaction));
		td->td_retval[0] = 0;
	}
	return (0);
}
