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

#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H

#include <sys/types.h>

#define _SIG_WORDS      4
#define _SIG_MAXSIG     128
#define _SIG_IDX(sig)   ((sig) - 1)
#define _SIG_WORD(sig)  (_SIG_IDX(sig) >> 5)
#define _SIG_BIT(sig)   (1 << (_SIG_IDX(sig) & 31))
#define _SIG_VALID(sig) ((sig) <= _SIG_MAXSIG && (sig) > 0)

// Flags for sigprocmask:
#define SIG_BLOCK       1       /* block specified signal set */
#define SIG_UNBLOCK     2       /* unblock specified signal set */
#define SIG_SETMASK     3       /* set specified signal set */

union sigval {
        /* Members as suggested by Annex C of POSIX 1003.1b. */
        int     sival_int;
        void    *sival_ptr;
        /* 6.0 compatibility */
        int     sigval_int;
        void    *sigval_ptr;
};

typedef struct __siginfo {
        int     si_signo;               /* signal number */
        int     si_errno;               /* errno association */
        /*
         * Cause of signal, one of the SI_ macros or signal-specific
         * values, i.e. one of the FPE_... values for SIGFPE.  This
         * value is equivalent to the second argument to an old-style
         * FreeBSD signal handler.
         */
        int     si_code;                /* signal code */
        __pid_t si_pid;                 /* sending process */
        __uid_t si_uid;                 /* sender's ruid */
        int     si_status;              /* exit value */
        void    *si_addr;               /* faulting instruction */
        union sigval si_value;          /* signal value */
        union   {
                struct {
                        int     _trapno;/* machine specific trap code */
                } _fault;
                struct {
                        int     _timerid;
                        int     _overrun;
                } _timer;
                struct {
                        int     _mqd;
                } _mesgq;
                struct {
                        long    _band;          /* band event for SIGPOLL */
                } _poll;                        /* was this ever used ? */
                struct {
                        long    __spare1__;
                        int     __spare2__[7];
                } __spare__;
        } _reason;
} siginfo_t;


// Signal vector "template" used in sigaction call.
struct sigaction {
        union {
                void    (*__sa_handler)(int);
                void    (*__sa_sigaction)(int, struct __siginfo *, void *);
        } __sigaction_u;                /* signal handler */
        int     sa_flags;               /* see signal options below */
        sigset_t sa_mask;               /* signal mask to apply */
};

#define sa_handler      __sigaction_u.__sa_handler
#define sa_sigaction    __sigaction_u.__sa_sigaction

/* sa_flags values (FreeBSD-compatible) */
#define SA_ONSTACK      0x0001  /* deliver on alternate signal stack */
#define SA_RESTART      0x0002  /* restart syscall on signal return */
#define SA_RESETHAND    0x0004  /* reset to SIG_DFL after delivery */
#define SA_NOCLDSTOP    0x0008  /* no SIGCHLD when child stops */
#define SA_NODEFER      0x0010  /* don't mask signal during handler */
#define SA_NOCLDWAIT    0x0020  /* no zombies on child death */
#define SA_SIGINFO      0x0040  /* handler gets siginfo_t arg */

/* si_code values — match musl/Linux so musl-compiled programs compare correctly */
#define SI_USER         0       /* kill() or raise() */
#define SI_KERNEL       128     /* kernel-generated (musl value; FreeBSD uses 0x10000) */
#define SI_QUEUE        (-1)    /* sigqueue() */
#define SI_TIMER        (-2)    /* timer expiry */
#define SI_TKILL        (-6)    /* tkill / tgkill */

/* si_code values for SIGSEGV */
#define SEGV_MAPERR     1       /* address not mapped */
#define SEGV_ACCERR     2       /* invalid permissions for mapped object */

/* si_code values for SIGBUS */
#define BUS_ADRALN      1       /* invalid address alignment */

/* SIG_DFL / SIG_IGN — stored in sa_handler */
#define SIG_DFL         ((void (*)(int))0)
#define SIG_IGN         ((void (*)(int))1)
#define SIG_ERR         ((void (*)(int))-1)

#endif /* END _SYS_SIGNAL_H */
