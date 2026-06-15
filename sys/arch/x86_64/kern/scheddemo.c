/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 generic-scheduler demo (Phase 4b-2).  Proves the arch-neutral scheduler
 * (sys/kern/sched_core.c + sched_dispatch.c) runs unmodified on x86_64, driving
 * real kTask_t threads through the x86_64 md hooks (md_new_task /
 * md_setup_initial_frame / switch_to, built on cpu_switch.S).  Two kernel threads
 * created via schedNewTask()/sched_ready() cooperatively yield through the real
 * sched()/sched_yield() dispatch; the boot context is the third (kernel) task.
 *
 * Like aarch64, the kernel is non-preemptive here (no EL0/userland yet, so the
 * timer only accounts ticks); kernel threads must yield.  Throwaway scaffolding,
 * superseded once a real init path + userland land.  Sibling of aarch64's
 * bringup/scheddemo.c.
 */

#include "x86_64.h"
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <machine/proc.h>
#include <string.h>

static volatile int g_a_iters;
static volatile int g_b_iters;

/**
 * Demo thread A: announce three iterations, yielding between each, then park.
 */
static void demo_thread_a(void *arg)
{
	(void)arg;
	for (int i = 0; i < 3; i++)
	{
		kprintf("  [sched A] iteration %d (pid=%d)\n", i, _current->id);
		g_a_iters++;
		sched_yield();
	}
	kprintf("  [sched A] done -> parking\n");
	for (;;)
		sched_yield();
}

/**
 * Demo thread B: same as A on its own stack.
 */
static void demo_thread_b(void *arg)
{
	(void)arg;
	for (int i = 0; i < 3; i++)
	{
		kprintf("  [sched B] iteration %d (pid=%d)\n", i, _current->id);
		g_b_iters++;
		sched_yield();
	}
	kprintf("  [sched B] done -> parking\n");
	for (;;)
		sched_yield();
}

/**
 * Spawn a kernel thread at @entry via the generic scheduler API.
 */
static kTask_t *spawn(void (*entry)(void *), const char *name)
{
	kTask_t *t = schedNewTask();
	t->md.md_entry = (u64)(unsigned long)entry;
	t->md.md_arg = 0;
	strncpy(t->name, name, sizeof(t->name) - 1);
	sched_ready(t);
	return t;
}

/**
 * Run the demo: the boot context is already task 0 (sched_init + set_current in
 * kmain), spawn A and B, then yield into the generic scheduler until both have
 * run; control returns here via switch_to.
 */
void x86_64_sched_demo(void)
{
	kprintf("sched demo: generic sched_core driving x86_64 threads...\n");

	spawn(demo_thread_a, "demoA");
	spawn(demo_thread_b, "demoB");

	for (int i = 0; i < 16 && (g_a_iters < 3 || g_b_iters < 3); i++)
		sched_yield();

	kprintf(
	    "sched demo: back in main (A=%d, B=%d iters) - generic scheduler works on x86_64.\n", g_a_iters, g_b_iters);
}
