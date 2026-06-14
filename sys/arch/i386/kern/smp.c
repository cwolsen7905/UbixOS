/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <i386/smp.h>
#include <i386/pcpu.h>
#include <ubixos/cpu_enum.h> /* acpi_enum_cpus result + cpu_enum_dump (Phase 1) */
#include <sys/gdt.h>
#include <sys/tss.h> /* struct tssStruct — per-CPU TSS (Phase 3) */
#include <sys/idt.h> /* idt_load() — AP loads the shared IDT (Phase 3) */
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <vmm/paging.h>
#include <string.h>
#include <sys/io.h>

static struct spinLock cpuInfoLock = SPIN_LOCK_INITIALIZER;
static u_int32_t cpus = 0;
struct cpuinfo_t cpuinfo[8];

/* Count of application processors that have reached C code and checked in.
 * Written by APs (paging off) with a locked add, polled by the BSP. */
volatile u_int32_t ap_online = 0;

/* The BSP's CR3 (kernel page directory, physical), captured before the APs
 * start so each AP can load it and run in the kernel's virtual address space. */
volatile u_int32_t g_kernel_cr3 = 0;

/* Physical address the AP trampoline is copied to and started at.  It MUST be
 * 0x0: the trampoline's 32-bit jump uses flat, base-0 offsets, so the code only
 * lands correctly when its physical address equals those offsets.  This clobbers
 * the real-mode IVT, which V86 BIOS calls (VESA) still need, so apicMagic saves
 * and restores the overwritten bytes around AP startup. */
#define AP_TRAMPOLINE_PHYS 0x0

static inline unsigned int apicRead(unsigned int address)
{
	return *(volatile unsigned int *)(0xFEE00000 + address);
}

static inline void apicWrite(unsigned int address, unsigned int data)
{
	*(volatile unsigned int *)(0xFEE00000 + address) = data;
}

/**
 * Signal end-of-interrupt to the running CPU's Local APIC.  Every LAPIC-delivered
 * interrupt handler (timer, IPI) must call this before returning or the LAPIC
 * will not deliver further interrupts of equal/lower priority.
 */
void lapic_eoi(void)
{
	apicWrite(LAPIC_EOI, 0);
}

/**
 * Send a fixed-delivery inter-processor interrupt @vector to the CPU with the
 * given Local APIC id, then wait for the LAPIC to report delivery.
 *
 * Writes the high ICR dword (destination) first; writing the low dword latches
 * and sends the IPI.  Bit 14 (assert) is set per the usual fixed-IPI encoding;
 * the destination shorthand is 00 (use the explicit apic id in the high dword).
 */
void lapic_send_ipi(u_int8_t apic_id, u_int8_t vector)
{
	apicWrite(LAPIC_ICR_HIGH, (u_int32_t)apic_id << 24);
	apicWrite(LAPIC_ICR_LOW, 0x00004000u | (u_int32_t)vector);
	/* Delivery-status bit 12 stays set until the IPI is accepted. */
	while (apicRead(LAPIC_ICR_LOW) & 0x1000u)
		__asm__ __volatile__("pause");
}

/**
 * Arm the calling CPU's Local APIC timer in periodic mode at @initial_count
 * (divide-by-16), delivering LAPIC_TIMER_VECTOR each period.  Each CPU programs
 * its own LAPIC timer; this is how an AP gets scheduler ticks once it runs (the
 * BSP is still driven by the legacy PIT).  @initial_count == 0 disables it.
 */
void lapic_timer_init(u_int32_t initial_count)
{
	apicWrite(LAPIC_TIMER_DIV, 0x3); /* divide bus clock by 16 */
	if (initial_count == 0)
	{
		apicWrite(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
		return;
	}
	apicWrite(LAPIC_LVT_TIMER, LAPIC_TIMER_VECTOR | LAPIC_TIMER_PERIODIC);
	apicWrite(LAPIC_TIMER_INIT, initial_count);
}

/* ------------------------------------------------------------------ */
/* Per-CPU state (Phase 2 scaffolding)                                 */
/* ------------------------------------------------------------------ */

struct pcpu g_pcpu[MAXCPU];

/* Set to 1 once per-CPU scheduling is live.  Until then smp_processor_id()
 * shortcuts to the BSP (cpu 0) so no MI caller has to touch the LAPIC. */
static volatile int g_smp_active = 0;

/* Set by the BSP once it has observed each AP's heartbeat advance (the boot
 * liveness check).  Until then an AP bumps its heartbeat in a short spin so the
 * BSP can see it is alive; afterwards the AP halts instead of spinning.  An idle
 * AP that busy-spins (pause) does NOT deschedule the vCPU under QEMU/TCG — it
 * pegs a host core and steals emulation throughput from the BSP, making the
 * whole VM crawl.  There is no per-CPU scheduler yet, so the AP has no work:
 * halting until the real per-CPU idle thread lands is correct. */
static volatile int g_ap_park = 0;

/* Set by a deferred kernel thread once the BSP has finished booting (desktop up,
 * system idle).  Releases each parked AP into the per-CPU scheduler (Phase 3 AP
 * entry) so it does not race the not-yet-SMP-safe boot path. */
volatile int g_ap_go = 0;

u_int32_t smp_processor_id(void)
{
	u_int8_t apicid;
	u_int32_t i;

	if (!g_smp_active)
		return (0); /* single-CPU / pre-SMP: always the boot processor */

	apicid = apicRead(0x20) >> 24;
	for (i = 0; i < MAXCPU; i++)
		if (g_pcpu[i].online && g_pcpu[i].apicid == apicid)
			return (i);
	return (0);
}

struct pcpu *curcpu(void)
{
	return (&g_pcpu[smp_processor_id()]);
}

/* Record a CPU's identity in its per-CPU slot.  Safe to call from an AP running
 * paging-off: g_pcpu lives in low (<4 MB, identity-mapped) kernel memory and the
 * LAPIC id register is reachable at its physical MMIO address. */
static void pcpu_register(u_int32_t id, u_int8_t apicid)
{
	if (id >= MAXCPU)
		return;
	g_pcpu[id].cpuid = id;
	g_pcpu[id].apicid = apicid;
	/* Do NOT touch current/idle here — on the BSP, current already points at
	 * the running thread by the time smpInit runs.  They are zero from BSS for
	 * the APs, which have run nothing yet. */
	g_pcpu[id].online = 1;
}

/**
 * Point this CPU's %gs at its per-CPU area.
 *
 * Patches GDT index GDT_PCPU_INDEX so its base is &g_pcpu[id], then loads
 * SEL_PCPU into %gs.  Thereafter %gs:offsetof(struct pcpu, field) reads this
 * CPU's slot — e.g. %gs:8 is the running task.  On a uniprocessor only id 0 is
 * ever used; each AP calls this with its own id against its own GDT copy.
 *
 * The descriptor base must be written before %gs is loaded: loading a segment
 * register re-reads the descriptor from the GDT in memory, so no relgdt is
 * needed.  Loading %gs is purely additive until kernel entry paths reload it
 * and _current is switched to %gs:8.
 */
void pcpu_install_gs(u_int32_t id)
{
	u_int32_t base = (u_int32_t)&g_pcpu[id];
	u_int16_t sel = SEL_PCPU;

	ubixGDT[GDT_PCPU_INDEX].descriptor.baseLow = (base & 0xFFFF);
	ubixGDT[GDT_PCPU_INDEX].descriptor.baseMed = ((base >> 16) & 0xFF);
	ubixGDT[GDT_PCPU_INDEX].descriptor.baseHigh = ((base >> 24) & 0xFF);

	__asm__ __volatile__("movw %0, %%gs" : : "rm"(sel) : "memory");
}

struct gdt_descr
{
	u_int16_t limit;
	u_int32_t *base __attribute__((packed));
};

/* Per-CPU GDT copies + per-CPU TSS (smp-plan Phase 3).
 *
 * A single shared GDT/TSS cannot serve multiple CPUs: the PCPU descriptor base
 * (what %gs resolves to) and the TSS ring-0 stack (esp0) are per-CPU.  Each CPU
 * loads its OWN copy of the 12-entry GDT — same selectors, so the hardcoded
 * SEL_PCPU / SEL_TSS (0x20) keep working — with its PCPU (idx 11) and TSS (idx 4)
 * descriptors patched to point at its own g_pcpu[] slot and its own TSS.
 */
union descriptorTableUnion g_cpu_gdt[MAXCPU][12] __attribute__((aligned(8)));
struct tssStruct g_cpu_tss[MAXCPU];

/**
 * Build CPU @id's private GDT + TSS and load them (lgdt + ltr + %gs).
 *
 * Copies the shared ubixGDT, repoints index 11 (PCPU) at &g_pcpu[id] and index 4
 * (TSS, selector 0x20) at &g_cpu_tss[id], clears the TSS descriptor BUSY bit (a
 * copied-from-an-already-loaded descriptor is marked busy; ltr #GPs on a busy
 * TSS), seeds the TSS ring-0 state from the boot TSS at 0x4200 so an early
 * ring3->ring0 entry lands on a valid stack, then loads all three.  Same
 * selectors as before, so no segment register needs reloading.
 */
void pcpu_gdt_tss_load(u_int32_t id)
{
	union descriptorTableUnion *gdt;
	u_int32_t pbase, tbase;
	struct gdt_descr gp;
	u_int16_t tss_sel = 0x20; /* GDT index 4 << 3 */
	u_int16_t pcpu_sel = SEL_PCPU;

	if (id >= MAXCPU)
		return;
	gdt = g_cpu_gdt[id];
	pbase = (u_int32_t)&g_pcpu[id];
	tbase = (u_int32_t)&g_cpu_tss[id];

	memcpy(gdt, ubixGDT, sizeof(union descriptorTableUnion) * 12);

	gdt[GDT_PCPU_INDEX].descriptor.baseLow = (u_int16_t)(pbase & 0xFFFF);
	gdt[GDT_PCPU_INDEX].descriptor.baseMed = (u_int8_t)((pbase >> 16) & 0xFF);
	gdt[GDT_PCPU_INDEX].descriptor.baseHigh = (u_int8_t)((pbase >> 24) & 0xFF);

	gdt[4].descriptor.baseLow = (u_int16_t)(tbase & 0xFFFF);
	gdt[4].descriptor.baseMed = (u_int8_t)((tbase >> 16) & 0xFF);
	gdt[4].descriptor.baseHigh = (u_int8_t)((tbase >> 24) & 0xFF);
	gdt[4].descriptor.access &= (u_int8_t)~0x02; /* BUSY -> available, else ltr #GPs */

	memcpy(&g_cpu_tss[id], (const void *)0x4200, sizeof(struct tssStruct));

	/*
	 * Cold-boot #TS / triple-fault fix.  pcpu_gdt_tss_load() is the very first
	 * thing kmain() does — BEFORE idt_init() populates the static 0x4200 TSS with
	 * ss0 = 0x10.  On a cold boot 0x4200 is still zeroed when we copy it above, so
	 * the live per-CPU TSS would inherit a null ss0.  The CPU reads SS0:ESP0 from
	 * this TSS on every ring3->ring0 entry; a null ss0 faults #TS on the first
	 * userland timer tick -> #DF -> triple fault -> reboot.  A warm reset hid the
	 * bug because guest RAM keeps the previous boot's 0x10 at 0x4200 across the
	 * reset.  Seed the ring-0 stack selector (and the I/O-map base) explicitly so
	 * the TSS is valid independent of 0x4200's init order.  esp0 is refreshed per
	 * switch by switch_to(); ss0 is not, so it must be correct here for good.
	 */
	g_cpu_tss[id].ss0 = 0x10;
	g_cpu_tss[id].io_map = 0x8000;

	gp.limit = (u_int16_t)(sizeof(union descriptorTableUnion) * 12 - 1);
	gp.base = (u_int32_t *)gdt;
	__asm__ __volatile__("lgdt %0" : : "m"(gp));
	__asm__ __volatile__("ltr %0" : : "rm"(tss_sel));
	__asm__ __volatile__("movw %0, %%gs" : : "rm"(pcpu_sel) : "memory");
}

static void GDT_fixer()
{
	struct gdt_descr gdt_descr;
	u_int32_t *gdt = (u_int32_t *)0x20000; // 128KB

	gdt[0] = 0;
	gdt[1] = 0;
	gdt[2] = 0x0000ffff; // seg 0x8  -- DPL 0 4GB code
	gdt[3] = 0x00cf9a00;
	gdt[4] = 0x0000ffff; // seg 0x10 -- DPL 0 4GB data
	gdt[5] = 0x00cf9200;
	gdt[6] = 0x0000ffff; // seg 0x1b -- DPL 3 4GB code
	gdt[7] = 0x00cffa00;
	gdt[8] = 0x0000ffff; // seg 0x23 -- DPL 3 4GB data
	gdt[9] = 0x00cff200;

	gdt_descr.limit = 32 * 4;
	gdt_descr.base = gdt;
	(void)gdt_descr; /* lgdt is commented out below; silence unused warning */

	/*
	 asm("lgdt %0;" : : "m" (gdt_descr));
	 __asm__ __volatile__ ("ljmp %0,$1f; 1:" :: "i" (0x08));
	 __asm__ __volatile__ ("movw %w0,%%ds" :: "r" (0x10));
	 __asm__ __volatile__ ("movw %w0,%%es" :: "r" (0x10));
	 __asm__ __volatile__ ("movw %w0,%%ss" :: "r" (0x10));
	 */
}

/*
 * c_ap_boot — C entry point for an application processor, reached from the
 * real-mode trampoline (ap-boot.S) after it switches to 32-bit protected mode.
 *
 * The AP runs with PAGING OFF, so it may touch only identity-addressable low
 * physical memory (the kernel is loaded below 4 MB) and I/O ports.  It must NOT
 * call kprintf() or the yielding spinLock() — there is no scheduler context on
 * this CPU yet.  Phase 1b therefore does the absolute minimum: record that this
 * core reached C with a locked increment, then park forever with interrupts
 * masked.  The BSP polls ap_online to report how many cores came up.
 */
void c_ap_boot(void)
{
	u_int32_t id = __sync_add_and_fetch(&ap_online, 1); /* 1, 2, 3, ... */

	/* Join the kernel's virtual address space: load the BSP's page directory
	 * (captured in g_kernel_cr3) and turn on paging.  The kernel identity-maps
	 * the low 4 MB — covering this code, our trampoline stack and the AP GDT —
	 * so execution continues seamlessly across the CR0.PG write, and the LAPIC
	 * MMIO the BSP mapped becomes reachable through the shared page tables. */
	__asm__ __volatile__("movl %0, %%cr3      \n"
	                     "movl %%cr0, %%eax   \n"
	                     "orl  $0x80000000, %%eax \n"
	                     "movl %%eax, %%cr0   \n"
	                     :
	                     : "r"(g_kernel_cr3)
	                     : "eax", "memory");

	pcpu_register(id, apicRead(0x20) >> 24);

	/* Liveness phase: the first code an AP runs continuously in the kernel
	 * address space, in parallel with the BSP — the seed of the real per-CPU idle
	 * thread.  Bump our own heartbeat (liveness the BSP can observe) until the BSP
	 * has confirmed us (g_ap_park), then fall through to halt.  We stay cli and
	 * touch only our own pcpu slot, so there is no cross-CPU contention.  We must
	 * NOT spin here forever: pause is only a spin-wait hint, it does not yield the
	 * vCPU under TCG, so an idle AP would peg a host core and starve the BSP that
	 * runs the actual system. */
	if (id < MAXCPU)
		while (!g_ap_park)
		{
			g_pcpu[id].heartbeat++;
			__asm__ __volatile__("pause");
		}

#if SMP_ENABLE_APS
	/* Wait (lightly) for the BSP to finish booting — tasks created, scheduler
	 * running, desktop up — before this AP joins the scheduler.  The release flag
	 * g_ap_go is set by a deferred kernel thread once the system is idle, so the
	 * AP does not race the (not-yet-SMP-safe) boot path. */
	while (!g_ap_go)
		__asm__ __volatile__("pause");

	/* smp-plan Phase 3 — AP scheduler entry.  Build this CPU's private GDT+TSS
	 * (so %gs / esp0 are per-CPU), load the shared IDT, activate g_smp_active so
	 * smp_processor_id() resolves the real LAPIC id, arm the periodic LAPIC timer
	 * (drives lapicTimerInt -> sched() on this CPU), then enable interrupts.
	 * _current is NULL here; the first LAPIC-timer sched() discards this boot
	 * context (prev==NULL) exactly as the BSP discards kmain, and runs a task —
	 * a ready task from the shared queue, or this CPU's g_pcpu[id].idle. */
	pcpu_gdt_tss_load(id);
	idt_load();
	g_smp_active = 1;
	lapic_timer_init(1000000); /* periodic ticks (rough rate; calibration TODO) */
	__asm__ __volatile__("sti");

	/* Idle until the LAPIC timer fires and sched() switches us to real work; when
	 * nothing is runnable we come back here and halt (woken by the next tick). */
	for (;;)
		__asm__ __volatile__("hlt");
#else
	/* SMP_ENABLE_APS == 0 (default): the AP has no work and the kernel still has
	 * uniprocessor assumptions (see smp.h).  Park in a low-power halt — cli;hlt,
	 * not a pause-spin, so an idle AP does not peg a host core under TCG. */
	(void)id;
	for (;;)
		__asm__ __volatile__("cli; hlt");
#endif
}

/*
 * smpInit (SMP phase 1b) — bring up the application processors far enough to
 * prove the bootstrap path works.  Map the Local APIC, register the BSP, then
 * INIT-SIPI the other cores.  Each AP lands in c_ap_boot(), records itself in
 * ap_online and parks in hlt; nothing runs on them yet (no per-CPU scheduler).
 * The BSP polls ap_online and reports the core count.  @return 0 always.
 */
int smpInit(void)
{
	/* The LAPIC lives at a fixed high MMIO address with no mapping yet; map it
	 * identity into the kernel space before any apicRead/apicWrite. */
	vmm_remap_io_page(LAPIC_PHYS, KERNEL_PAGE_DEFAULT, sysID);

	GDT_fixer();                          /* build the flat GDT at 0x20000 that the APs load */
	cpuInfo();                            /* BSP self-registers as cpuinfo[0] */
	pcpu_register(0, cpuinfo[0].apic_id); /* BSP per-CPU slot */
	pcpu_install_gs(0);                   /* BSP %gs -> g_pcpu[0] (unused yet) */

	kprintf("smp: BSP online cpu%u apic_id=%d ver=0x%x \"%s\"\n",
	        curcpu()->cpuid,
	        cpuinfo[0].apic_id,
	        cpuinfo[0].apic_ver,
	        cpuinfo[0].brand);

	/* smp-plan Phase 1: enumerate CPUs from the ACPI MADT into the MI cpu_enum
	 * table (authoritative count + APIC ids).  Discovery only — apicMagic() below
	 * still launches the APs the existing way; this does not change that yet. */
	acpi_enum_cpus(cpuinfo[0].apic_id);
	cpu_enum_dump();

	/* Capture the kernel page directory so each AP can adopt it and run in the
	 * kernel address space. */
	__asm__ __volatile__("movl %%cr3, %0" : "=r"(g_kernel_cr3));

	/* Start the application processors (apicMagic waits for them to check in). */
	apicMagic();

	kprintf("smp: %u application processor(s) online (+ BSP) = %u core(s)\n", ap_online, ap_online + 1);

	/* Report the per-CPU table the cores filled in (proves curcpu/pcpu work). */
	for (u_int32_t i = 0; i < MAXCPU; i++)
		if (g_pcpu[i].online)
			kprintf("smp:   pcpu[%u] apicid=%d\n", g_pcpu[i].cpuid, g_pcpu[i].apicid);

	/* Prove the APs are executing C in parallel: snapshot each core's heartbeat,
	 * wait, then show it advanced.  The BSP (cpu0) does not run the idle loop, so
	 * its heartbeat stays 0. */
	{
		u_int32_t hb[MAXCPU];
		u_int32_t i, d;
		for (i = 0; i < MAXCPU; i++)
			hb[i] = g_pcpu[i].heartbeat;
		for (d = 0; d < 20000000; d++)
			asm("nop");
		for (i = 0; i < MAXCPU; i++)
			if (g_pcpu[i].online)
				kprintf("smp:   cpu%u heartbeat %u -> %u (%s)\n",
				        i,
				        hb[i],
				        g_pcpu[i].heartbeat,
				        (i == 0)                         ? "BSP"
				        : (g_pcpu[i].heartbeat != hb[i]) ? "running"
				                                         : "STALLED");
	}

	/* Liveness confirmed: release the APs from their heartbeat spin so they halt
	 * instead of pegging a host core under TCG. */
	g_ap_park = 1;

	return (0);
}

void cpuidDetect()
{
	if (!(getEflags() & (1 << 21)))
	{
		setEflags(getEflags() | (1 << 21));
		if (!(getEflags() & (1 << 21)))
		{
			kpanic("CPU doesn't support CPUID, get a newer machine\n");
		}
	}
}

u_int8_t cpuInfo()
{
	u_int32_t data[4], i;

	/* CPUID is unconditionally present on every CPU UbixOS boots on (GRUB
	 * multiboot i386, Pentium and later), so we call it directly.  The legacy
	 * EFLAGS.ID toggle test that used to gate this was both unnecessary and
	 * unreliable. */

	spinLock(&cpuInfoLock);
	cpuinfo[cpus].ok = 1;
	cpuinfo[cpus].apic_id = apicRead(0x20) >> 24;
	cpuinfo[cpus].apic_ver = apicRead(0x30) & 0xFF;

	cpuid(0, data);
	*(u_int32_t *)&cpuinfo[cpus].ident[0] = data[1];
	*(u_int32_t *)&cpuinfo[cpus].ident[4] = data[3];
	*(u_int32_t *)&cpuinfo[cpus].ident[8] = data[2];
	cpuinfo[cpus].ident[17] = 0;
	cpuinfo[cpus].max = data[0];

	cpuid(1, data);
	cpuinfo[cpus].signature = data[0];
	cpuinfo[cpus].feature = data[3];

	cpuid(0x80000000, data);
	if (data[0] >= 0x80000004)
	{
		for (i = 0; i < 3; i++)
		{
			cpuid(0x80000002 + i, data);

			*(unsigned int *)&cpuinfo[cpus].brand[16 * i + 0] = data[0];
			*(unsigned int *)&cpuinfo[cpus].brand[16 * i + 4] = data[1];
			*(unsigned int *)&cpuinfo[cpus].brand[16 * i + 8] = data[2];
			*(unsigned int *)&cpuinfo[cpus].brand[16 * i + 12] = data[3];
		}
		cpuinfo[cpus].brand[48] = 0;
	}
	else
	{
		cpuinfo[cpus].brand[0] = 0;
	}

	cpuinfo[cpus].id = cpus;

	cpus++;

	spinUnlock(&cpuInfoLock);

	return (cpus - 1);
}

extern void ap_trampoline_start(), ap_trampoline_end();
void apicMagic(void)
{
	u_int32_t tmp;

	static u_int8_t lowmem_save[512];
	u_int32_t tramp_len = (u_int32_t)((char *)ap_trampoline_end - (char *)ap_trampoline_start);
	/* SIPI start vector = trampoline page number (phys >> 12). */
	u_int32_t sipi = 0x000C4600 | (AP_TRAMPOLINE_PHYS >> 12);

	kprintf("smp: copying %u-byte AP trampoline to 0x%x\n", tramp_len, AP_TRAMPOLINE_PHYS);
	/* Save the low memory (real-mode IVT) we are about to overwrite. */
	memcpy(lowmem_save, (void *)AP_TRAMPOLINE_PHYS, tramp_len);
	memcpy((void *)AP_TRAMPOLINE_PHYS, (char *)ap_trampoline_start, tramp_len);

	apicWrite(0x280, 0); // clear APIC errors
	apicRead(0x280);

	apicWrite(0x300, 0x000C4500); // INIT IPI, all-excluding-self
	for (tmp = 0; tmp < 800000; tmp++)
		asm("nop");     // ~10 ms settle
	apicWrite(0x300, sipi); // STARTUP IPI
	for (tmp = 0; tmp < 800000; tmp++)
		asm("nop");
	apicWrite(0x300, sipi); // second STARTUP IPI (per Intel MP spec)

	/* Let every AP run through the real-mode trampoline into c_ap_boot before
	 * we restore the low memory it is executing from. */
	for (tmp = 0; tmp < 20000000; tmp++)
		asm("nop");

	memcpy((void *)AP_TRAMPOLINE_PHYS, lowmem_save, tramp_len); // restore IVT
}

u_int32_t getEflags()
{
	u_int32_t eflags = 0x0;
	asm("pushfl     \n"
	    "popl %%eax \n"
	    : "=a"(eflags));
	return (eflags);
}

void setEflags(u_int32_t eflags)
{
	asm("pushl %%eax \n"
	    "popfl       \n"
	    :
	    : "a"(eflags));
}

asm(".globl cpuid            \n"
    "cpuid:                  \n"
    "  pushl   %ebx          \n"
    "  pushl   %edi          \n"
    "  movl    12(%esp),%eax \n"
    "  movl    16(%esp),%edi \n"
    "  cpuid                 \n"
    "  movl    %eax,0(%edi)  \n"
    "  movl    %ebx,4(%edi)  \n"
    "  movl    %ecx,8(%edi)  \n"
    "  movl    %edx,12(%edi) \n"
    "  popl    %edi          \n"
    "  popl    %ebx          \n"
    "  ret                   \n");

/***
 END
 ***/
