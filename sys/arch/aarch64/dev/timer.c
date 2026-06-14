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
#include <ubixos/sched.h>  /* sched_account_tick (Phase 3.5) */
#include <aarch64/pcpu.h>  /* curcpu() — per-CPU tick (smp-plan M2) */

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

/**
 * md_uptime (aarch64): sample the always-running virtual counter (CNTVCT_EL0)
 * as monotonic time-since-boot — the machine-dependent time source the shared
 * gettimeofday / clock_gettime / lwIP sys_now all build on.  The counter
 * advances at CNTFRQ_EL0 Hz independently of the 100 Hz scheduler tick, so it
 * backs userland clocks with one that actually advances (without it,
 * timing-driven programs such as the DOOM main loop never advance a game tic).
 *
 * The split avoids 64-bit overflow: whole seconds plus a sub-second remainder
 * scaled to nanoseconds.
 *
 * @param sec   out: whole seconds since boot.
 * @param nsec  out: nanosecond remainder in [0, 1e9).
 */
void md_uptime(u_int64_t *sec, u_int64_t *nsec)
{
	u_int64_t cnt, freq;
	__asm__ volatile("mrs %0, cntvct_el0" : "=r"(cnt));
	freq = read_cntfrq();
	if (freq == 0)
	{
		*sec = 0;
		*nsec = 0;
		return;
	}
	*sec = cnt / freq;
	*nsec = ((cnt % freq) * 1000000000ULL) / freq;
}

/* PL031 RTC on the QEMU `virt` board: its data register (offset 0) reads the
 * current wall-clock time as seconds since the Unix epoch. */
#define PL031_BASE 0x09010000UL
#define PL031_DR (*(volatile u_int32_t *)(PL031_BASE + 0x00))

/**
 * Capture the boot wall-clock epoch from the PL031 RTC into
 * systemVitals->timeStart.  gettimeofday() returns timeStart + md_uptime(), so
 * without this the clock starts at 1970-01-01 00:00:00 (the i386 path reads the
 * CMOS RTC in time_init(), which is x86-only — guarded out on aarch64).
 */
void aarch64_rtc_init(void)
{
	if (systemVitals != 0)
	{
		systemVitals->timeStart = PL031_DR;
		kprintf("rtc: PL031 boot epoch = %u s (wall clock = this + uptime)\n", systemVitals->timeStart);
	}
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
	/* Only the BSP prints — a concurrent AP kprintf races the unlocked console and
	 * garbles output (a console spinlock is smp-plan M4). */
	if (curcpu()->cpuid == 0)
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
	/*
	 * Per-CPU tick (smp-plan M2).  Only the BSP drives the shared scheduler clock
	 * (g_ticks / sysTicks / accounting) — an AP doing so would double-count the
	 * quantum/aging clock.  An AP instead bumps its own heartbeat so the BSP can
	 * confirm its per-CPU GIC + timer fire.  Both re-arm their own (banked) CNTV.
	 */
	if (curcpu()->cpuid == 0)
	{
		g_ticks++;
		if (systemVitals != 0)
			systemVitals->sysTicks++; /* the scheduler's quantum/aging clock */
		sched_account_tick();             /* Phase 3.5: charge the tick to the running thread */
	}
	else
	{
		curcpu()->heartbeat++; /* AP liveness, driven by its own timer IRQ */
	}
	write_tval(g_interval); /* re-arm for the next interval (banked, per-CPU) */
}
