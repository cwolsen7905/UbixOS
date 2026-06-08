/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 exception handling (QEMU `virt` bring-up, Phase 12).
 *
 * vectors.S funnels every EL1 trap here as aarch64_exception(kind, frame).
 * Synchronous faults and unexpected vectors dump ESR/ELR/FAR and park the CPU
 * (so a fault is visible instead of a silent hang).  IRQs are routed to the GIC
 * dispatcher (Phase 12b); until that lands an IRQ is reported and parked.
 */

#include "bringup.h"
#include <ubixos/sched.h> /* _current — identify the faulting task in dumps */

enum
{
	EXC_INVALID = 0,
	EXC_SYNC = 1,
	EXC_IRQ = 2,
	EXC_EL0_SYNC = 3,
};

#define ESR_EC_SVC64 0x15 /* ESR_EL1 EC for an AArch64 SVC instruction */

/**
 * Read a system register by name into a u_int64_t.
 */
#define READ_SYSREG(reg)                                                                                               \
	({                                                                                                             \
		u_int64_t _v;                                                                                          \
		__asm__ volatile("mrs %0, " #reg : "=r"(_v));                                                          \
		_v;                                                                                                    \
	})

/**
 * Install the EL1 vector table base (VBAR_EL1) and synchronize.
 */
void aarch64_vbar_init(void)
{
	u_int64_t base = (u_int64_t)(uintptr_t)vectors_el1;
	__asm__ volatile("msr vbar_el1, %0; isb" : : "r"(base));
}

/**
 * C exception dispatcher called from every vector stub.  @kind is one of the
 * EXC_* values; @frame points at the saved GPRs (unused for now).
 */
void aarch64_exception(u_int64_t kind, void *frame)
{
	(void)frame;

	/* IRQs: hand off to the GIC dispatcher and resume (ERET in the stub). */
	if (kind == EXC_IRQ)
	{
		aarch64_irq_dispatch();
		return;
	}

	/* Synchronous exception from EL0 — the only expected cause is an SVC. */
	if (kind == EXC_EL0_SYNC)
	{
		u_int64_t esr = READ_SYSREG(esr_el1);
		u_int64_t *gpr = (u_int64_t *)frame; /* saved x0..x30 (KERNEL_ENTRY layout) */

		if (((esr >> 26) & 0x3f) == ESR_EC_SVC64)
		{
			/* x8 = syscall number; x0..x5 (gpr[0..5]) are the arguments.
			 * The return value lands in x0 via KERNEL_EXIT (exit never returns). */
			gpr[0] = aarch64_syscall(gpr[8], gpr);
			return; /* ERET back to EL0 */
		}

		/* Any other EL0 sync cause (fault, undef) falls through to the dump. */
	}

	u_int64_t esr = READ_SYSREG(esr_el1);
	u_int64_t elr = READ_SYSREG(elr_el1);
	u_int64_t far = READ_SYSREG(far_el1);

	if (kind == EXC_SYNC)
		kprintf("\n*** aarch64 synchronous exception ***\n");
	else
		kprintf("\n*** aarch64 unexpected/invalid vector ***\n");

	kprintf("  ESR_EL1=0x%lx  EC=0x%lx  ELR_EL1=0x%lx  FAR_EL1=0x%lx\n", esr, (esr >> 26) & 0x3f, elr, far);
	if (_current != 0)
		kprintf("  task: pid=%d name=%s state=%d md_entry=0x%lx md_usp=0x%lx md_ttbr0=0x%lx\n",
		        _current->id,
		        _current->name,
		        _current->state,
		        _current->md.md_entry,
		        _current->md.md_usp,
		        _current->md.md_ttbr0);
	if (frame != 0)
	{
		u_int64_t *g = (u_int64_t *)frame;
		kprintf("  frame: x30(lr)=0x%lx elr=0x%lx spsr=0x%lx sp_el0=0x%lx\n", g[30], g[32], g[33], g[34]);
	}

	for (;;)
		__asm__ volatile("wfi");
}
