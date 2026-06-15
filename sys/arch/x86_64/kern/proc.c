/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 machine-dependent scheduler hooks (Phase 4b).  The four md entry points
 * the arch-neutral scheduler (sys/kern/sched_core.c + sched_dispatch.c) calls:
 * md_new_task, md_setup_initial_frame, switch_to, md_sched_pre_switch — built on
 * the register switch in cpu_switch.S.  Sibling of aarch64/kern/proc.c.
 */

#include "../x86_64.h"
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>

#define KSTACK_SIZE 65536 /* must match schedNewTask's kmalloc */
#define FRAME_SLOTS 7     /* 6 callee-saved (r15..rbx) + 1 return address */

/**
 * Initialise a freshly-allocated task's md state (schedNewTask after the kernel
 * stack is allocated).  md_kstack stays 0 ("not yet seeded" — sched_ready builds
 * the frame); inherit the current (kernel) address space by default.
 */
void md_new_task(kTask_t *t)
{
	u64 cr3;
	t->md.md_kstack = 0;
	__asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
	t->md.md_cr3 = cr3;
}

/**
 * First-switch trampoline for a kernel thread.  A fresh task is first entered
 * from within the timer IRQ handler (preemptive dispatch) with interrupts masked,
 * so enable them before running the thread — else it can never be preempted.
 */
static void kthread_trampoline(void)
{
	void (*entry)(void *) = (void (*)(void *))(unsigned long)_current->md.md_entry;
	void *arg = (void *)(unsigned long)_current->md.md_arg;

	__asm__ __volatile__("sti");
	entry(arg);
	for (;;) /* a kernel thread returning is unexpected — park */
		sched_yield();
}

/**
 * Build a new task's initial kernel-stack frame so the first switch into it
 * (cpu_switch.S: pop 6 callee-saved, RET) lands in kthread_trampoline.
 */
void md_setup_initial_frame(kTask_t *t)
{
	u64 *sp = (u64 *)((unsigned char *)t->kernelStack + KSTACK_SIZE);

	sp -= FRAME_SLOTS;
	for (unsigned i = 0; i < FRAME_SLOTS; i++)
		sp[i] = 0;
	sp[FRAME_SLOTS - 1] = (u64)kthread_trampoline; /* RET target after the 6 pops */

	t->md.md_kstack = (u64)sp;
}

/**
 * Register-level context switch from prev to next.  Swap the address space if it
 * differs (kernel threads share the kernel CR3), then save/restore registers via
 * cpu_switch.S.  Call with interrupts disabled.
 */
void switch_to(kTask_t *prev, kTask_t *next)
{
	if (next->md.md_cr3 != 0 && next->md.md_cr3 != prev->md.md_cr3)
		__asm__ __volatile__("mov %0, %%cr3" : : "r"(next->md.md_cr3) : "memory");

	x86_64_ctx_switch((u64 *)&prev->md.md_kstack, (u64)next->md.md_kstack);
}

/** Per-dispatch hook before switch_to (i386 masks the timer for VM86).  x86_64: none. */
void md_sched_pre_switch(kTask_t *t)
{
	(void)t;
}
