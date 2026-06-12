/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-independent CPU enumeration table (smp-plan Phase 1).  The per-arch
 * firmware parsers (i386 sys/arch/i386/kern/madt.c, aarch64 the FDT /cpus reader
 * in sys/arch/aarch64/vmm/vmm_machdep.c) fill this through cpu_enum_add(); the MI
 * SMP code reads it back.  See <ubixos/cpu_enum.h>.
 */

#include <ubixos/cpu_enum.h>
#include <lib/kprintf.h>

struct cpu_desc g_cpu_desc[CPU_ENUM_MAX];
unsigned g_cpu_desc_count = 0;

/**
 * Record a firmware-discovered CPU (deduped by hwid).  See the header.
 */
int cpu_enum_add(u_int64_t hwid, u_int8_t enabled, u_int8_t is_boot, u_int8_t method)
{
	unsigned i;

	/* Dedupe: firmware tables may list a CPU more than once.  Refresh the
	 * existing slot's flags rather than adding a duplicate. */
	for (i = 0; i < g_cpu_desc_count; i++)
	{
		if (g_cpu_desc[i].hwid == hwid)
		{
			if (enabled)
				g_cpu_desc[i].enabled = 1;
			if (is_boot)
				g_cpu_desc[i].is_boot = 1;
			if (method != CPU_ENABLE_UNKNOWN)
				g_cpu_desc[i].method = method;
			return (int)i;
		}
	}

	if (g_cpu_desc_count >= CPU_ENUM_MAX)
		return -1;

	i = g_cpu_desc_count++;
	g_cpu_desc[i].hwid = hwid;
	g_cpu_desc[i].enabled = enabled ? 1 : 0;
	g_cpu_desc[i].is_boot = is_boot ? 1 : 0;
	g_cpu_desc[i].method = method;
	return (int)i;
}

/**
 * @return number of enabled CPUs discovered (the header explains the >=1 rule).
 */
unsigned smp_cpu_count(void)
{
	unsigned i, n = 0;

	for (i = 0; i < g_cpu_desc_count; i++)
		if (g_cpu_desc[i].enabled)
			n++;
	return n;
}

/**
 * Map an enable-method code to a short label for the dump.
 */
static const char *cpu_enum_method_name(u_int8_t method)
{
	switch (method)
	{
		case CPU_ENABLE_APIC:
			return "apic";
		case CPU_ENABLE_PSCI:
			return "psci";
		case CPU_ENABLE_SPINTABLE:
			return "spintable";
		default:
			return "unknown";
	}
}

/**
 * kprintf the enumeration table.
 */
void cpu_enum_dump(void)
{
	unsigned i;

	kprintf("cpu_enum: %u CPU(s) discovered (%u enabled)\n", g_cpu_desc_count, smp_cpu_count());
	for (i = 0; i < g_cpu_desc_count; i++)
		/* hwid printed 32-bit: %lX is 32-bit `long` on i386, so passing a
		 * u_int64_t there misaligns the varargs.  APIC ids / MPIDR affinities
		 * fit in 32 bits, so cast and use %X — portable on both arches. */
		kprintf("cpu_enum:   cpu%u hwid=0x%X %s%s method=%s\n",
		        i,
		        (u_int32_t)g_cpu_desc[i].hwid,
		        g_cpu_desc[i].enabled ? "enabled" : "disabled",
		        g_cpu_desc[i].is_boot ? " boot" : "",
		        cpu_enum_method_name(g_cpu_desc[i].method));
}
