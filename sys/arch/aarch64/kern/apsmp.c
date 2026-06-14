/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 SMP application-processor bring-up (smp-plan M1).
 *
 * Starts the secondary cores the DTB enumerated (cpu_enum) via PSCI CPU_ON, each
 * landing in secondary_entry (apentry.S) -> c_ap_boot_arm() here.  M1 proves
 * liveness only (the AP enables its MMU, installs its per-CPU block, and bumps a
 * heartbeat the BSP observes); the per-CPU scheduler arrives in M2/M3.
 */

#include <sys/types.h>
#include <aarch64/pcpu.h>    /* struct pcpu, g_pcpu, curcpu, aarch64_pcpu_install */
#include <ubixos/cpu_enum.h> /* g_cpu_desc, smp_cpu_count */
#include <lib/kprintf.h>
#include "../bringup.h" /* aarch64_mmu_enable_secondary, aarch64_vbar_init, c_ap_boot_arm */

#define AP_STACK_SIZE 16384 /* must match apentry.S */

/* Per-CPU kernel stacks for the APs (slot 0 = BSP's, unused — the BSP runs on
 * the linker's _stack).  BSS, so zeroed; apentry.S carves sp from this symbol. */
char g_ap_stack[MAXCPU][AP_STACK_SIZE] __attribute__((aligned(16)));

/* secondary_entry lives in apentry.S; PSCI CPU_ON needs its physical address,
 * which equals its link address (the kernel is identity-mapped). */
extern void secondary_entry(void);

#define PSCI_CPU_ON 0xC4000003u /* SMC64/HVC64 CPU_ON function id */

/** Read MPIDR_EL1 affinity (low 24 bits) of the calling CPU. */
static u_int64_t ap_read_mpidr(void)
{
	u_int64_t v;
	__asm__ volatile("mrs %0, mpidr_el1" : "=r"(v));
	return (v & 0xFFFFFFu);
}

/**
 * PSCI call over HVC (QEMU `virt` conduit when there is no secure EL3).
 * @return the PSCI return code (0 = SUCCESS).
 */
static int64_t psci_call(u_int64_t fn, u_int64_t a0, u_int64_t a1, u_int64_t a2)
{
	register u_int64_t x0 __asm__("x0") = fn;
	register u_int64_t x1 __asm__("x1") = a0;
	register u_int64_t x2 __asm__("x2") = a1;
	register u_int64_t x3 __asm__("x3") = a2;
	__asm__ __volatile__(
	    "hvc #0"
	    : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
	    :
	    : "memory", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17");
	return (int64_t)x0;
}

/**
 * C entry for an application processor (reached from apentry.S with the MMU off).
 * Enable the MMU on the shared kernel page tables, install this CPU's vectors +
 * per-CPU block, then prove liveness by bumping our heartbeat before parking.
 */
void c_ap_boot_arm(u_int32_t id)
{
	aarch64_mmu_enable_secondary();            /* MMU + caches on (BSP's l1_table) */
	aarch64_vbar_init();                       /* this CPU's EL1 vectors (banked VBAR_EL1) */
	aarch64_pcpu_install(id, ap_read_mpidr()); /* TPIDR_EL1 -> g_pcpu[id]; _current usable */

	/*
	 * M1 liveness: bump our heartbeat a bounded number of times so the BSP sees
	 * us executing kernel C in parallel, then park in a low-power wait.  M3
	 * replaces this with the per-CPU scheduler (LAPIC/CNTV-timer driven sched()).
	 */
	for (int i = 0; i < 20000000; i++)
	{
		curcpu()->heartbeat++;
		__asm__ __volatile__("dmb ish");
	}
	for (;;)
		__asm__ __volatile__("wfi");
}

/**
 * BSP: start every enumerated secondary core via PSCI CPU_ON and confirm each
 * one comes alive (its heartbeat advances).  Discovery is done by cpu_enum
 * (DTB /cpus); here we actually launch.
 */
void aarch64_smp_start_aps(void)
{
	u_int64_t entry = (u_int64_t)(uintptr_t)&secondary_entry; /* phys == link addr */
	unsigned n = smp_cpu_count();
	unsigned online = 0;

	for (unsigned id = 1; id < n && id < MAXCPU; id++)
	{
		u_int64_t mpidr = g_cpu_desc[id].hwid;
		u_int32_t hb0 = g_pcpu[id].heartbeat;
		int64_t rc = psci_call(PSCI_CPU_ON, mpidr, entry, id);

		if (rc != 0)
		{
			kprintf("smp: PSCI CPU_ON cpu%u (mpidr=0x%lx) failed: %ld\n", id, mpidr, (long)rc);
			continue;
		}

		/* Wait (bounded) for the AP to bump its heartbeat. */
		volatile u_int32_t spins = 0;
		while (g_pcpu[id].heartbeat == hb0 && spins < 200000000u)
			spins++;

		if (g_pcpu[id].heartbeat != hb0)
		{
			online++;
			kprintf("smp: cpu%u online (mpidr=0x%lx, heartbeat advancing)\n", id, mpidr);
		}
		else
		{
			kprintf("smp: cpu%u no response after CPU_ON (mpidr=0x%lx)\n", id, mpidr);
		}
	}

	kprintf("smp: %u application processor(s) online (+ BSP) = %u core(s)\n", online, online + 1);
}
