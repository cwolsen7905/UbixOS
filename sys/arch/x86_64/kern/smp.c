/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 SMP application-processor startup (smp-plan, x86_64 AP boot).
 *
 * Brings the secondary CPUs out of reset via the standard INIT-SIPI-SIPI
 * sequence.  IPIs are sent through the **x2APIC** (MSR-based) rather than the
 * legacy xAPIC MMIO window, so no LAPIC page has to be mapped (the MMIO base is
 * above the low-1 GB physmap).  Each AP starts in 16-bit real mode at the copied
 * trampoline (boot/ap_trampoline.S), walks to 64-bit long mode on the shared
 * kernel PML4, and lands in x86_64_ap_entry().
 *
 * This is the bring-up milestone: an AP executes 64-bit kernel code and parks in
 * a per-CPU idle (bumping its pcpu heartbeat so the BSP can confirm it is alive).
 * Real scheduler participation (run-queue locking) is a later, interlocked step.
 */

#include "../x86_64.h"
#include <x86_64/pcpu.h>
#include <ubixos/cpu_enum.h>
#include <ubixos/vitals.h>
#include <lib/kmalloc.h>
#include <string.h>

#define MSR_APIC_BASE 0x1B     /* IA32_APIC_BASE: bit11 = global enable, bit10 = x2APIC */
#define MSR_X2APIC_SVR 0x80F   /* Spurious Interrupt Vector Register */
#define MSR_X2APIC_LINT0 0x835 /* LVT LINT0 */
#define MSR_X2APIC_LINT1 0x836 /* LVT LINT1 */
#define MSR_X2APIC_ICR 0x830   /* x2APIC Interrupt Command Register (single 64-bit write) */

#define SVR_ENABLE 0x100u /* SVR bit 8: software-enable the LAPIC */
#define LVT_EXTINT 0x700u /* delivery mode = ExtINT (pass the 8259 through) */
#define LVT_NMI 0x400u    /* delivery mode = NMI */

#define APIC_BASE_ENABLE (1u << 11)
#define APIC_BASE_X2APIC (1u << 10)

/* ICR command fields (low 32 bits). */
#define ICR_INIT 0x00000500u    /* delivery mode = INIT */
#define ICR_STARTUP 0x00000600u /* delivery mode = Startup (SIPI) */
#define ICR_ASSERT 0x00004000u  /* level = assert */

#define AP_TRAMP_PHYS 0x8000  /* low page the trampoline runs at (SIPI vector 0x08) */
#define AP_ARGS_PHYS 0x8F00   /* arg block within that page (matches ap_trampoline.S) */
#define AP_SIPI_VECTOR 0x08   /* AP_TRAMP_PHYS >> 12 */
#define AP_KSTACK_SIZE 0x8000 /* 32 KB per-AP kernel stack */

extern u8 ap_trampoline_start[];
extern u8 ap_trampoline_end[];

static u64 g_kernel_cr3; /* the kernel PML4 the APs run on (captured at init) */

static u64 rdmsr64(u32 msr)
{
	u32 lo, hi;
	__asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return ((u64)hi << 32) | lo;
}

static void wrmsr64(u32 msr, u64 val)
{
	__asm__ __volatile__("wrmsr" : : "c"(msr), "a"((u32)val), "d"((u32)(val >> 32)));
}

/** @return non-zero if the CPU advertises x2APIC (CPUID.1:ECX bit 21). */
static int x2apic_supported(void)
{
	u32 a = 1, b = 0, c = 0, d = 0;
	__asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
	return (c & (1u << 21)) != 0;
}

/* Busy-loop delays.  These must NOT depend on the PIT/sysTicks: enabling the
 * LAPIC re-gates the CPU's interrupt input through LINT0, so the 8259 timer is
 * briefly not delivering until LINT0=ExtINT is set, and the AP-start sequence
 * runs across that window.  Iteration counts are rough (QEMU is lenient about
 * INIT-SIPI timing). */
static void delay_long(void) /* ~INIT->SIPI gap (>=10 ms equivalent) */
{
	for (volatile u64 i = 0; i < 20000000ULL; i++)
		__asm__ __volatile__("pause");
}

static void delay_short(void) /* ~SIPI->SIPI gap */
{
	for (volatile int i = 0; i < 200000; i++)
		__asm__ __volatile__("pause");
}

/** Send an x2APIC IPI: @cmd (delivery mode + vector + flags) to @apicid. */
static void send_ipi(u32 apicid, u32 cmd)
{
	wrmsr64(MSR_X2APIC_ICR, ((u64)apicid << 32) | cmd);
}

/**
 * AP 64-bit C entry (reached from ap_trampoline.S once in long mode on the kernel
 * PML4).  Install this CPU's per-CPU GS, mark it alive, and park.  Does not
 * return.  Real scheduler entry is a later step; for now the AP idles halted so it
 * costs 0% and cannot race the (unlocked) run queue.
 */
void x86_64_ap_entry(u32 cpuid)
{
	x86_64_pcpu_install(cpuid);
	curcpu()->heartbeat = 0xA5 + cpuid; /* liveness marker the BSP polls */
	__asm__ __volatile__("cli");
	for (;;)
		__asm__ __volatile__("hlt");
}

/**
 * Start the application processors.  Enable x2APIC on the BSP, stage the AP
 * trampoline in low memory, and INIT-SIPI-SIPI each non-boot enumerated CPU,
 * confirming liveness via its pcpu heartbeat.
 *
 * @return the number of APs that came online.
 */
int x86_64_smp_init(void)
{
	unsigned i;
	int next_cpuid = 1;
	int online = 0;
	u64 ab;

	if (g_cpu_desc_count <= 1)
	{
		kprintf("smp: single CPU; no APs to start\n");
		return 0;
	}
	if (!x2apic_supported())
	{
		kprintf("smp: x2APIC not supported; APs not started (BSP only)\n");
		return 0;
	}

	/* Capture the kernel PML4 (current CR3 at early boot) — the APs run on it. */
	__asm__ __volatile__("mov %%cr3, %0" : "=r"(g_kernel_cr3));

	/* Enable x2APIC on the BSP, then software-enable the LAPIC (SVR) and route
	 * LINT0 = ExtINT so the legacy 8259 PIT keeps reaching the CPU (enabling the
	 * LAPIC otherwise gates off the external-interrupt input → the PIT, hence
	 * sysTicks + scheduler preemption, would stop dead). */
	ab = rdmsr64(MSR_APIC_BASE);
	wrmsr64(MSR_APIC_BASE, ab | APIC_BASE_ENABLE | APIC_BASE_X2APIC);
	wrmsr64(MSR_X2APIC_SVR, SVR_ENABLE | 0xFF);
	wrmsr64(MSR_X2APIC_LINT0, LVT_EXTINT);
	wrmsr64(MSR_X2APIC_LINT1, LVT_NMI);

	/* Stage the trampoline at the SIPI page. */
	memcpy(P2V(AP_TRAMP_PHYS), ap_trampoline_start, (size_t)(ap_trampoline_end - ap_trampoline_start));

	for (i = 0; i < g_cpu_desc_count; i++)
	{
		u32 apicid = (u32)g_cpu_desc[i].hwid;
		u8 *args;
		void *stack;
		int cpuid;

		if (g_cpu_desc[i].is_boot || !g_cpu_desc[i].enabled)
			continue;
		if (next_cpuid >= MAXCPU)
			break;
		cpuid = next_cpuid++;

		stack = kmalloc(AP_KSTACK_SIZE);
		if (stack == 0)
		{
			kprintf("smp: out of memory for cpu%d stack\n", cpuid);
			continue;
		}

		/* Fill the trampoline arg block (physical low memory, reached via P2V). */
		args = (u8 *)P2V(AP_ARGS_PHYS);
		*(volatile u32 *)(args + 0x00) = (u32)g_kernel_cr3;                      /* ARG_CR3 */
		*(volatile u64 *)(args + 0x08) = (u64)(uintptr_t)stack + AP_KSTACK_SIZE; /* ARG_STACK */
		*(volatile u32 *)(args + 0x10) = (u32)cpuid;                             /* ARG_CPUID */
		*(volatile u64 *)(args + 0x18) = (u64)(uintptr_t)&x86_64_ap_entry;       /* ARG_ENTRY */

		g_pcpu[cpuid].apicid = apicid;
		g_pcpu[cpuid].heartbeat = 0;
		__asm__ __volatile__("mfence" ::: "memory");

		/* INIT, then two SIPIs (Intel MP startup protocol). */
		send_ipi(apicid, ICR_INIT | ICR_ASSERT);
		delay_long(); /* >=10 ms */
		send_ipi(apicid, ICR_STARTUP | AP_SIPI_VECTOR);
		delay_short();
		send_ipi(apicid, ICR_STARTUP | AP_SIPI_VECTOR);

		/* Wait for the AP to set its heartbeat (busy loop — independent of the PIT). */
		{
			volatile u64 spin = 0;
			while (g_pcpu[cpuid].heartbeat == 0 && spin < 100000000ULL)
			{
				spin++;
				__asm__ __volatile__("pause");
			}
		}

		if (g_pcpu[cpuid].heartbeat != 0)
		{
			kprintf(
			    "smp: cpu%d (apic %u) online (heartbeat 0x%X)\n", cpuid, apicid, g_pcpu[cpuid].heartbeat);
			online++;
		}
		else
		{
			kprintf("smp: cpu%d (apic %u) did NOT start\n", cpuid, apicid);
		}
	}

	kprintf("smp: %d AP(s) online (+ BSP)\n", online);
	return online;
}
