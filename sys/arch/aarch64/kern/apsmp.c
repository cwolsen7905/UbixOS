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
#include <aarch64/pcpu.h>          /* struct pcpu, g_pcpu, curcpu, aarch64_pcpu_install */
#include <ubixos/cpu_enum.h>       /* g_cpu_desc, smp_cpu_count */
#include <ubixos/sched.h>          /* kTask_t, sched_yield, set_current, QOS_IDLE */
#include <ubixos/sched_internal.h> /* schedNewTask, sched_set_priority */
#include <lib/kprintf.h>
#include <lib/kconsole.h> /* kconsole_enable_mp — engage console locking once SMP is live */
#include <string.h>
#include "../bringup.h" /* aarch64_mmu_enable_secondary, aarch64_vbar_init, c_ap_boot_arm, aarch64_kernel_l1 */

#define AP_STACK_SIZE 16384 /* must match apentry.S */

/*
 * smp-plan: release the APs into the SCHEDULER (M3).  Cooperative parallel SMP —
 * the APs pull READY tasks off the shared run queue in parallel with the BSP,
 * mirroring the model proven on x86_64.  Phase-0 hardening landed the two fixes
 * that real-display testing needed: the virtio-blk request path is now serialised
 * (sys/arch/aarch64/dev/virtio_blk.c — a concurrent disk read no longer returns a
 * stale sector, the cause of the failed-first-login), and a GICv2 SGI reschedule
 * IPI (arch_smp_reschedule below) wakes an idle AP immediately on enqueue instead
 * of its 10 ms timer poll (the cause of the laggy/dead mouse).  The APs stay
 * *cooperative*: their timer wakes wfi but never preempts a running EL0 task —
 * only the BSP time-slices (see aarch64_exception) — which avoids the
 * async-interrupt-preemption-on-a-secondary corruption deferred on x86_64.
 *
 * ENABLED (1) on the v2 per-CPU-run-queue scheduler.  The original fragility was the
 * single global run queue: two CPUs could dispatch the same task onto one kernel
 * stack (double-dispatch -> torn context), which froze the desktop.  With
 * CONFIG_SCHED_PERCPU each CPU schedules from its own g_rq[cpuid] and a task lives in
 * exactly one queue, so that class is structurally impossible — validated 24/24 clean
 * on x86_64 -smp 2 (docs/design/smp-scheduler-v2-plan.md, Phase 2c-lite).  The Phase-0
 * fixes here (virtio-blk lock, reschedule SGI, BSP-only preempt gate) carry over.  The
 * AP marks cpu_rq(cpuid)->online so sched_select_cpu() homes work to it.  (ubixfs is
 * still not SMP-safe — intermittent "not loadable" is a separate workstream.)
 */
#define AARCH64_SMP_ENABLE_APS 1

/*
 * smp-plan M3 AP-release gate.  The APs come up (M1/M2) before the scheduler and
 * the per-CPU idle tasks exist, so they idle on their own timer (heartbeat) until
 * the BSP has sched_init'd, scheduled init, created each AP's idle task, and set
 * this flag — then each AP adopts its idle and joins the shared run queue.
 */
volatile int g_arm_ap_go = 0;

/* Per-CPU kernel stacks for the APs (slot 0 = BSP's, unused — the BSP runs on
 * the linker's _stack).  BSS, so zeroed; apentry.S carves sp from this symbol. */
char g_ap_stack[MAXCPU][AP_STACK_SIZE] __attribute__((aligned(16)));

/* secondary_entry lives in apentry.S (.text.boot, linked LOW); PSCI CPU_ON needs
 * its physical address, which equals its low link address.  Take it through an
 * absolute-relocated pointer (R_AARCH64_ABS64 in .data) rather than referencing
 * &secondary_entry directly: in the higher-half kernel this C runs at a high VA,
 * and a direct adrp to the low symbol overflows the ±4 GB PC-relative range. */
extern void secondary_entry(void);
/* volatile, not const: const lets the optimizer fold the read back into a direct
 * &secondary_entry adrp (re-introducing the out-of-range relocation); volatile
 * forces the load from the absolute-relocated .data slot. */
static void *volatile g_secondary_entry_phys = (void *)&secondary_entry;

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

#define PSCI_SYSTEM_RESET 0x84000009u /* PSCI SYSTEM_RESET (SMC32/HVC32 function id) */

/**
 * Reset the machine via PSCI SYSTEM_RESET.  On QEMU `virt` this restarts the
 * machine, so the firmware/bootloader runs again — U-Boot re-loads the kernel
 * from the FAT boot partition and boots it (the reboot half of the self-rebuild
 * loop: build a new kernel, install it to /boot, reset).
 *
 * The PSCI *conduit* depends on the boot environment: QEMU's built-in PSCI (the
 * direct `-kernel` boot) answers HVC, while a firmware/secure-monitor — U-Boot
 * as `-bios`, or real hardware with EL3 — answers SMC.  Try HVC first, then SMC;
 * SYSTEM_RESET does not return on success, so reaching the SMC (or the WFI spin)
 * means the prior conduit was not the active one.
 */
void aarch64_system_reset(void)
{
	register u_int64_t x0 __asm__("x0") = PSCI_SYSTEM_RESET;
	register u_int64_t x1 __asm__("x1") = 0;
	register u_int64_t x2 __asm__("x2") = 0;
	register u_int64_t x3 __asm__("x3") = 0;

	__asm__ __volatile__("hvc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");

	x0 = PSCI_SYSTEM_RESET;
	x1 = 0;
	x2 = 0;
	x3 = 0;
	__asm__ __volatile__("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");

	for (;;)
		__asm__ volatile("wfi");
}

/**
 * C entry for an application processor (reached from apentry.S with the MMU off).
 * Enable the MMU on the shared kernel page tables, install this CPU's vectors +
 * per-CPU block, then prove liveness by bumping our heartbeat before parking.
 */
void c_ap_boot_arm(u_int32_t id)
{
	/* The MMU + caches were enabled by the secondary_entry stub (apentry.S) before
	 * the branch to the high half, so we are already running translated here. */
	aarch64_vbar_init();                       /* this CPU's EL1 vectors (banked VBAR_EL1) */
	aarch64_pcpu_install(id, ap_read_mpidr()); /* TPIDR_EL1 -> g_pcpu[id]; _current usable */

	/* Immediate liveness so the BSP's CPU_ON wait returns promptly (the first
	 * timer tick is ~10 ms out). */
	curcpu()->heartbeat++;

	/*
	 * smp-plan M2: bring up this CPU's own (banked) GIC interface + CNTV timer,
	 * then unmask IRQs.  From here the AP's own 100 Hz timer PPI drives its
	 * heartbeat (timer_tick bumps it for cpuid != 0), proving per-CPU interrupt
	 * delivery without touching the BSP's shared scheduler clock.  It otherwise
	 * idles in wfi (woken by each tick).  M3 replaces the wfi loop with the
	 * per-CPU scheduler.
	 */
	aarch64_intc_secondary_init();
	timer_init();
	__asm__ __volatile__("msr daifclr, #2"); /* unmask IRQ (keep FIQ/SError masked) */

	/*
	 * smp-plan M3: wait for the BSP to finish booting and create our per-CPU idle
	 * task (g_arm_ap_go), taking our own timer ticks meanwhile.  Then adopt the
	 * idle as _current and enter the cooperative scheduler: sched_yield() pulls a
	 * READY task from the shared run queue (run-queue-locked, so safe alongside
	 * the BSP); when nothing is runnable we halt until our next timer tick.  This
	 * is the per-CPU-idle path (dispatcher: cpu_idle = curcpu()->idle).
	 */
	while (!g_arm_ap_go)
		__asm__ __volatile__("wfi");

	kTask_t *idle = curcpu()->idle;
	idle->state = RUNNING;
	set_current(idle);

	/* Advertise this CPU's run queue as a placement target (v2 load distribution):
	 * sched_select_cpu() will now home freshly-runnable tasks here. */
	cpu_rq(curcpu()->cpuid)->online = 1;

	for (;;)
	{
		sched_yield();
		__asm__ __volatile__("wfi");
	}
}

/**
 * BSP: create one non-enqueued per-CPU idle task per AP and release the APs into
 * the scheduler (smp-plan M3).  Called once the scheduler is up and init is
 * scheduled — the APs then pull READY tasks from the shared run queue.
 */
void aarch64_smp_release_aps(void)
{
	unsigned n = smp_cpu_count();

#if !AARCH64_SMP_ENABLE_APS
	/* Default: leave the APs parked at the M2 gate (up + taking their own timer
	 * ticks, but not scheduling).  True-SMP hardening (M4) is incomplete. */
	(void)n;
	return;
#endif

	for (unsigned id = 1; id < n && id < MAXCPU; id++)
	{
		kTask_t *idle = schedNewTask();
		if (idle == 0)
			continue;
		/* md_ttbr0 is a physmap VA (Convention B): pmap_switch converts back to the
		 * physical root.  aarch64_kernel_l1() is the kernel L1's pointer; lift its
		 * physical into the physmap. */
		idle->md.md_ttbr0 = AARCH64_VIRT_OF((uintptr_t)aarch64_kernel_l1());
		idle->md.md_entry = 0;
		idle->md.md_usp = 0;
		strncpy(idle->name, "idle/ap", sizeof(idle->name) - 1);
		sched_set_priority(idle, QOS_IDLE); /* lowest band; never enqueued (per-CPU fallback) */
		g_pcpu[id].idle = idle;             /* the AP adopts this as its _current */
	}

	kconsole_enable_mp(); /* SMP about to be live + MMU long up — safe to lock the console */
	__asm__ __volatile__("dmb ish");
	g_arm_ap_go = 1;
	kprintf("smp: released %u application processor(s) into the scheduler\n", (n > 1) ? n - 1 : 0);
}

/**
 * arch_smp_reschedule (strong override of the weak no-op in sched_core.c): poke
 * every other CPU's reschedule IPI so an idle one wakes from wfi, re-runs
 * sched_yield(), and picks up work just enqueued under schedulerSpinLock — instead
 * of waiting up to 10 ms for its own next timer tick.  This is what makes input/MPI
 * wakeups to a task parked on a secondary prompt.  No-op until the APs are released
 * (single CPU before that; and the GIC is fully up by release time).  Sends to the
 * BSP too (cpu 0), so an AP enqueuing work wakes an idle BSP.  Runs under
 * schedulerSpinLock — a couple of GICD_SGIR writes, cheap.
 */
void arch_smp_reschedule(void)
{
	unsigned self, c, n;

	if (!g_arm_ap_go)
		return; /* APs not scheduling yet — only the BSP is running */

	self = curcpu()->cpuid;
	n = smp_cpu_count();
	if (n > MAXCPU)
		n = MAXCPU;
	for (c = 0; c < n; c++)
	{
		if (c == self)
			continue;
		aarch64_intc_send_resched(c);
	}
}

/**
 * Targeted reschedule (arch_smp_reschedule_cpu override): wake exactly @cpu via its
 * GICv2 SGI so it leaves wfi and picks up work just enqueued on its per-CPU run queue.
 * One SGI, no broadcast — replaces the storm that interrupted the BSP on every
 * secondary enqueue.  No-op for our own CPU, an out-of-range CPU, or before the APs
 * are released.  Runs under schedulerSpinLock.
 */
void arch_smp_reschedule_cpu(unsigned cpu)
{
	if (!g_arm_ap_go || cpu >= smp_cpu_count() || cpu == curcpu()->cpuid)
		return;
	aarch64_intc_send_resched(cpu);
}

/**
 * BSP: start every enumerated secondary core via PSCI CPU_ON and confirm each
 * one comes alive (its heartbeat advances).  Discovery is done by cpu_enum
 * (DTB /cpus); here we actually launch.
 */
void aarch64_smp_start_aps(void)
{
	u_int64_t entry = (u_int64_t)(uintptr_t)g_secondary_entry_phys; /* phys == low link addr */
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
