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

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

#define GICD(off) (*(volatile uint32_t *)(GICD_BASE + (off)))
#define GICC(off) (*(volatile uint32_t *)(GICC_BASE + (off)))
#define GICD_PRIO(intid) (*(volatile uint8_t *)(GICD_BASE + 0x400 + (intid)))

#define GICD_CTLR 0x000
#define GICD_ISENABLER 0x100
#define GICC_CTLR 0x000
#define GICC_PMR 0x004
#define GICC_IAR 0x00C
#define GICC_EOIR 0x010

#define GICC_SPURIOUS 1020 /* INTIDs >= 1020 are spurious */

/**
 * Bring up the GICv2: enable the distributor and CPU interface and unmask all
 * priorities (PMR = 0xFF — an interrupt fires when its priority < PMR).
 */
void gic_init(void)
{
	GICD(GICD_CTLR) = 1;   /* enable distributor */
	GICC(GICC_PMR) = 0xFF; /* allow every priority */
	GICC(GICC_CTLR) = 1;   /* enable CPU interface */
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
 */
void aarch64_irq_dispatch(void)
{
	uint32_t iar = GICC(GICC_IAR);
	uint32_t intid = iar & 0x3FF;

	if (intid >= GICC_SPURIOUS)
		return; /* spurious — no EOI */

	if (intid == 30) /* EL1 physical (generic) timer PPI */
		timer_tick();
	else
		kprintf("irq: unexpected intid %u\n", intid);

	GICC(GICC_EOIR) = iar;
}
