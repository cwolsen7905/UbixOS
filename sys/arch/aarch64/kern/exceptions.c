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
#include <ubixos/sched.h>       /* _current — identify the faulting task in dumps */
#include <ubixos/endtask.h>     /* endTask — terminate a faulting user process */
#include <ubixos/signal.h>      /* signal_check / sys_sigreturn + AARCH64_SIGTRAMP_RETADDR */
#include <sys/trap.h>           /* struct trapframe (aarch64 layout) */
#include <sys/sysproto_posix.h> /* struct sys_sigreturn_args */

enum
{
	EXC_INVALID = 0,
	EXC_SYNC = 1,
	EXC_IRQ = 2,
	EXC_EL0_SYNC = 3,
};

#define ESR_EC_SVC64 0x15    /* ESR_EL1 EC for an AArch64 SVC instruction */
#define ESR_EC_IABT_LOW 0x20 /* instruction abort taken from a lower EL (EL0) */
#define SPSR_M_MASK 0x0full  /* PSTATE mode bits; 0b0000 == EL0t (was at EL0) */

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

	/* IRQs: hand off to the GIC dispatcher and resume (ERET in the stub).  If we
	 * preempted EL0 (timer tick), deliver any pending signal on the way out so a
	 * Ctrl-C reaches a CPU-bound foreground program, not just one blocked in a
	 * syscall. */
	if (kind == EXC_IRQ)
	{
		struct trapframe *tf = (struct trapframe *)frame;
		aarch64_irq_dispatch();
		if (_current != 0 && (tf->tf_spsr & SPSR_M_MASK) == 0)
		{
			_current->td.frame = tf;
			signal_check(tf);
		}
		return;
	}

	/* Synchronous exception from EL0 — normally an SVC, or a returning signal
	 * handler faulting on the magic trampoline address. */
	if (kind == EXC_EL0_SYNC)
	{
		u_int64_t esr = READ_SYSREG(esr_el1);
		u_int64_t ec = (esr >> 26) & 0x3f;
		struct trapframe *tf = (struct trapframe *)frame;
		u_int64_t *gpr = (u_int64_t *)frame; /* saved x0..x30 (KERNEL_ENTRY layout) */

		if (_current != 0)
			_current->td.frame = tf;

		if (ec == ESR_EC_SVC64)
		{
			/* x8 = syscall number; x0..x5 (gpr[0..5]) are the arguments.  The
			 * return value lands in x0 via KERNEL_EXIT (exit never returns). */
			gpr[0] = aarch64_syscall(gpr[8], gpr);
			if (_current != 0)
				signal_check(tf); /* may rewrite tf to enter a handler */
			return;                   /* ERET back to EL0 */
		}

		/* A returning signal handler `ret`s to AARCH64_SIGTRAMP_RETADDR; the
		 * instruction fetch faults here.  The user SP is back at the sigframe, so
		 * scp == sp_el0 — restore the interrupted context and resume it. */
		if (ec == ESR_EC_IABT_LOW && tf->tf_elr == AARCH64_SIGTRAMP_RETADDR && _current != 0)
		{
			struct sys_sigreturn_args a = {.scp = (struct ubx_sigcontext *)(uintptr_t)tf->tf_sp};
			sys_sigreturn(&_current->td, &a);
			signal_check(tf); /* deliver another queued signal, if any */
			return;
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

	/*
	 * Containment: if a *user* task faulted — at EL0 (a bad access in its own
	 * code) or in the kernel mid-syscall (a bad user pointer the kernel
	 * dereferenced on its behalf) — terminate just that process and reschedule,
	 * so a buggy application can't take down the whole OS.  Mirrors do_exit
	 * (endTask + sched_yield, which never returns here).
	 *
	 * A fault with no current user task (md_usp == 0: a kernel thread / early
	 * boot) is a genuine kernel bug with no safe recovery — keep failing fast by
	 * parking the CPU after the dump.  Only instruction/data aborts are treated
	 * as recoverable; other synchronous causes (e.g. a kernel BRK) still park.
	 */
	{
		u_int64_t ec = (esr >> 26) & 0x3f;
		int is_abort = (ec == 0x20 || ec == 0x21 || /* instruction abort (lower / same EL) */
		                ec == 0x24 || ec == 0x25);  /* data abort (lower / same EL) */

		if (is_abort && _current != 0 && _current->md.md_usp != 0)
		{
			kprintf("  -> terminating offending user task pid=%d (%s); OS continues\n", _current->id,
			        _current->name);
			endTask(_current->id);
			sched_yield();
			/* not reached — sched_yield switches to another task */
		}
	}

	for (;;)
		__asm__ volatile("wfi");
}
