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

#ifndef _MACHINE_PROC_H
#define _MACHINE_PROC_H

#include <sys/tss.h>

/*
 * i386 machine-dependent per-process state embedded in kTask_t.
 * Generic code accesses this via the 'md' field; arch code uses
 * md_tss and md_i387 directly.  An x86_64 port replaces this file
 * with a software context frame (no hardware TSS task switching).
 */
struct md_proc
{
	struct tssStruct md_tss;
	struct i387Struct md_i387;
	/*
	 * Saved kernel stack pointer for software context switching (switch_to).
	 * Points at this task's saved callee-registers frame on its kernel stack.
	 * Zero until md_setup_initial_frame() builds the task's first frame.  Unused
	 * while hardware (ljmp/TSS) switching is still active and for v86 tasks.
	 */
	u_int32_t md_kstack;
};

/* Convenience accessor so arch code can write TASK_TSS(t).eip */
#define TASK_TSS(t) ((t)->md.md_tss)

struct taskStruct; /* == kTask_t */

/*
 * Build a newly created task's initial kernel-stack frame for software context
 * switching.  Call once, after md_tss is populated and before the task first
 * runs (from sched_ready).  Defined in arch/i386/context_switch.c.
 */
void md_setup_initial_frame(struct taskStruct *t);

/*
 * Software context switch from prev to next (saves/restores callee-saved regs +
 * kernel ESP, swaps CR3, updates the kernel TSS esp0).  Call with interrupts
 * disabled.  Defined in arch/i386/context_switch.c.
 */
void switch_to(struct taskStruct *prev, struct taskStruct *next);

#endif /* _MACHINE_PROC_H */
