/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 preemptive-scheduling demo (QEMU `virt` bring-up).
 *
 * Two CPU-bound kernel tasks that NEVER call sched_yield().  Under cooperative
 * scheduling the first would run forever and starve the second; the fact that
 * their output interleaves proves the 100 Hz generic timer is driving sched()
 * (gic.c → timer_tick → sched) and preempting them on quantum expiry — real
 * preemptive multitasking on aarch64, built on the full-state trapframe.
 *
 * Created before the timer is enabled (boot.c); the timer then time-slices them
 * (and the idle/boot context).  Throwaway scaffolding.
 */

#include "bringup.h"
#include <ubixos/sched.h>
#include <string.h>

#define SPIN_WORK 4000000u /* busy iterations between prints (tuned for ~1/quantum) */

static volatile unsigned g_spin_sink; /* defeat dead-loop elimination */

/**
 * Busy-loop @rounds times printing a heartbeat, never yielding.
 */
static void spinner(const char *who)
{
	for (unsigned round = 0; round < 6; round++)
	{
		for (unsigned i = 0; i < SPIN_WORK; i++)
			g_spin_sink += i;
		kprintf("  [preempt %s] round %u (never yielded — timer preempted me)\n", who, round);
	}
	for (;;)
		g_spin_sink++; /* park: keep consuming CPU so preemption is still exercised */
}

/**
 * Task entry points (kernel threads take no argument, so wrap the shared body).
 */
static void spinner_a(void)
{
	spinner("A");
}

static void spinner_b(void)
{
	spinner("B");
}

/**
 * Spawn two CPU-bound, never-yielding kernel tasks.  The timer (enabled next in
 * boot.c) preempts between them.
 */
void aarch64_preempt_demo(void)
{
	kTask_t *a, *b;

	kprintf("preempt demo: two CPU-bound tasks that never yield — the timer must "
	        "preempt them...\n");

	a = schedNewTask();
	a->md.md_entry = (u_int64_t)(uintptr_t)spinner_a;
	strncpy(a->name, "spinA", sizeof(a->name) - 1);
	sched_ready(a);

	b = schedNewTask();
	b->md.md_entry = (u_int64_t)(uintptr_t)spinner_b;
	strncpy(b->name, "spinB", sizeof(b->name) - 1);
	sched_ready(b);

	kprintf("  spinA pid=%d, spinB pid=%d ready; enabling the timer next...\n", a->id, b->id);
}
