/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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

#endif /* END _SYS_SIGNAL_H */
