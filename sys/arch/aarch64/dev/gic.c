/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * GICv2 interrupt controller (QEMU `virt` bring-up, Phase 12b).
 *
 * `virt` exposes a GICv2: distributor (GICD) at 0x08000000, CPU interface
 * (GICC) at 0x08010000.  Boot QEMU with `-machine virt,gic-version=2` so this
 * driver matches (the HVF default on Apple Silicon can otherwise be GICv3).
 * This is the minimal subset: enable, set a per-INTID priority, ack (IAR) and
 * end-of-interrupt (EOIR).  Replaced by a real driver as aarch64 matures.
 */

#include "bringup.h"
#include <ubixos/sched.h> /* sched() — timer-driven preemption */

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

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

/* SMP reschedule IPI: SGI 0.  arch_smp_reschedule() (apsmp.c) sends it to wake an
 * idle secondary out of wfi so it re-runs sched_yield() and picks up newly enqueued
 * work — the aarch64 analogue of x86_64's 0xFD reschedule IPI.  The wake IS the
 * point; the handler just EOIs (no preempt — APs stay cooperative). */
#define RESCHED_SGI 0

/**
 * Bring up the GICv2: enable the distributor and CPU interface and unmask all
 * priorities (PMR = 0xFF — an interrupt fires when its priority < PMR).
 */
void gic_init(void)
{
	GICD(GICD_CTLR) = 1;   /* enable distributor */
	GICC(GICC_PMR) = 0xFF; /* allow every priority */
	GICC(GICC_CTLR) = 1;   /* enable CPU interface */
	gic_enable_intid(RESCHED_SGI); /* receive the reschedule IPI (banked GICD_ISENABLER0) */
}

/**
 * smp-plan M2: per-CPU GIC init for an application processor.  The distributor
 * (GICD) is global and already enabled by the BSP; an AP only brings up its own
 * (banked) CPU interface — priority mask + enable.
 */
void gic_secondary_init(void)
{
	GICC(GICC_PMR) = 0xFF;
	GICC(GICC_CTLR) = 1;
	gic_enable_intid(RESCHED_SGI); /* this AP must receive the reschedule IPI (banked) */
}

/**
 * Send the reschedule IPI (RESCHED_SGI) to a single CPU @cpu.
 *
 * GICD_SGIR with TargetListFilter=0 (use CPUTargetList) and CPUTargetList = 1<<cpu.
 * On QEMU `virt` the GIC CPU-interface number equals the cpu index, so a cpuid maps
 * straight to its target bit.  The dsb orders the run-queue enqueue (done by the
 * caller under schedulerSpinLock) before the wake so the woken CPU sees the work.
 */
void aarch64_gic_send_resched(unsigned cpu)
{
	__asm__ volatile("dsb ish" ::: "memory");
	GICD(GICD_SGIR) = ((1u << (cpu & 0xFF)) << 16) | (RESCHED_SGI & 0x0F);
}

/**
 * Enable @intid and give it a high priority (0) so PMR never masks it.
 * INTID < 32 (SGI/PPI) targets this CPU via GICD_ISENABLER0.
 */
void gic_enable_intid(unsigned intid)
{
	GICD_PRIO(intid) = 0x00;
	GICD(GICD_ISENABLER + (intid / 32) * 4) = (1u << (intid % 32));
}

/**
 * IRQ dispatch (called from the EL1 IRQ vector): ack via IAR, route by INTID,
 * then signal end-of-interrupt via EOIR.
 *
 * @return non-zero if a timer tick occurred (the caller, aarch64_exception,
 *         reschedules on a tick — but only when EL0 was interrupted, so the
 *         kernel itself runs non-preemptibly).
 */
int aarch64_irq_dispatch(void)
{
	u_int32_t iar = GICC(GICC_IAR);
	u_int32_t intid = iar & 0x3FF;

	if (intid >= GICC_SPURIOUS)
		return (0); /* spurious — no EOI */

	if (intid == 27) /* EL1 virtual (generic) timer PPI */
		timer_tick();
	else if (intid == RESCHED_SGI) /* SMP reschedule IPI — the wake itself is the work */
		;                      /* fall through to EOI; the AP re-runs sched_yield after ERET */
	else
		kprintf("irq: unexpected intid %u\n", intid);

	GICC(GICC_EOIR) = iar; /* EOI before any context switch */

	/* The reschedule is driven by the caller (after EOI, so the GIC priority is
	 * already dropped) and ONLY when the tick interrupted EL0 — see
	 * aarch64_exception.  This makes the kernel non-preemptible: an EL1 context
	 * busy-polling a virtio ring during bring-up runs to its next voluntary
	 * sched_yield() instead of being preempted mid-spin and left unresumed. */
	return (intid == 27);
}
