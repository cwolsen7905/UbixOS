/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 context-switch bring-up test (Phase 4b).
 *
 * Self-contained verification of cpu_switch.S (x86_64_ctx_switch) before the
 * machine-independent scheduler is linked: build a second kernel "thread" on its
 * own stack with a seeded initial frame, switch into it, have it switch back, and
 * resume it — exercising both a fresh-thread entry and a save/restore round trip.
 * The real scheduler (sched_core/sched_dispatch) reuses x86_64_ctx_switch via a
 * proper switch_to(); this throwaway demo just proves the MD asm.
 */

#include "x86_64.h"

#define DEMO_STACK 16384

/* Minimal context object (NOT kTask_t — avoids the scheduler dependency cascade). */
struct ctx
{
	u64 rsp;                         /* saved stack pointer */
	u64 entry;                       /* thread entry (read by the trampoline) */
	unsigned char stack[DEMO_STACK]; /* this context's kernel stack */
};

static struct ctx g_main; /* the boot context */
static struct ctx g_thr;  /* the demo thread   */
static struct ctx *g_cur; /* whoever is running now */

static void ctx_switch_to(struct ctx *next)
{
	struct ctx *prev = g_cur;
	g_cur = next;
	x86_64_ctx_switch(&prev->rsp, next->rsp);
}

/* First-switch trampoline: x86_64_ctx_switch RETs here for a fresh thread. */
static void ctx_trampoline(void)
{
	void (*entry)(void) = (void (*)(void))g_cur->entry;
	entry();
	/* A demo thread returning is unexpected; park. */
	for (;;)
		__asm__ __volatile__("hlt");
}

/* Seed @t so the first switch into it RETs into ctx_trampoline -> entry. */
static void ctx_seed(struct ctx *t, void (*entry)(void))
{
	u64 *sp = (u64 *)(t->stack + DEMO_STACK);
	sp -= 7; /* 6 callee-saved slots + 1 return address */
	for (int i = 0; i < 7; i++)
		sp[i] = 0;
	sp[6] = (u64)ctx_trampoline; /* RET target after the 6 pops */
	t->entry = (u64)entry;
	t->rsp = (u64)sp;
}

static void demo_thread(void)
{
	serial_puts("  [thread] running on its own stack (fresh-entry ok)\n");
	ctx_switch_to(&g_main); /* yield back to main */
	serial_puts("  [thread] resumed after main yielded back (save/restore ok)\n");
	ctx_switch_to(&g_main); /* final yield back */
}

void x86_64_ctx_test(void)
{
	serial_puts("ctx test: main -> thread\n");
	g_cur = &g_main;
	ctx_seed(&g_thr, demo_thread);

	ctx_switch_to(&g_thr); /* into the fresh thread */
	serial_puts("ctx test: back in main (thread yielded)\n");

	ctx_switch_to(&g_thr); /* resume the thread mid-function */
	serial_puts("ctx test: context switch verified (main<->thread round trips)\n");
}
