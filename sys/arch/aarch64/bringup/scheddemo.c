/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 generic-scheduler demo (QEMU `virt` bring-up, Phase 13c).
 *
 * Proves the arch-neutral scheduler (sys/kern/sched_core.c + sched_dispatch.c)
 * runs unmodified on aarch64, driving real kTask_t threads through the aarch64
 * md hooks (md_new_task/md_setup_initial_frame/switch_to).  Two kernel threads
 * created via schedNewTask()/sched_ready() cooperatively yield through the real
 * sched()/sched_yield() dispatch; the boot context is the third (kernel) task.
 *
 * Throwaway scaffolding — superseded once a real init path and the timer-driven
 * (preemptive) tick land.
 */

#include "bringup.h"
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <machine/proc.h>
#include <string.h>

static kTask_t *g_a;
static kTask_t *g_b;
static volatile int g_a_iters;
static volatile int g_b_iters;

/**
 * Demo thread A: announce three iterations, yielding between each, then park.
 */
static void demo_thread_a(void)
{
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
static void demo_thread_b(void)
{
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
static kTask_t *spawn(void (*entry)(void), const char *name)
{
	kTask_t *t = schedNewTask();
	t->md.md_entry = (u_int64_t)(uintptr_t)entry;
	strncpy(t->name, name, sizeof(t->name) - 1);
	sched_ready(t);
	return t;
}

/**
 * Run the demo: make the boot context a task, spawn A and B, then yield into the
 * generic scheduler until both have run; control returns here via switch_to.
 */
void aarch64_sched_demo(void)
{
	kprintf("sched demo: generic sched_core driving aarch64 threads...\n");

	/* Bootstrap the task list and adopt the boot context as the current task. */
	sched_init();
	set_current(taskList);
	taskList->state = RUNNING;
	taskList->priority = QOS_DEFAULT; /* drop from REALTIME so A/B share the CPU */
	taskList->base_priority = QOS_DEFAULT;

	g_a = spawn(demo_thread_a, "demoA");
	g_b = spawn(demo_thread_b, "demoB");

	for (int i = 0; i < 16 && (g_a_iters < 3 || g_b_iters < 3); i++)
		sched_yield();

	kprintf("sched demo: back in main (A=%d, B=%d iters) - generic scheduler works on aarch64.\n",
	        g_a_iters,
	        g_b_iters);
}
