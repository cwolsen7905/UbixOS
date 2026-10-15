/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 8259 PIC + 8253/8254 PIT + IRQ dispatch (bring-up Phase 4a).
 *
 * Remaps the legacy PICs so the 16 hardware IRQs land at IDT vectors 32..47
 * (clear of the CPU exceptions 0..31), programs the PIT for a periodic tick, and
 * dispatches IRQs from the common ISR path (idt.c) — sending the PIC EOI.  This
 * is the timer/interrupt foundation the scheduler (Phase 4b) rides on; the APIC
 * comes with SMP later.
 */

#include "x86_64.h"
#include <ubixos/vitals.h>
#include <lib/kprintf.h>

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIT_HZ 1193182u /* base frequency */

#define IRQ_BASE 32 /* vectors 32..47 */

static volatile u64 g_ticks;

/**
 * Remap the master/slave PICs to vector base 32/40 and mask every line (the
 * timer is unmasked in pit_init).  Standard ICW1..ICW4 sequence.
 */
void pic_remap(void)
{
	u8 mask1 = inb(PIC1_DATA);
	u8 mask2 = inb(PIC2_DATA);

	outb(PIC1_CMD, 0x11); /* ICW1: init + ICW4 */
	io_wait();
	outb(PIC2_CMD, 0x11);
	io_wait();
	outb(PIC1_DATA, IRQ_BASE); /* ICW2: master vector offset = 32 */
	io_wait();
	outb(PIC2_DATA, IRQ_BASE + 8); /* slave vector offset = 40 */
	io_wait();
	outb(PIC1_DATA, 0x04); /* ICW3: slave on IRQ2 */
	io_wait();
	outb(PIC2_DATA, 0x02);
	io_wait();
	outb(PIC1_DATA, 0x01); /* ICW4: 8086 mode */
	io_wait();
	outb(PIC2_DATA, 0x01);
	io_wait();

	(void)mask1;
	(void)mask2;
	outb(PIC1_DATA, 0xFF); /* mask all master lines  */
	outb(PIC2_DATA, 0xFF); /* mask all slave lines   */
}

/** Unmask a single IRQ line (0..15) on the PIC. */
void irq_unmask(unsigned irq)
{
	u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	u8 line = (irq < 8) ? irq : (irq - 8);
	outb(port, (u8)(inb(port) & ~(1u << line)));
}

/** Program PIT channel 0 for a periodic @hz tick and unmask IRQ0. */
void pit_init(unsigned hz)
{
	unsigned divisor = (hz != 0) ? (PIT_HZ / hz) : 0;
	if (divisor == 0 || divisor > 0xFFFF)
		divisor = 0xFFFF;

	outb(PIT_CMD, 0x36); /* ch0, lo/hi byte, mode 3 (square wave), binary */
	outb(PIT_CH0, (u8)(divisor & 0xFF));
	outb(PIT_CH0, (u8)((divisor >> 8) & 0xFF));

	irq_unmask(0); /* IRQ0 = the timer */
}

/** Signal end-of-interrupt to the PIC(s) for a handled IRQ vector. */
/* --- TSC clock source ------------------------------------------------------
 * Counting PIT interrupts is not a clock: under emulation (QEMU TCG — x86 on an
 * ARM host) and whenever IRQs sit masked, IRQ0s coalesce and are lost, so a
 * tick-counted clock ran at ~40% speed (date fell minutes behind, sleep 5 took
 * 30 s).  Like aarch64's CNTVCT, x86_64 now reads time from a free-running
 * counter: the TSC, calibrated once against PIT channel 2 (polled, so lost
 * interrupts cannot skew it).  The tick ISR advances sysTicks by the elapsed
 * TSC time, catching up on lost ticks, so callout deadlines stay real-time. */
#define PIT_CH2 0x42
#define PIT_PORTB 0x61
#define TSC_CAL_MS 50

static u64 g_tsc_hz;        /* TSC ticks per second (0 = not calibrated) */
static u64 g_tsc_base;      /* TSC at tick 0 of the TSC-driven tick count */
static u64 g_tsc_ticked;    /* scheduler ticks accounted since g_tsc_base */
static u32 g_systicks_base; /* sysTicks when the TSC took over */

static inline u64 rdtsc64(void)
{
	u32 lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((u64)hi << 32) | lo;
}

/**
 * Measure the TSC rate against a TSC_CAL_MS one-shot on PIT channel 2 (gate via
 * port 0x61, OUT2 readable in bit 5).  Call once, before the TSC is used; IRQs
 * may be on.  Leaves the PC speaker disconnected.
 */
void x86_64_tsc_calibrate(void)
{
	u32 count = (PIT_HZ * TSC_CAL_MS) / 1000;
	u64 t0, t1;
	u8 b = inb(PIT_PORTB);

	outb(PIT_PORTB, (u8)((b & ~0x02) | 0x01)); /* gate ch2 on, speaker data off */
	outb(PIT_CMD, 0xB0);                       /* ch2, lo/hi byte, mode 0, binary */
	outb(PIT_CH2, (u8)(count & 0xFF));
	t0 = rdtsc64();
	outb(PIT_CH2, (u8)((count >> 8) & 0xFF)); /* counting starts on the high byte */
	while ((inb(PIT_PORTB) & 0x20) == 0)
		;
	t1 = rdtsc64();
	outb(PIT_PORTB, b & ~0x03);

	if (t1 <= t0)
		return; /* no usable TSC: stay on tick counting */
	g_tsc_hz = (t1 - t0) * (1000 / TSC_CAL_MS);
	g_systicks_base = 0;
	g_tsc_ticked = 0;
	g_tsc_base = rdtsc64();
	{
		extern vitalsNode *systemVitals;
		if (systemVitals != 0)
			g_systicks_base = systemVitals->sysTicks;
	}
	kprintf("tsc: %u kHz (calibrated against PIT ch2)\n", (u32)(g_tsc_hz / 1000));
}

/*
 * md_uptime (x86_64): monotonic time since boot from the calibrated TSC, at
 * nanosecond resolution; falls back to the 100 Hz tick count before calibration.
 * Uptime = sysTicks at hand-over (whole ticks) + TSC time since.
 */
void md_uptime(u_int64_t *sec, u_int64_t *nsec)
{
	extern vitalsNode *systemVitals;
	u64 s, ns;

	if (g_tsc_hz == 0)
	{
		u32 ticks = (systemVitals != 0) ? systemVitals->sysTicks : 0;
		s = ticks / 100u;
		ns = (u64)(ticks % 100u) * 10000000ULL;
	}
	else
	{
		u64 d = rdtsc64() - g_tsc_base;
		u64 base_ns = (u64)g_systicks_base * 10000000ULL;
		s = d / g_tsc_hz;
		ns = ((d % g_tsc_hz) * 1000000000ULL) / g_tsc_hz + base_ns % 1000000000ULL;
		s += base_ns / 1000000000ULL + ns / 1000000000ULL;
		ns %= 1000000000ULL;
	}
	if (sec != 0)
		*sec = s;
	if (nsec != 0)
		*nsec = ns;
}

/* How many scheduler ticks this IRQ0 stands for: the real 10 ms periods elapsed
 * since the last one per the TSC (>1 after lost interrupts, 0 if this one came
 * early), or 1 before calibration. */
static u32 tsc_ticks_due(void)
{
	u64 due;
	u32 n;

	if (g_tsc_hz == 0)
		return 1;
	due = ((rdtsc64() - g_tsc_base) * 100u) / g_tsc_hz;
	if (due <= g_tsc_ticked)
		return 0;
	n = (u32)(due - g_tsc_ticked);
	g_tsc_ticked = due;
	return n;
}

void irq_eoi(unsigned vector)
{
	unsigned irq = vector - IRQ_BASE;
	if (irq >= 8)
		outb(PIC2_CMD, PIC_EOI); /* slave first */
	outb(PIC1_CMD, PIC_EOI);
}

u64 timer_ticks(void)
{
	return g_ticks;
}

/**
 * IRQ handler (called from the common ISR path in idt.c for vector >= 32).
 * Counts the timer tick (IRQ0), charges it to the running task, and sends EOI.
 * Other lines are masked for now.
 *
 * @return 1 if this was a timer tick (IRQ0), so the caller can decide whether to
 *         preempt; 0 otherwise.  EOI is already sent on return, so the caller may
 *         reschedule safely.
 */
int x86_64_irq(unsigned vector)
{
	int ticked = 0;

	if (vector == IRQ_BASE) /* IRQ0 — PIT timer */
	{
		extern void sched_account_tick(void);
		extern vitalsNode *systemVitals;
		g_ticks++;
		if (systemVitals != 0)
			systemVitals->sysTicks +=
			    tsc_ticks_due(); /* MI time base (aging, callouts), lost ticks caught up */
		vitals_tick_uptime();        /* seconds since boot, for the wall clock */
		/* Charge the elapsed tick to whatever was running. */
		sched_account_tick();
		ticked = 1;
	}
	else if (vector == IRQ_BASE + 4) /* IRQ4 — COM1 */
	{
		if (serial_irq() > 0)
			x86_64_console_rx_wake();
	}
	irq_eoi(vector);
	return ticked;
}
