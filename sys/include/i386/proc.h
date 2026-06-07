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
#include <sys/gdt.h> /* SEL_PCPU */

/**
 * Re-establish this CPU's per-CPU segment base (%gs = SEL_PCPU).
 *
 * Generic code (e.g. endTask after freeing the address space) calls this where
 * an operation may have disturbed %gs; without it, `_current` (read as %gs:8)
 * would be poisoned for the rest of the function.  aarch64 has no equivalent
 * (per-CPU state is not segment-based), so its hook is a no-op.
 */
static inline void machine_pcpu_reload(void)
{
	__asm__ __volatile__("movw %0, %%gs" : : "r"((u_int16_t)SEL_PCPU) : "memory");
}

/**
 * Halt the CPU until the next interrupt (idle / unreachable-loop backstop).
 */
static inline void machine_idle(void)
{
	__asm__ __volatile__("hlt");
}

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
 * Initialise a freshly-allocated task's md state (called by schedNewTask after
 * its kernel stack is allocated): sets the TSS ring-0 stack (esp0/ss0).
 * Defined in arch/i386/context_switch.c.
 */
void md_new_task(struct taskStruct *t);

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

/* Scheduler tick rate (Hz): the PIT is programmed for 200 Hz on i386. */
#define SCHED_HZ 200

/*
 * Per-dispatch arch hook the generic scheduler calls just before switch_to(),
 * passing the incoming task.  i386 masks the timer IRQ while a VM86 (BIOS) task
 * runs.  Defined in arch/i386/context_switch.c.
 */
void md_sched_pre_switch(struct taskStruct *t);

#endif /* _I386_PROC_H */
