/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 context-switch demo (QEMU `virt` bring-up, Phase 13b).
 *
 * Proves aarch64_ctx_switch() end to end: two kernel "threads" on their own
 * stacks cooperatively hand control back and forth, then return to main.  This
 * is throwaway scaffolding to validate the switch primitive before the generic
 * sched_core port drives it for real.
 */

#include "bringup.h"

#define STACK_WORDS 1024 /* 8 KB per thread stack */
#define FRAME_SLOTS 12   /* callee-saved frame: x19-x28, x29, x30 */
#define LR_SLOT 11       /* x30 (lr) offset 88 = slot 11 in the frame */

static u_int64_t g_main_sp;
static u_int64_t g_a_sp;
static u_int64_t g_b_sp;
static u_int64_t g_a_stack[STACK_WORDS] __attribute__((aligned(16)));
static u_int64_t g_b_stack[STACK_WORDS] __attribute__((aligned(16)));

/**
 * Seed a fresh thread stack with a ctx-switch frame whose saved lr (x30) is the
 * entry point; the first switch-in restores it and `ret`s into @entry.  Returns
 * the SP to hand to aarch64_ctx_switch().
 */
static u_int64_t seed(u_int64_t *stack, unsigned words, void (*entry)(void))
{
	u_int64_t *sp = stack + words; /* top of stack (grows down) */
	sp -= FRAME_SLOTS;
	for (unsigned i = 0; i < FRAME_SLOTS; i++)
		sp[i] = 0;
	sp[LR_SLOT] = (u_int64_t)(uintptr_t)entry;
	return (u_int64_t)(uintptr_t)sp;
}

static void thread_a(void)
{
	for (int i = 0; i < 3; i++)
	{
		kprintf("  [thread A] iteration %d\n", i);
		aarch64_ctx_switch(&g_a_sp, g_b_sp); /* yield to B */
	}
	kprintf("  [thread A] done -> returning to main\n");
	aarch64_ctx_switch(&g_a_sp, g_main_sp);
}

static void thread_b(void)
{
	for (int i = 0; i < 3; i++)
	{
		kprintf("  [thread B] iteration %d\n", i);
		aarch64_ctx_switch(&g_b_sp, g_a_sp); /* yield to A */
	}
}

/**
 * Run the demo: start thread A; it bounces with B three times, then A returns
 * here.  Reaching the final line means the switch primitive works both ways.
 */
void aarch64_ctx_demo(void)
{
	kprintf("ctx demo: two kernel threads cooperatively switching...\n");
	g_a_sp = seed(g_a_stack, STACK_WORDS, thread_a);
	g_b_sp = seed(g_b_stack, STACK_WORDS, thread_b);
	aarch64_ctx_switch(&g_main_sp, g_a_sp); /* returns here when A switches back */
	kprintf("ctx demo: back in main - context switch works.\n");
}
