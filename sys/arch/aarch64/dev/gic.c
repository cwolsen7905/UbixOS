/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * GICv2 interrupt controller (QEMU `virt`) — one implementation of the M1
 * interrupt-controller abstraction (struct aarch64_intc, bringup.h).  Exposed as
 * g_gicv2_intc and selected by board_qemu_virt; the kernel reaches it through the
 * board-neutral aarch64_intc_* shims (dev/intc.c).  The Pi 3 selects a separate
 * BCM2837 "ARM local" controller instead (gic.c does not transfer).
 *
 * `virt` exposes a GICv2: distributor (GICD) + CPU interface (GICC), whose bases
 * come from g_board.  Boot QEMU with `-machine virt,gic-version=2` so this driver
 * matches (the HVF default on Apple Silicon can otherwise be GICv3).  Minimal
 * subset: enable, per-INTID priority, ack (IAR) and end-of-interrupt (EOIR).
 */

#include "bringup.h"
#include <ubixos/sched.h> /* sched() — timer-driven preemption */

/* Reach the GIC MMIO through the TTBR1 physmap (Phase 4: TTBR0 user-only); the
 * per-board physical bases live in g_board.  Block 0 is DEVICE-mapped (mmu.c). */
#define GICD_BASE (PHYSMAP_BASE + g_board->gicd_base)
#define GICC_BASE (PHYSMAP_BASE + g_board->gicc_base)

#define GICD(off) (*(volatile u_int32_t *)(GICD_BASE + (off)))
#define GICC(off) (*(volatile u_int32_t *)(GICC_BASE + (off)))
#define GICD_PRIO(intid) (*(volatile u_int8_t *)(GICD_BASE + 0x400 + (intid)))

#define GICD_CTLR 0x000
#define GICD_ISENABLER 0x100
#define GICD_SGIR 0xF00 /* Software-Generated Interrupt Register (write-only) */
#define GICC_CTLR 0x000
#define GICC_PMR 0x004
#define GICC_IAR 0x00C
#define GICC_EOIR 0x010

#define GICC_SPURIOUS 1020 /* INTIDs >= 1020 are spurious */
#define GIC_TIMER_INTID 27 /* EL1 virtual (generic) timer PPI */

/* SMP reschedule IPI: SGI 0.  Sent to wake an idle secondary out of wfi so it
 * re-runs sched_yield() and picks up newly enqueued work.  The wake IS the point;
 * the handler just EOIs (no preempt — APs stay cooperative). */
#define RESCHED_SGI 0

/**
 * Enable @intid and give it a high priority (0) so PMR never masks it.
 * INTID < 32 (SGI/PPI) targets this CPU via GICD_ISENABLER0.
 */
static void gicv2_enable_intid(unsigned intid)
{
	GICD_PRIO(intid) = 0x00;
	GICD(GICD_ISENABLER + (intid / 32) * 4) = (1u << (intid % 32));
}

/**
 * Bring up the GICv2: enable the distributor and CPU interface and unmask all
 * priorities (PMR = 0xFF — an interrupt fires when its priority < PMR).
 */
static void gicv2_init(void)
{
	GICD(GICD_CTLR) = 1;             /* enable distributor */
	GICC(GICC_PMR) = 0xFF;           /* allow every priority */
	GICC(GICC_CTLR) = 1;             /* enable CPU interface */
	gicv2_enable_intid(RESCHED_SGI); /* receive the reschedule IPI (banked GICD_ISENABLER0) */
}

/**
 * smp-plan M2: per-CPU GIC init for an application processor.  The distributor
 * (GICD) is global and already enabled by the BSP; an AP only brings up its own
 * (banked) CPU interface — priority mask + enable.
 */
static void gicv2_secondary_init(void)
{
	GICC(GICC_PMR) = 0xFF;
	GICC(GICC_CTLR) = 1;
	gicv2_enable_intid(RESCHED_SGI); /* this AP must receive the reschedule IPI (banked) */
}

/**
 * Enable this CPU's generic-timer interrupt (the EL1 virtual timer PPI).
 */
static void gicv2_timer_enable(void)
{
	gicv2_enable_intid(GIC_TIMER_INTID);
}

/**
 * Send the reschedule IPI (RESCHED_SGI) to a single CPU @cpu via GICD_SGIR
 * (TargetListFilter=0, CPUTargetList = 1<<cpu).  The dsb orders the run-queue
 * enqueue (done by the caller under schedulerSpinLock) before the wake.
 */
static void gicv2_send_resched(unsigned cpu)
{
	__asm__ volatile("dsb ish" ::: "memory");
	GICD(GICD_SGIR) = ((1u << (cpu & 0xFF)) << 16) | (RESCHED_SGI & 0x0F);
}

/**
 * IRQ dispatch (called via aarch64_irq_dispatch from the EL1 IRQ vector): ack via
 * IAR, route by INTID, then signal end-of-interrupt via EOIR.
 *
 * @return non-zero if a timer tick occurred (the caller reschedules on a tick, but
 *         only when EL0 was interrupted, so the kernel itself runs non-preemptibly).
 */
static int gicv2_dispatch(void)
{
	u_int32_t iar = GICC(GICC_IAR);
	u_int32_t intid = iar & 0x3FF;

	if (intid >= GICC_SPURIOUS)
		return (0); /* spurious — no EOI */

	if (intid == GIC_TIMER_INTID) /* EL1 virtual (generic) timer PPI */
		timer_tick();
	else if (intid == RESCHED_SGI) /* SMP reschedule IPI — the wake itself is the work */
		;                      /* fall through to EOI; the AP re-runs sched_yield after ERET */
	else
		kprintf("irq: unexpected intid %u\n", intid);

	GICC(GICC_EOIR) = iar; /* EOI before any context switch */

	return (intid == GIC_TIMER_INTID);
}

/* The GICv2 implementation of the interrupt-controller abstraction. */
const struct aarch64_intc g_gicv2_intc = {
    .init = gicv2_init,
    .secondary_init = gicv2_secondary_init,
    .timer_enable = gicv2_timer_enable,
    .send_resched = gicv2_send_resched,
    .dispatch = gicv2_dispatch,
};
