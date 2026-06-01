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

#ifndef _SYS_RESOURCE_H_
#define _SYS_RESOURCE_H_

#include <sys/_types.h>
#include <sys/_timeval.h>

/* Who values for getrusage(). */
#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)

struct rusage {
	struct timeval ru_utime;   /* user time used */
	struct timeval ru_stime;   /* system time used */
	long ru_maxrss;            /* max resident set size */
	long ru_ixrss;             /* integral shared memory size */
	long ru_idrss;             /* integral unshared data size */
	long ru_isrss;             /* integral unshared stack size */
	long ru_minflt;            /* page reclaims */
	long ru_majflt;            /* page faults */
	long ru_nswap;             /* swaps */
	long ru_inblock;           /* block input operations */
	long ru_oublock;           /* block output operations */
	long ru_msgsnd;            /* messages sent */
	long ru_msgrcv;            /* messages received */
	long ru_nsignals;          /* signals received */
	long ru_nvcsw;             /* voluntary context switches */
	long ru_nivcsw;            /* involuntary context switches */
};

#ifndef _RLIM_T_DECLARED
typedef __rlim_t        rlim_t;
#define _RLIM_T_DECLARED
#endif


/*
 * Resource limits
 */
#define RLIMIT_CPU      0               /* maximum cpu time in seconds */
#define RLIMIT_FSIZE    1               /* maximum file size */
#define RLIMIT_DATA     2               /* data size */
#define RLIMIT_STACK    3               /* stack size */
#define RLIMIT_CORE     4               /* core file size */
#define RLIMIT_RSS      5               /* resident set size */
#define RLIMIT_MEMLOCK  6               /* locked-in-memory address space */
#define RLIMIT_NPROC    7               /* number of processes */
#define RLIMIT_NOFILE   8               /* number of open files */
#define RLIMIT_SBSIZE   9               /* maximum size of all socket buffers */
#define RLIMIT_VMEM     10              /* virtual process size (incl. mmap) */
#define RLIMIT_AS       RLIMIT_VMEM     /* standard name for RLIMIT_VMEM */
#define RLIMIT_NPTS     11              /* pseudo-terminals */
#define RLIMIT_SWAP     12              /* swap used */
#define RLIMIT_KQUEUES  13              /* kqueues allocated */
#define RLIMIT_UMTXP    14              /* process-shared umtx */

#define RLIM_NLIMITS    15              /* number of resource limits */


/*
 * Resource limit string identifiers
 */


struct rlimit {
        rlim_t  rlim_cur;               /* current (soft) limit */
        rlim_t  rlim_max;               /* maximum value for rlim_cur */
};

#endif /* !_SYS_RESOURCE_H_ */
