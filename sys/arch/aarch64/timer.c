/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * ARM generic timer (EL1 physical timer, CNTP) — QEMU `virt` bring-up, Phase 12b.
 *
 * The EL1 physical timer raises PPI INTID 30 on the GIC.  We program a periodic
 * tick by re-arming CNTP_TVAL_EL0 each interrupt.  This is the arch tick that
 * the (arch-neutral) scheduler/callout subsystem will ride on once aarch64
 * scheduling lands; for now it just proves interrupts fire.
 */

#include "bringup.h"

#define TIMER_INTID 30 /* EL1 physical timer PPI */

static u_int64_t g_interval; /* counts per tick */
static unsigned g_ticks;

/**
 * Read the timer frequency (Hz) the platform reports in CNTFRQ_EL0.
 */
static u_int64_t read_cntfrq(void)
{
	u_int64_t v;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
	return v;
}

static void write_tval(u_int64_t v)
{
	__asm__ volatile("msr cntp_tval_el0, %0" : : "r"(v));
}

static void write_ctl(u_int64_t v)
{
	__asm__ volatile("msr cntp_ctl_el0, %0" : : "r"(v));
}

/**
 * Program the EL1 physical timer for a ~2 Hz periodic tick and enable its GIC
 * interrupt.  Call after gic_init() and before unmasking IRQs.
 */
void timer_init(void)
{
	u_int64_t freq = read_cntfrq();
	g_interval = freq / 2; /* 2 Hz */
	kprintf("timer: cntfrq=%lu Hz, tick interval=%lu counts (~2 Hz)\n", freq, g_interval);

	gic_enable_intid(TIMER_INTID);
	write_tval(g_interval);
	write_ctl(1); /* ENABLE=1, IMASK=0 */
}

/**
 * Timer IRQ handler (from aarch64_irq_dispatch): count the tick and re-arm.
 */
void timer_tick(void)
{
	g_ticks++;
	kprintf("timer tick #%u\n", g_ticks);
	write_tval(g_interval); /* re-arm for the next interval */
}
