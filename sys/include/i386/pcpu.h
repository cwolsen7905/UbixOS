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

#ifndef _I386_PCPU_H
#define _I386_PCPU_H

#include <sys/types.h>

#define MAXCPU 8 /* matches the cpuinfo[] pool size in smp.c */

/*
 * Per-CPU state.  One instance per logical processor lives in g_pcpu[].  This is
 * the structure the SMP scheduler will hang the running thread ("current") and
 * the per-CPU idle thread off, replacing today's single global _current.  The
 * thread pointers are void * for now to avoid a dependency on the scheduler
 * headers; they become kTask_t * when the scheduler is made per-CPU.
 */
struct pcpu {
	u_int32_t cpuid;  /* logical CPU index (0 = BSP) */
	u_int8_t apicid;  /* Local APIC id of this CPU */
	u_int8_t online;  /* 1 once this CPU has registered itself */
	void *current;    /* thread currently running on this CPU */
	void *idle;       /* this CPU's idle thread (pinned here) */
};

extern struct pcpu g_pcpu[MAXCPU];

/**
 * Logical id of the CPU the caller is running on.
 *
 * Until the scheduler is made per-CPU (g_smp_active stays 0), this always
 * returns 0 — the boot processor — so single-CPU callers never touch the LAPIC.
 * Once SMP scheduling is live it resolves the caller's Local APIC id to its
 * g_pcpu[] index.
 */
u_int32_t smp_processor_id(void);

/** Per-CPU state for the calling CPU (&g_pcpu[smp_processor_id()]). */
struct pcpu *curcpu(void);

#endif /* _I386_PCPU_H */
