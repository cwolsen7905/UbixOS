/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 machine-dependent per-process state embedded in kTask_t (the 'md'
 * field).  No hardware TSS: a software context frame.  Sibling of i386/proc.h;
 * reached via <machine/proc.h>.
 */
#ifndef _AARCH64_PROC_H
#define _AARCH64_PROC_H

#include <sys/types.h>

struct md_proc
{
	u_int64_t md_kstack; /* saved kernel SP (callee-saved frame; aarch64_ctx_switch) */
	u_int64_t md_ttbr0;  /* user address-space root (TTBR0_EL1), 0 for kernel-only */
	u_int64_t md_entry;  /* initial PC for a new thread (seeded into the first frame's lr) */
};

struct taskStruct; /* == kTask_t */

/* Initialise a freshly-allocated task's md state (called by schedNewTask after
 * its kernel stack is allocated).  i386 sets the TSS ring-0 stack; aarch64
 * clears the software-context fields. */
void md_new_task(struct taskStruct *t);
void md_setup_initial_frame(struct taskStruct *t);
void switch_to(struct taskStruct *prev, struct taskStruct *next);

/* Scheduler tick rate (Hz): the bring-up generic timer ticks at 2 Hz (timer.c). */
#define SCHED_HZ 2

/* Per-dispatch arch hook before switch_to (see i386/proc.h).  aarch64: no-op. */
void md_sched_pre_switch(struct taskStruct *t);

#endif /* _AARCH64_PROC_H */
