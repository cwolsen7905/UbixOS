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

#define FRAME_SLOTS 12     /* x19-x28, x29(fp), x30(lr) — must match context.S */
#define LR_SLOT 11         /* x30 (lr) is the 12th slot */
#define KSTACK_SIZE 65536  /* 64 KB; matches schedNewTask kmalloc + INITIAL_KSTACK_SIZE */
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
	sched_resume_unlock(); /* release the run-queue lock held across the dispatch switch */
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

	sched_resume_unlock();               /* release the run-queue lock held across the switch */
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

	/* Eagerly save the outgoing task's FPSIMD (v0-v31 + FPSR/FPCR) and restore the
	 * incoming task's.  The kernel is -mgeneral-regs-only, so the physical q
	 * registers still hold the OUTGOING task's user vector state here (whether we
	 * arrived via a cooperative SVC or a timer preemption — the EL1 entry saves only
	 * x0-x30).  CPACR_EL1.FPEN=0b11 (mmu.c) lets EL1 run these.  16-byte-align the
	 * in-use region within md_fpu.  Lazy/trap-based FPSIMD is avoided (unsafe SMP). */
	{
		u_int8_t *fp_prev = (u_int8_t *)(((uintptr_t)prev->md.md_fpu + 15) & ~(uintptr_t)15);
		u_int8_t *fp_next = (u_int8_t *)(((uintptr_t)next->md.md_fpu + 15) & ~(uintptr_t)15);
		__asm__ __volatile__("stp q0, q1, [%0, #0]\n\t"
		                     "stp q2, q3, [%0, #32]\n\t"
		                     "stp q4, q5, [%0, #64]\n\t"
		                     "stp q6, q7, [%0, #96]\n\t"
		                     "stp q8, q9, [%0, #128]\n\t"
		                     "stp q10, q11, [%0, #160]\n\t"
		                     "stp q12, q13, [%0, #192]\n\t"
		                     "stp q14, q15, [%0, #224]\n\t"
		                     "stp q16, q17, [%0, #256]\n\t"
		                     "stp q18, q19, [%0, #288]\n\t"
		                     "stp q20, q21, [%0, #320]\n\t"
		                     "stp q22, q23, [%0, #352]\n\t"
		                     "stp q24, q25, [%0, #384]\n\t"
		                     "stp q26, q27, [%0, #416]\n\t"
		                     "stp q28, q29, [%0, #448]\n\t"
		                     "stp q30, q31, [%0, #480]\n\t"
		                     :
		                     : "r"(fp_prev)
		                     : "memory");
		__asm__ volatile("mrs %0, fpsr" : "=r"(prev->md.md_fpsr));
		__asm__ volatile("mrs %0, fpcr" : "=r"(prev->md.md_fpcr));
		__asm__ volatile("msr fpsr, %0" : : "r"(next->md.md_fpsr));
		__asm__ volatile("msr fpcr, %0" : : "r"(next->md.md_fpcr));
		__asm__ __volatile__("ldp q0, q1, [%0, #0]\n\t"
		                     "ldp q2, q3, [%0, #32]\n\t"
		                     "ldp q4, q5, [%0, #64]\n\t"
		                     "ldp q6, q7, [%0, #96]\n\t"
		                     "ldp q8, q9, [%0, #128]\n\t"
		                     "ldp q10, q11, [%0, #160]\n\t"
		                     "ldp q12, q13, [%0, #192]\n\t"
		                     "ldp q14, q15, [%0, #224]\n\t"
		                     "ldp q16, q17, [%0, #256]\n\t"
		                     "ldp q18, q19, [%0, #288]\n\t"
		                     "ldp q20, q21, [%0, #320]\n\t"
		                     "ldp q22, q23, [%0, #352]\n\t"
		                     "ldp q24, q25, [%0, #384]\n\t"
		                     "ldp q26, q27, [%0, #416]\n\t"
		                     "ldp q28, q29, [%0, #448]\n\t"
		                     "ldp q30, q31, [%0, #480]\n\t"
		                     :
		                     : "r"(fp_next)
		                     : "memory");
	}

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
	_current->children++;

	/* Inherit the parent's process context (cwd, ppid, pgrp/sid, ctty, attached
	 * terminal, creds, QoS floor) and its signal dispositions + mask.  MI helpers
	 * shared with i386 sys_fork (sys/kern/kern_fork.c).  pmap_fork_copy() already
	 * ran above, so the signal-state clear is safely after the address-space copy
	 * (see proc_fork_signal_init). */
	proc_fork_inherit_context(child);
	proc_fork_signal_init(child, &_current->td);

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
 * rfork(RFMEM) — thread create: a new task SHARING the caller's address space
 * (same TTBR0, not a copy), running on a caller-supplied user stack, resuming at
 * the caller's rfork() return point with x0 = 0 — so musl's clone.s child path runs
 * the thread function.  It joins the caller's thread group (tgid), so endTask frees
 * the shared address space + fd table only when the last task of the group exits.
 *
 * @parent_tf is the caller's trapframe; the SysV-on-aarch64 syscall args are its
 * x0..x2: x0 = flags (informational; v1 always shares), x1 = child user-stack top,
 * x2 = TLS base (TPIDR_EL0; 0 if none).
 *
 * @return the new thread's tid to the caller; the new thread itself sees 0.
 */
int aarch64_rfork(u_int64_t *parent_tf)
{
	u_int64_t child_stack = parent_tf[1];
	u_int64_t tls = parent_tf[2];
	kTask_t *child;
	u_int8_t *top;
	u_int64_t *tf, *ctx;
	int i;

	child = schedNewTask();

	/* SHARE the caller's address space (same L1) — the defining difference from
	 * fork(), which copies it via pmap_fork_copy. */
	child->md.md_ttbr0 = _current->md.md_ttbr0;
	child->md.md_usp = child_stack; /* its own user SP; marks a user task */
	child->md.md_entry = 0;         /* unused — the trapframe drives the return */
	child->md.md_tpidr = tls != 0 ? tls : _current->md.md_tpidr;

	child->parent = _current;
	_current->children++;

	/* Same thread group => shared AS + fd table; endTask's sched_tgid_others_alive()
	 * frees them only when the LAST thread of the group exits. */
	child->tgid = _current->tgid;

	proc_fork_inherit_context(child);
	proc_fork_signal_init(child, &_current->td);

	/* Shallow-share the fd table: threads share open files.  Free the placeholder
	 * fds schedNewTask() made for 0-2, then point at the caller's file objects.
	 * (endTask closes them only for the last thread of the tgid — guarded there.) */
	for (i = 0; i < O_FILES; i++)
	{
		if (child->td.o_files[i] != 0)
		{
			kfree(child->td.o_files[i]);
			child->td.o_files[i] = 0;
		}
		child->td.o_files[i] = _current->td.o_files[i];
	}

	/* Build the child kernel stack: a copy of the caller's trapframe (x0 = 0, its own
	 * user SP, fresh frame pointer), and below it a ctx frame that ret_from_fork's
	 * into it. */
	top = (u_int8_t *)child->kernelStack + KSTACK_SIZE;
	tf = (u_int64_t *)(top - TRAPFRAME_SIZE);
	memcpy(tf, parent_tf, TRAPFRAME_SIZE);
	tf[0] = 0;                        /* the new thread sees rfork() == 0 */
	tf[TF_SLOT_SP_EL0] = child_stack; /* its own user stack */
	tf[29] = 0;                       /* fresh frame pointer (x29) */

	ctx = (u_int64_t *)((u_int8_t *)tf - FRAME_SLOTS * 8);
	for (i = 0; i < (int)FRAME_SLOTS; i++)
		ctx[i] = 0;
	ctx[LR_SLOT] = (u_int64_t)(uintptr_t)ret_from_fork;

	child->md.md_kstack = (u_int64_t)(uintptr_t)ctx;
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
