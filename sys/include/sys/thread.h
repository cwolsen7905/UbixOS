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

#ifndef _SYS_THREAD_H
#define _SYS_THREAD_H

#include <sys/types.h>
#include <sys/trap.h>
#include <sys/signal.h>
#include <sys/resource.h>

//#define O_FILES 64
#define O_FILES 512

struct thread {
    int td_retval[2];
    void *o_files[O_FILES];
    u_long vm_daddr;
    u_long vm_dsize;
    u_long vm_taddr;
    u_long vm_tsize;
    struct trapframe *frame;
    int abi;
    sigset_t sigmask;
    struct  sigaction sigact[128];
    struct rlimit rlim[RLIM_NLIMITS];
    volatile uint32_t sig_pending; /* bitmask of pending signals (bit N-1 = signal N) */

    /* Per-signal metadata for siginfo_t population (indexed by signal N-1). */
    uint8_t sig_code[32];          /* SI_USER, SI_KERNEL, SEGV_MAPERR, etc. */
    union {
        int   si_pid;              /* kill: sender pid (SI_USER) */
        void *si_addr;             /* fault: fault address (SIGSEGV/SIGBUS) */
    } sig_extra[32];
};

#endif /* END _SYS_THREAD_H */
