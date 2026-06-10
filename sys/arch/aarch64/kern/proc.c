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
#include <sys/descrip.h>
#include <sys/pipe.h>
#include <lib/kmalloc.h>
#include <string.h>

#define FRAME_SLOTS 12 /* x19-x28, x29(fp), x30(lr) — must match context.S */
#define LR_SLOT 11     /* x30 (lr) is the 12th slot */
#define KSTACK_SIZE 8192
#define TRAPFRAME_SIZE 288 /* must match KERNEL_ENTRY in vectors.S */
#define TF_SLOT_SP_EL0 34  /* sp_el0 at offset 272 in the trapframe */

/**
 * Initialise a freshly-allocated task's md state.  The software-context fields
 * are already zeroed by schedNewTask's memset; this is the explicit arch hook
 * and the place future per-task MMU (TTBR0) setup will land.
 */
void md_new_task(kTask_t *t)
{
	u_int64_t ttbr0;

	t->md.md_kstack = 0; /* "not yet seeded" — sched_ready() builds the frame */

	/* Inherit the current (kernel) address space by default.  A user process
	 * overrides md_ttbr0 with its own page-table root at exec time. */
	__asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
	t->md.md_ttbr0 = ttbr0 & 0x0000FFFFFFFFF000UL;
}

/**
 * First-switch trampoline for a user (EL0) task: ERET to its entry at EL0.
 *
 * Reached when switch_to() first dispatches the task — _current is already the
 * new task, and SP (SP_EL1) is its kernel stack (the seeded frame was popped),
 * so its later EL0 traps land on that stack.  Does not return.
 */
static void user_trampoline(void)
{
	aarch64_eret_to_el0(_current->md.md_entry, _current->md.md_usp);
}

/**
 * First-switch trampoline for a kernel thread.  A fresh task is first entered
 * from within the timer IRQ handler (preemptive dispatch) with IRQs masked, so
 * unmask them before running the thread — otherwise it can never be preempted.
 */
static void kthread_trampoline(void)
{
	void (*entry)(void *) = (void (*)(void *))(uintptr_t)_current->md.md_entry;
	void *arg = (void *)(uintptr_t)_current->md.md_arg;

	__asm__ volatile("msr daifclr, #2"); /* enable IRQ for the new thread */
	entry(arg);                          /* arg is 0 for no-arg entries (extra x0 ignored) */
	for (;;)                             /* a kernel thread returning is unexpected — park */
		sched_yield();
}

/**
 * Build a new task's initial kernel-stack frame.  A kernel thread's saved lr is
 * its entry point (the first switch_to() `ret`s into it); a user task's (md_usp
 * set) saved lr is user_trampoline, which ERETs to EL0.
 */
void md_setup_initial_frame(kTask_t *t)
{
	u_int64_t *sp = (u_int64_t *)((u_int8_t *)t->kernelStack + KSTACK_SIZE);

	sp -= FRAME_SLOTS;
	for (unsigned i = 0; i < FRAME_SLOTS; i++)
		sp[i] = 0;
	sp[LR_SLOT] =
	    (t->md.md_usp != 0) ? (u_int64_t)(uintptr_t)user_trampoline : (u_int64_t)(uintptr_t)kthread_trampoline;

	t->md.md_kstack = (u_int64_t)(uintptr_t)sp;
}

/**
 * Register-level context switch from prev to next.  Saves prev's callee-saved
 * state + SP into prev->md.md_kstack and resumes next from next->md.md_kstack.
 * Call with interrupts disabled.
 */
void switch_to(kTask_t *prev, kTask_t *next)
{
	/* Load next's address space if it differs (avoid a needless TLB flush when
	 * switching between tasks that share one, e.g. kernel threads).  All address
	 * spaces map the kernel + identity-mapped stacks, so it is safe to swap here
	 * before the register switch resumes next. */
	if (next->md.md_ttbr0 != 0 && next->md.md_ttbr0 != prev->md.md_ttbr0)
		pmap_switch((u_int64_t *)(uintptr_t)next->md.md_ttbr0);

	/* Save/restore the EL0 TLS thread pointer.  musl keeps its thread-control
	 * block here (errno, cancellation state, stack-canary base); leaking the
	 * outgoing task's TPIDR_EL0 into the incoming one makes its TLS accesses read
	 * another task's (or a freed) TCB — e.g. a parent resuming after a musl child
	 * exits would inherit the child's thread pointer and fault. */
	__asm__ volatile("mrs %0, tpidr_el0" : "=r"(prev->md.md_tpidr));
	__asm__ volatile("msr tpidr_el0, %0" : : "r"(next->md.md_tpidr));

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

/**
 * fork(): create a child task that resumes at the same EL0 point as the caller
 * with a copy of its address space.  @parent_tf is the caller's trapframe (the
 * saved x0-x30 + ELR/SPSR/SP_EL0 from the fork SVC).
 *
 * The child gets a deep copy of the parent's user pages, a duplicate of the
 * trapframe (with x0 = 0 so its fork returns 0), and a seeded ctx frame whose lr
 * is ret_from_fork, so its first dispatch ERETs to EL0 via that trapframe.
 *
 * @return the child's pid (the parent's fork return value), or -1 on failure.
 */
int aarch64_fork(u_int64_t *parent_tf)
{
	u_int64_t ttbr0;
	u_int64_t *child_l1;
	kTask_t *child;
	u_int8_t *top;
	u_int64_t *tf, *ctx;

	__asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
	child_l1 = pmap_fork_copy((u_int64_t *)(uintptr_t)(ttbr0 & 0x0000FFFFFFFFF000UL));
	if (child_l1 == 0)
		return -1;

	child = schedNewTask();
	child->md.md_ttbr0 = (u_int64_t)(uintptr_t)child_l1;
	child->md.md_usp = parent_tf[TF_SLOT_SP_EL0]; /* same user SP (copied space); marks a user task */
	child->md.md_entry = 0;                       /* unused — the trapframe drives the return */

	/* Parent linkage so wait4() can find + reap this child (and so the generic
	 * sched() reaper notifies the right parent). */
	child->parent = _current;
	child->ppid = _current->id;
	_current->children++;

	/* Inherit the parent's process context: cwd, process group / session,
	 * controlling terminal, and credentials. */
	memcpy(child->oInfo.cwd, _current->oInfo.cwd, sizeof(child->oInfo.cwd));
	child->pgrp = _current->pgrp;
	child->sid = _current->sid;
	child->ct_tty = _current->ct_tty;
	child->uid = _current->uid;
	child->gid = _current->gid;

	/* Inherit the open-file table (stdin/stdout/stderr + everything else). */
	fork_copy_fdtable(child, &_current->td);

	/* Inherit the parent's EL0 TLS thread pointer: the child's address space is a
	 * copy, so the parent's TCB lives at the same VA.  The child resumes at the
	 * fork return point and must see its TLS until it sets up its own. */
	__asm__ volatile("mrs %0, tpidr_el0" : "=r"(child->md.md_tpidr));

	/* Build the child kernel stack: a copy of the parent trapframe at the top
	 * (with x0 = 0), and below it a ctx frame that ret_from_fork's into it. */
	top = (u_int8_t *)child->kernelStack + KSTACK_SIZE;
	tf = (u_int64_t *)(top - TRAPFRAME_SIZE);
	memcpy(tf, parent_tf, TRAPFRAME_SIZE);
	tf[0] = 0; /* child's fork() returns 0 */

	ctx = (u_int64_t *)((u_int8_t *)tf - FRAME_SLOTS * 8);
	for (unsigned i = 0; i < FRAME_SLOTS; i++)
		ctx[i] = 0;
	ctx[LR_SLOT] = (u_int64_t)(uintptr_t)ret_from_fork;

	child->md.md_kstack = (u_int64_t)(uintptr_t)ctx; /* set, so sched_ready skips md_setup_initial_frame */
	sched_ready(child);

	return child->id;
}

/**
 * Create a kernel thread that runs @tproc(@arg) at EL1, scheduled by the generic
 * scheduler.  AArch64 implementation of the MI execThread() (declared in
 * <ubixos/exec.h>): the i386 version delivers @arg via the cdecl stack; here it
 * is stashed in md_arg and handed to the thread in x0 by kthread_trampoline.
 * Used by ubthread_create() / lwIP's sys_thread_new().
 *
 * @param stack  requested stack size (ignored — kernel threads use the fixed
 *               per-task kernel stack allocated by schedNewTask).
 * @return the new task's pid (an opaque handle to the caller).
 */
u_int32_t execThread(void (*tproc)(void), u_int32_t stack, char *arg, const char *name)
{
	kTask_t *t = schedNewTask();

	(void)stack;
	if (t == 0)
		return (0);
	t->md.md_entry = (u_int64_t)(uintptr_t)tproc;
	t->md.md_arg = (u_int64_t)(uintptr_t)arg;
	if (name != 0)
		strncpy(t->name, name, sizeof(t->name) - 1);
	sched_ready(t);
	return (t->id);
}
