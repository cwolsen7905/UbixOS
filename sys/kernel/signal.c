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
#include <i386/signal.h>
#include <lib/kprintf.h>
#include <string.h>
#include <assert.h>

/* -----------------------------------------------------------------------
 * Signals with SIG_DFL action = terminate the process.
 * Bit N-1 represents signal N (same encoding as sig_pending).
 * --------------------------------------------------------------------- */
#define SIGTERM_MASK \
    ((1u <<  0) |   /* SIGHUP  1  */ \
     (1u <<  1) |   /* SIGINT  2  */ \
     (1u <<  2) |   /* SIGQUIT 3  */ \
     (1u <<  3) |   /* SIGILL  4  */ \
     (1u <<  4) |   /* SIGTRAP 5  */ \
     (1u <<  5) |   /* SIGABRT 6  */ \
     (1u <<  6) |   /* SIGBUS  7  */ \
     (1u <<  7) |   /* SIGFPE  8  */ \
     (1u <<  8) |   /* SIGKILL 9  */ \
     (1u << 10) |   /* SIGSEGV 11 */ \
     (1u << 12) |   /* SIGPIPE 13 */ \
     (1u << 14))    /* SIGTERM 15 */

/* Signals whose SIG_DFL action is to stop (not terminate) the process. */
#define SIGSTOP_MASK \
    ((1u << (SIGTSTP  - 1)) |   /* 20 */ \
     (1u << (SIGSTOP  - 1)) |   /* 19 */ \
     (1u << (SIGTTIN  - 1)) |   /* 21 */ \
     (1u << (SIGTTOU  - 1)))    /* 22 */

/* Pending stop signals cleared by SIGCONT. */
#define SIGPENDSTOP_MASK \
    ((1u << (SIGTSTP  - 1)) | \
     (1u << (SIGSTOP  - 1)) | \
     (1u << (SIGTTIN  - 1)) | \
     (1u << (SIGTTOU  - 1)))

/*
 * signal_post — post signal `sig` to the task identified by `pid`.
 *
 * Safe to call from interrupt context: only performs an atomic OR on
 * sig_pending (volatile uint32_t).  No scheduler calls, no locks.
 */
void
signal_post(int pid, int sig)
{
    kTask_t *t;

    if (sig < 1 || sig > 31)
        return;

    t = schedFindTask((uint32_t)pid);
    if (t == NULL)
        return;

    if (sig == SIGCONT) {
        t->td.sig_pending &= ~SIGPENDSTOP_MASK;
        if (t->state == STOPPED) {
            t->t_stopped_sig = 0;
            sched_ready(t);
        }
    }
    t->td.sig_code[sig - 1] = SI_KERNEL;
    t->td.sig_pending |= (1u << (sig - 1));
}

/*
 * signal_post_kill — post signal `sig` from sender to target, with SI_USER
 * metadata (si_pid = sender).  Used by sys_kill / sys_tkill.
 */
void
signal_post_kill(int sender_pid, int target_pid, int sig)
{
    kTask_t *t;

    if (sig < 1 || sig > 31)
        return;

    t = schedFindTask((uint32_t)target_pid);
    if (t == NULL)
        return;

    t->td.sig_code[sig - 1]        = SI_USER;
    t->td.sig_extra[sig - 1].si_pid = sender_pid;
    t->td.sig_pending |= (1u << (sig - 1));
}

/*
 * signal_post_fault — post a synchronous fault signal (SIGSEGV/SIGBUS) to
 * _current, recording the fault address and si_code.  The caller must call
 * signal_check(frame) after releasing any held spinlocks.
 */
void
signal_post_fault(int sig, void *fault_addr, int fault_code)
{
    struct thread *td;

    if (_current == NULL || sig < 1 || sig > 31)
        return;

    td = &_current->td;
    td->sig_code[sig - 1]        = (uint8_t)fault_code;
    td->sig_extra[sig - 1].si_addr = fault_addr;
    td->sig_pending |= (1u << (sig - 1));
}

/*
 * signal_post_tty — post signal `sig` to the terminal's foreground process
 * group (term->t_pgrp).  Falls back to the most recently created live task on
 * the terminal when t_pgrp is 0 or no matching task exists.
 */
void
signal_post_tty(tty_term *term, int sig)
{
    kTask_t *t, *target = NULL;
    int      sent = 0;

    if (sig < 1 || sig > 31 || term == NULL)
        return;

    /* Primary: signal every task in the foreground process group. */
    if (term->t_pgrp != 0) {
        for (t = taskList; t != NULL; t = t->next) {
            if (t->state == DEAD || (pid_t)t->pgrp != term->t_pgrp)
                continue;
            t->td.sig_code[sig - 1] = SI_KERNEL;
            t->td.sig_pending |= (1u << (sig - 1));
            sent = 1;
        }
        if (sent)
            return;
    }

    /* Fallback: signal the most recently created live task on this terminal. */
    for (t = taskList; t != NULL; t = t->next) {
        if (t->term != term || t->state == DEAD)
            continue;
        if (target == NULL || t->id > target->id)
            target = t;
    }
    if (target != NULL) {
        target->td.sig_code[sig - 1] = SI_KERNEL;
        target->td.sig_pending |= (1u << (sig - 1));
    }
}

/*
 * signal_post_pgrp — post signal `sig` to every task in process group `pgrp`.
 *
 * Special handling for SIGCONT: wake any STOPPED tasks immediately so they
 * can resume (they won't reach signal_check otherwise), and discard pending
 * stop signals per POSIX.
 */
void
signal_post_pgrp(pid_t pgrp, int sig)
{
    kTask_t *t;

    if (sig < 1 || sig > 31 || pgrp == 0)
        return;
    for (t = taskList; t != NULL; t = t->next) {
        if (t->state == DEAD || (pid_t)t->pgrp != pgrp)
            continue;
        if (sig == SIGCONT) {
            /* Discard pending stop signals. */
            t->td.sig_pending &= ~SIGPENDSTOP_MASK;
            /* Wake stopped task so it can run again. */
            if (t->state == STOPPED) {
                t->t_stopped_sig = 0;
                sched_ready(t);
            }
        }
        t->td.sig_code[sig - 1] = SI_KERNEL;
        t->td.sig_pending |= (1u << (sig - 1));
    }
}

/*
 * signal_check — deliver the highest-priority pending unblocked signal.
 *
 * Called on every POSIX syscall exit, before iret returns to user mode.
 * Only handles Phase 1 actions:
 *   SIG_DFL on a terminating signal → endTask + yield
 *   SIG_IGN                         → discard
 *   Custom handler                  → Phase 2 (not yet implemented; log + discard)
 */
void
signal_check(struct trapframe *frame)
{
    struct thread    *td = &_current->td;
    uint32_t          pending, unblocked;
    int               sig;
    struct sigaction *sa;

    (void)frame;

    pending   = td->sig_pending;
    unblocked = pending & ~td->sigmask.__bits[0];

    if (unblocked == 0)
        return;

    /* Pick the lowest-numbered pending unblocked signal. */
    for (sig = 1; sig <= 31; sig++) {
        if (!(unblocked & (1u << (sig - 1))))
            continue;

        /* Clear the pending bit before delivery so we don't re-deliver. */
        td->sig_pending &= ~(1u << (sig - 1));

        sa = &td->sigact[sig];

        if ((void *)sa->sa_handler == (void *)0x1 /* SIG_IGN */) {
            /* ignored — discard */
            return;
        }

        if ((void *)sa->sa_handler == (void *)0x0 /* SIG_DFL */ ||
            sa->sa_handler == NULL) {
            /* Default action: stop for SIGSTOP_MASK, terminate for
             * SIGTERM_MASK, ignore for the rest (SIGCHLD etc.). */
            if ((1u << (sig - 1)) & SIGSTOP_MASK) {
                sched_stop(_current, sig);
                sched_yield();
                /* Resumes here after SIGCONT wakes us via sched_ready. */
                return;
            }
            if ((1u << (sig - 1)) & SIGTERM_MASK) {
                kprintf("signal: SIG_DFL terminate sig=%d pid=%d name=%s\n",
                    sig, _current->id, _current->name);
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

/* Write the sigreturn trampoline into buf[14].
 * The trampoline: push $sc_addr; push $0; mov $417,%eax; int $0x80 */
static void
write_trampoline(uint8_t *buf, uint32_t sc_addr)
{
    buf[0]  = 0x68;
    buf[1]  = (uint8_t)(sc_addr >>  0);
    buf[2]  = (uint8_t)(sc_addr >>  8);
    buf[3]  = (uint8_t)(sc_addr >> 16);
    buf[4]  = (uint8_t)(sc_addr >> 24);
    buf[5]  = 0x6A; buf[6]  = 0x00;
    buf[7]  = 0xB8;
    buf[8]  = 0xA1; buf[9]  = 0x01; buf[10] = 0x00; buf[11] = 0x00;
    buf[12] = 0xCD; buf[13] = 0x80;
}

/* Save the interrupted CPU state into *sc and update td's sigmask. */
static void
save_sigcontext(struct ubx_sigcontext *sc, struct trapframe *frame,
    struct thread *td, struct sigaction *sa, int sig)
{
    uint32_t sc_num = frame->tf_err & 0xFFFF;
    uint32_t sc_err = (frame->tf_err >> 16) & 0xFFFF;

    sc->sc_eax    = (uint32_t)frame->tf_eax;
    sc->sc_ecx    = (uint32_t)frame->tf_ecx;
    sc->sc_edx    = (uint32_t)frame->tf_edx;
    sc->sc_ebx    = (uint32_t)frame->tf_ebx;
    sc->sc_esp    = (uint32_t)frame->tf_esp;
    sc->sc_ebp    = (uint32_t)frame->tf_ebp;
    sc->sc_esi    = (uint32_t)frame->tf_esi;
    sc->sc_edi    = (uint32_t)frame->tf_edi;
    sc->sc_eip    = (uint32_t)frame->tf_eip;
    sc->sc_eflags = (uint32_t)frame->tf_eflags;
    memcpy(&sc->sc_mask, &td->sigmask, sizeof(sigset_t));

    /* SA_RESTART: re-execute the interrupted syscall after the handler. */
    if ((sa->sa_flags & SA_RESTART) && sc_err == EINTR && sc_num != 0) {
        sc->sc_eip = (uint32_t)frame->tf_eip - 2; /* point back to int $0x80 */
        sc->sc_eax = sc_num;                       /* restore original syscall# */
    }

    td->sigmask.__bits[0] |= sa->sa_mask.__bits[0];
    if (!(sa->sa_flags & SA_NODEFER))
        td->sigmask.__bits[0] |= (1u << (sig - 1));
}

/*
 * signal_deliver_frame — build a signal frame on the user stack and redirect
 * the trapframe so that iret enters the user-space handler.
 *
 * Non-SA_SIGINFO: handler(int sig)
 *   [+0]  retaddr → trampoline; [+4] signum; [+8] trampoline[14]; [+24] sc
 *
 * SA_SIGINFO: handler(int sig, siginfo_t *info, void *uctx)
 *   [+0]  retaddr → trampoline; [+4] signum; [+8] &info; [+12] NULL
 *   [+16] siginfo_t (128 bytes, musl layout); [+144] trampoline[14]; [+160] sc
 */
void
signal_deliver_frame(int sig, struct sigaction *sa, struct trapframe *frame,
    struct thread *td)
{
    uint32_t new_esp, sc_addr;

    if (sa->sa_flags & SA_SIGINFO) {
        struct ubx_sigframe_info *fp;
        uint32_t info_addr;

        new_esp   = ((uint32_t)frame->tf_esp -
            (uint32_t)sizeof(struct ubx_sigframe_info)) & ~3u;
        fp        = (struct ubx_sigframe_info *)new_esp;
        sc_addr   = new_esp +
            (uint32_t)__builtin_offsetof(struct ubx_sigframe_info, sf_sc);
        info_addr = new_esp +
            (uint32_t)__builtin_offsetof(struct ubx_sigframe_info, sf_info);

        write_trampoline(fp->sf_trampoline, sc_addr);
        fp->sf_retaddr  = new_esp +
            (uint32_t)__builtin_offsetof(struct ubx_sigframe_info, sf_trampoline);
        fp->sf_signo    = (uint32_t)sig;
        fp->sf_info_ptr = info_addr;
        fp->sf_uctx_ptr = 0;

        memset(&fp->sf_info, 0, sizeof(fp->sf_info));
        fp->sf_info.si_signo = sig;
        fp->sf_info.si_errno = 0;
        fp->sf_info.si_code  = (int)(uint8_t)td->sig_code[sig - 1];
        if (fp->sf_info.si_code == SI_USER) {
            fp->sf_info.u.__kill.si_pid = td->sig_extra[sig - 1].si_pid;
        } else {
            fp->sf_info.u.si_addr = td->sig_extra[sig - 1].si_addr;
        }

        save_sigcontext(&fp->sf_sc, frame, td, sa, sig);
        frame->tf_esp = (int)new_esp;
        frame->tf_eip = (int)(uintptr_t)sa->sa_sigaction;
    } else {
        struct ubx_sigframe *fp;

        new_esp = ((uint32_t)frame->tf_esp -
            (uint32_t)sizeof(struct ubx_sigframe)) & ~3u;
        fp      = (struct ubx_sigframe *)new_esp;
        sc_addr = new_esp +
            (uint32_t)__builtin_offsetof(struct ubx_sigframe, sf_sc);

        write_trampoline(fp->sf_trampoline, sc_addr);
        fp->sf_retaddr = new_esp +
            (uint32_t)__builtin_offsetof(struct ubx_sigframe, sf_trampoline);
        fp->sf_signum  = (uint32_t)sig;

        save_sigcontext(&fp->sf_sc, frame, td, sa, sig);
        frame->tf_esp = (int)new_esp;
        frame->tf_eip = (int)(uintptr_t)sa->sa_handler;
    }
}

/*
 * sys_sigreturn — restore saved CPU state after a signal handler returns.
 * Called by the trampoline on the user stack via int $0x80 (slot 417).
 */
int
sys_sigreturn(struct thread *td, struct sys_sigreturn_args *args)
{
    struct ubx_sigcontext *scp = args->scp;
    struct trapframe      *frame = td->frame;

    if (scp == NULL) {
        td->td_retval[0] = -1;
        return (-1);
    }

    /* Restore saved registers. */
    frame->tf_eax    = (int)scp->sc_eax;
    frame->tf_ecx    = (int)scp->sc_ecx;
    frame->tf_edx    = (int)scp->sc_edx;
    frame->tf_ebx    = (int)scp->sc_ebx;
    frame->tf_esp    = (int)scp->sc_esp;
    frame->tf_ebp    = (int)scp->sc_ebp;
    frame->tf_esi    = (int)scp->sc_esi;
    frame->tf_edi    = (int)scp->sc_edi;
    frame->tf_eip    = (int)scp->sc_eip;
    frame->tf_eflags = (int)(scp->sc_eflags & ~0x200u); /* keep IF clear in kernel */

    /* Restore sigmask. */
    memcpy(&td->sigmask, &scp->sc_mask, sizeof(sigset_t));

    td->td_retval[0] = (int)scp->sc_eax;
    td->td_retval[1] = (int)scp->sc_edx;
    return (0);
}

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
void
signal_ast_check(void)
{
    struct thread    *td;
    uint32_t          pending, unblocked;
    int               sig;
    struct sigaction *sa;

    if (_current == NULL)
        return;

    td        = &_current->td;
    pending   = td->sig_pending;
    unblocked = pending & ~td->sigmask.__bits[0];
    if (unblocked == 0)
        return;

    for (sig = 1; sig <= 31; sig++) {
        if (!(unblocked & (1u << (sig - 1))))
            continue;

        sa = &td->sigact[sig];

        if ((void *)sa->sa_handler == (void *)0x1) { /* SIG_IGN */
            td->sig_pending &= ~(1u << (sig - 1));
            return;
        }

        if ((void *)sa->sa_handler == (void *)0x0 || sa->sa_handler == NULL) {
            if ((1u << (sig - 1)) & SIGTERM_MASK) {
                td->sig_pending &= ~(1u << (sig - 1));
                kprintf("signal: AST SIG_DFL terminate sig=%d pid=%d name=%s\n",
                    sig, _current->id, _current->name);
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

/*
 * sys_sigsuspend — atomically replace sigmask and sleep until an unblocked
 * signal arrives.  Always returns EINTR; restores the original mask on wakeup.
 *
 * FreeBSD slot 179; musl rt_sigsuspend also uses 179 — no musl patch needed.
 */
int
sys_sigsuspend(struct thread *td, struct sys_sigsuspend_args *args)
{
    sigset_t saved;

    if (args->sigmask == NULL) {
        td->td_retval[0] = -1;
        return (EINVAL);
    }

    memcpy(&saved, &td->sigmask, sizeof(sigset_t));
    memcpy(&td->sigmask, args->sigmask, sizeof(sigset_t));
    /* SIGKILL is never maskable. */
    td->sigmask.__bits[0] &= ~(1u << (SIGKILL - 1));

    /* Yield until an unblocked signal arrives. */
    while ((td->sig_pending & ~td->sigmask.__bits[0]) == 0)
        sched_yield();

    memcpy(&td->sigmask, &saved, sizeof(sigset_t));
    td->td_retval[0] = -1;  /* always returns -1/EINTR */
    return (EINTR);
}

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
