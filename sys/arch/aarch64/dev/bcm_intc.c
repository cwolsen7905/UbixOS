/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * BCM2837 "ARM local" interrupt controller (Raspberry Pi 3) — the Pi's
 * implementation of the M1 interrupt-controller abstraction (struct aarch64_intc,
 * bringup.h).  Exposed as g_bcm_intc and selected by board_rpi3.  The Pi has NO
 * GIC; the per-core "ARM local" block at 0x40000000 routes the architected-timer
 * IRQs (and inter-core mailboxes) to each core.  Confirmed on hardware in the M0.75
 * standalone (board/rpi3b); here it routes the *virtual* timer (CNTVIRQ) because
 * the shared timer.c drives CNTV.  See docs/design/raspberry-pi-3b-bringup.md.
 */

#ifdef BOARD_RPI3

#include "bringup.h"
#include <ubixos/sched.h>
#include <aarch64/pcpu.h> /* curcpu() — per-core IRQ source/routing registers */

/* Per-core "ARM local" registers (reached through the TTBR1 physmap). */
#define LOCAL_BASE (PHYSMAP_BASE + 0x40000000UL)
#define CORE_TIMER_IRQCNTL(c) (*(volatile u_int32_t *)(LOCAL_BASE + 0x40 + 4 * (c))) /* timers->IRQ */
#define CORE_IRQ_SOURCE(c) (*(volatile u_int32_t *)(LOCAL_BASE + 0x60 + 4 * (c)))    /* pending IRQs */
#define LOCAL_CNTVIRQ (1u << 3) /* bit 3: virtual-timer (CNTV) -> IRQ */

/**
 * Controller bring-up.  The BCM local block has no global distributor to enable
 * (unlike the GIC); per-core routing happens in timer_enable, so this is a no-op
 * on the boot CPU for now.  (Inter-core mailbox IPIs come with Pi SMP.)
 */
static void bcm_init(void)
{
}

/**
 * Per-CPU bring-up for an application processor (Pi SMP is a later milestone).
 */
static void bcm_secondary_init(void)
{
}

/**
 * Route the running core's virtual-timer IRQ (CNTVIRQ) to its IRQ line.
 */
static void bcm_timer_enable(void)
{
	CORE_TIMER_IRQCNTL(curcpu()->cpuid) = LOCAL_CNTVIRQ;
}

/**
 * Send the reschedule IPI to @cpu — Pi SMP (mailbox IPIs) is a later milestone.
 */
static void bcm_send_resched(unsigned cpu)
{
	(void)cpu;
}

/**
 * IRQ dispatch from the EL1 vector: read this core's IRQ source; on a virtual-timer
 * interrupt run the tick (timer_tick re-arms CNTV_TVAL, which clears the condition —
 * the BCM controller has no separate EOI for the timer line).
 *
 * @return non-zero on a timer tick.
 */
static int bcm_dispatch(void)
{
	if ((CORE_IRQ_SOURCE(curcpu()->cpuid) & LOCAL_CNTVIRQ) != 0)
	{
		timer_tick();
		return (1);
	}
	return (0);
}

/* The BCM2837 implementation of the interrupt-controller abstraction. */
const struct aarch64_intc g_bcm_intc = {
    .init = bcm_init,
    .secondary_init = bcm_secondary_init,
    .timer_enable = bcm_timer_enable,
    .send_resched = bcm_send_resched,
    .dispatch = bcm_dispatch,
};

#endif /* BOARD_RPI3 */
