/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 machine-dependent process glue for the generic scheduler.
 *
 * Implements the four md hooks the arch-neutral sched_core/sched_dispatch call:
 *   md_new_task()            initialise a fresh task's software-context fields
 *   md_setup_initial_frame() seed its kernel stack so the first switch-in `ret`s
 *                            into the thread entry (md_entry)
 *   switch_to()              the register-level context switch (wraps the
 *                            aarch64_ctx_switch primitive in context.S)
 *   md_sched_pre_switch()    per-dispatch hook (no-op on aarch64; i386 masks the
 *                            timer for VM86 tasks)
 *
 * The kernel-stack frame layout matches aarch64_ctx_switch: 12 callee-saved
 * slots (x19-x28, x29=fp, x30=lr) at the top, with lr = the thread entry.
 */

#include "bringup.h"
#include <ubixos/sched.h>
#include <machine/proc.h>

#define FRAME_SLOTS 12 /* x19-x28, x29(fp), x30(lr) — must match context.S */
#define LR_SLOT 11     /* x30 (lr) is the 12th slot */
#define KSTACK_SIZE 8192

/**
 * Initialise a freshly-allocated task's md state.  The software-context fields
 * are already zeroed by schedNewTask's memset; this is the explicit arch hook
 * and the place future per-task MMU (TTBR0) setup will land.
 */
void md_new_task(kTask_t *t)
{
	t->md.md_kstack = 0; /* "not yet seeded" — sched_ready() builds the frame */
	t->md.md_ttbr0 = 0;  /* kernel-only address space for now */
}

/**
 * Build a new thread's initial kernel-stack frame.  Lays down a 12-slot
 * callee-saved frame at the top of the task's kernel stack with the saved lr set
 * to md_entry, so the first switch_to() into this task restores the (zeroed)
 * registers and `ret`s straight into the entry point.
 */
void md_setup_initial_frame(kTask_t *t)
{
	u_int64_t *sp = (u_int64_t *)((u_int8_t *)t->kernelStack + KSTACK_SIZE);

	sp -= FRAME_SLOTS;
	for (unsigned i = 0; i < FRAME_SLOTS; i++)
		sp[i] = 0;
	sp[LR_SLOT] = t->md.md_entry; /* aarch64_ctx_switch `ret`s here on first switch-in */

	t->md.md_kstack = (u_int64_t)(uintptr_t)sp;
}

/**
 * Register-level context switch from prev to next.  Saves prev's callee-saved
 * state + SP into prev->md.md_kstack and resumes next from next->md.md_kstack.
 * Call with interrupts disabled.
 */
void switch_to(kTask_t *prev, kTask_t *next)
{
	aarch64_ctx_switch(&prev->md.md_kstack, next->md.md_kstack);
}

/**
 * Per-dispatch arch hook invoked just before switch_to().  Nothing to do on
 * aarch64 (no VM86 monitor to protect from the tick).
 */
void md_sched_pre_switch(kTask_t *t)
{
	(void)t;
}
