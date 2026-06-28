/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Board-neutral interrupt-controller shims (M1 board abstraction): the kernel
 * calls these; they delegate to the active board's implementation (g_board->intc).
 * QEMU virt selects g_gicv2_intc (gic.c); the Raspberry Pi 3 selects a BCM2837
 * "ARM local" controller.  See docs/design/raspberry-pi-3b-bringup.md.
 */

#include "bringup.h"

/**
 * Bring up the interrupt controller on the boot CPU.
 */
void aarch64_intc_init(void)
{
	g_board->intc->init();
}

/**
 * Per-CPU interrupt-controller bring-up for an application processor.
 */
void aarch64_intc_secondary_init(void)
{
	g_board->intc->secondary_init();
}

/**
 * Enable the running CPU's generic-timer interrupt (called from timer_init).
 */
void aarch64_intc_timer_enable(void)
{
	g_board->intc->timer_enable();
}

/**
 * Send the reschedule IPI to @cpu (wake it out of wfi to re-run the scheduler).
 */
void aarch64_intc_send_resched(unsigned cpu)
{
	g_board->intc->send_resched(cpu);
}

/**
 * EL1 IRQ vector entry: ack/route/EOI the pending interrupt.
 *
 * @return non-zero if a timer tick occurred (the caller reschedules on a tick,
 *         but only when EL0 was interrupted — the kernel stays non-preemptible).
 */
int aarch64_irq_dispatch(void)
{
	return (g_board->intc->dispatch());
}
