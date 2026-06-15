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
static void pic_unmask(unsigned irq)
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

	pic_unmask(0); /* IRQ0 = the timer */
}

/** Signal end-of-interrupt to the PIC(s) for a handled IRQ vector. */
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
 * Counts the timer tick (IRQ0) and sends EOI.  Other lines are masked for now.
 */
void x86_64_irq(unsigned vector)
{
	if (vector == IRQ_BASE) /* IRQ0 — PIT timer */
	{
		extern void sched_account_tick(void);
		extern vitalsNode *systemVitals;
		g_ticks++;
		if (systemVitals != 0)
			systemVitals->sysTicks++; /* MI time base (scheduler aging, callouts) */
		/* Charge the elapsed tick to whatever was running.  The kernel is
		 * non-preemptive here (like aarch64: no EL0/userland yet), so we do
		 * not reschedule from IRQ context — kernel threads cooperatively
		 * yield.  Once userland lands, the EL0 timer path will call sched(). */
		sched_account_tick();
	}
	irq_eoi(vector);
}
