/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * ARM generic timer (EL1 virtual timer, CNTV) — QEMU `virt` bring-up, Phase 12b.
 *
 * The virtual timer raises PPI INTID 27 on the GIC.  We program a periodic tick
 * by re-arming CNTV_TVAL_EL0 each interrupt.  The *virtual* timer (not the EL0
 * physical timer, CNTP) is used because the Apple-Silicon HVF accelerator traps
 * EL1 access to the physical timer registers — `msr cntp_tval_el0` faults under
 * `-accel hvf`.  The virtual timer is freely accessible and is the standard
 * guest tick.  This is the arch tick the (arch-neutral) scheduler/callout
 * subsystem rides on.
 */

#include "bringup.h"
#include <ubixos/vitals.h> /* systemVitals->sysTicks */

#define TIMER_INTID 27 /* EL1 virtual timer PPI */

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
	__asm__ volatile("msr cntv_tval_el0, %0" : : "r"(v));
}

static void write_ctl(u_int64_t v)
{
	__asm__ volatile("msr cntv_ctl_el0, %0" : : "r"(v));
}

/**
 * Program the EL1 virtual timer for a 100 Hz periodic tick and enable its GIC
 * interrupt.  Call after gic_init() and before unmasking IRQs.
 */
void timer_init(void)
{
	u_int64_t freq = read_cntfrq();
	g_interval = freq / 100; /* 100 Hz scheduler tick */
	kprintf("timer: cntfrq=%lu Hz, tick interval=%lu counts (100 Hz)\n", freq, g_interval);

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
	if (systemVitals != 0)
		systemVitals->sysTicks++; /* the scheduler's quantum/aging clock */
	write_tval(g_interval);           /* re-arm for the next interval */
}
