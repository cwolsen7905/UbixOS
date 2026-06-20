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
#include <string.h> /* strncpy (execThread sets the thread name) */

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
 * Create and schedule a kernel thread that runs @tproc(@arg) at ring 0 in the
 * kernel address space.  Leaves md_usp = 0 (so the first dispatch enters via
 * kthread_trampoline, not the ring-3 path) and md_kstack = 0 (so sched_ready
 * builds the initial frame).  Used by ubthread_create / sys_thread_new (the lwIP
 * tcpip + RX threads).  Sibling of aarch64's execThread.
 *
 * @return the new thread's pid, or 0 on failure.
 */
u_int32_t execThread(void (*tproc)(void), u_int32_t stack, char *arg, const char *name)
{
	kTask_t *t = schedNewTask();

	(void)stack;
	if (t == 0)
		return (0);
	t->md.md_entry = (u64)(uintptr_t)tproc;
	t->md.md_arg = (u64)(uintptr_t)arg;
	t->md.md_usp = 0; /* kernel thread — no ring-3 stack */
	if (name != 0)
		strncpy(t->name, name, sizeof(t->name) - 1);
	sched_ready(t);
	return (t->id);
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

	sched_resume_unlock(); /* release the run-queue lock held across the dispatch switch */
	__asm__ __volatile__("sti");
	entry(arg);
	for (;;) /* a kernel thread returning is unexpected — park */
		sched_yield();
}

/**
 * First-switch trampoline for a user (ring-3) task: IRETQ to its entry at ring 3.
 * Reached when switch_to() first dispatches the task — _current is already the
 * new task, CR3 is its address space, and rsp0 is its kernel stack, so its later
 * ring-3 traps land on that stack.  Does not return.
 */
static void user_trampoline(void)
{
	sched_resume_unlock(); /* release the run-queue lock held across the dispatch switch */
	x86_64_iret_to_user(_current->md.md_entry, _current->md.md_usp);
}

/**
 * Build a new task's initial kernel-stack frame so the first switch into it
 * (cpu_switch.S: pop 6 callee-saved, RET) lands in its trampoline: a user task
 * (md_usp set) IRETQs to ring 3; a kernel thread runs its entry directly.
 */
void md_setup_initial_frame(kTask_t *t)
{
	u64 *sp = (u64 *)((unsigned char *)t->kernelStack + KSTACK_SIZE);

	sp -= FRAME_SLOTS;
	for (unsigned i = 0; i < FRAME_SLOTS; i++)
		sp[i] = 0;
	sp[FRAME_SLOTS - 1] = (t->md.md_usp != 0) ? (u64)user_trampoline : (u64)kthread_trampoline;

	t->md.md_kstack = (u64)sp;
}

/**
 * Register-level context switch from prev to next.  Swap the address space if it
 * differs (kernel threads share the kernel CR3), re-arm the TSS ring-0 stack to
 * next's kernel stack (so a ring-3 task's traps land on its own stack), then
 * save/restore registers via cpu_switch.S.  Call with interrupts disabled.
 */
void switch_to(kTask_t *prev, kTask_t *next)
{
	if (next->md.md_cr3 != 0 && next->md.md_cr3 != prev->md.md_cr3)
		__asm__ __volatile__("mov %0, %%cr3" : : "r"(next->md.md_cr3) : "memory");

	if (next->kernelStack != 0)
		x86_64_set_user_kstack((u64)((unsigned char *)next->kernelStack + KSTACK_SIZE));

	/* Restore the task's user TLS base (FS.base): the syscall path doesn't use
	 * swapgs (the kernel owns %gs; musl keeps TLS in %fs), so FS.base is NOT saved
	 * per task by hardware.  Without this it stays at whatever the last task set —
	 * a task whose ld-musl reads TLS before its own arch_prctl(SET_FS) would see a
	 * stale pointer into another address space (corrupts ld.so's dynamic linking). */
	__asm__ __volatile__("wrmsr"
	                     :
	                     : "c"(0xC0000100u), "a"((u32)next->md.md_fsbase), "d"((u32)(next->md.md_fsbase >> 32)));

	/* Eagerly save the outgoing task's x87/SSE state and restore the incoming
	 * task's.  The kernel is built -mno-sse, so the physical XMM registers still
	 * hold the OUTGOING task's user vector state here (whether we arrived via a
	 * cooperative yield or a timer preemption — the ISR path saves only GPRs).
	 * FXSAVE/FXRSTOR need a 16-byte-aligned 512-byte area; align within md_fpu.
	 * CR4.OSFXSR is set at boot, so the XMM half is included; CR0.TS is clear, so
	 * no #NM trap.  Lazy/TS-trap FPU is deliberately avoided (it is unsafe on SMP). */
	{
		u8 *fp_prev = (u8 *)(((uintptr_t)prev->md.md_fpu + 15) & ~(uintptr_t)15);
		u8 *fp_next = (u8 *)(((uintptr_t)next->md.md_fpu + 15) & ~(uintptr_t)15);
		__asm__ __volatile__("fxsave (%0)" : : "r"(fp_prev) : "memory");
		__asm__ __volatile__("fxrstor (%0)" : : "r"(fp_next) : "memory");
	}

	x86_64_ctx_switch((u64 *)&prev->md.md_kstack, (u64)next->md.md_kstack);
}

/** Per-dispatch hook before switch_to (i386 masks the timer for VM86).  x86_64: none. */
void md_sched_pre_switch(kTask_t *t)
{
	(void)t;
}
