/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * i386 machine-dependent per-process state embedded in kTask_t (the 'md'
 * field).  Generic code only passes 'md' around; arch code uses md_tss/md_i387
 * directly.  Reached via <machine/proc.h>.  The aarch64 sibling replaces the
 * hardware TSS with a software register frame.
 */
#ifndef _I386_PROC_H
#define _I386_PROC_H

#include <sys/tss.h>

struct md_proc
{
	struct tssStruct md_tss;
	struct i387Struct md_i387;
	/*
	 * Saved kernel stack pointer for software context switching (switch_to).
	 * Points at this task's saved callee-registers frame on its kernel stack.
	 * Zero until md_setup_initial_frame() builds the task's first frame.
	 */
	u_int32_t md_kstack;
};

/* Convenience accessor so arch code can write TASK_TSS(t).eip */
#define TASK_TSS(t) ((t)->md.md_tss)

struct taskStruct; /* == kTask_t */

/*
 * Build a newly created task's initial kernel-stack frame for software context
 * switching.  Defined in arch/i386/context_switch.c.
 */
void md_setup_initial_frame(struct taskStruct *t);

/*
 * Software context switch from prev to next (callee-saved regs + kernel ESP,
 * swaps CR3, updates the kernel TSS esp0).  Call with interrupts disabled.
 * Defined in arch/i386/context_switch.c.
 */
void switch_to(struct taskStruct *prev, struct taskStruct *next);

#endif /* _I386_PROC_H */
