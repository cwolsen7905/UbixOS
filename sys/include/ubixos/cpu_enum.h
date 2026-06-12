/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-independent CPU enumeration table (smp-plan Phase 1).
 *
 * Both architectures discover their CPUs from firmware before any application
 * processor is launched — i386 from the ACPI MADT, aarch64 from the device-tree
 * /cpus node — and record what they find here through one common contract.  The
 * MI scheduler/SMP code then reads this table (smp_cpu_count(), g_cpu_desc[])
 * instead of an arch-specific structure; only the *source* of each id differs.
 *
 * This is enumeration only: populating the table neither launches nor schedules
 * anything.  It is safe to fill on a uniprocessor (one boot CPU) and is the
 * authoritative count the later AP-launch phases consume.
 */
#ifndef _UBIXOS_CPU_ENUM_H
#define _UBIXOS_CPU_ENUM_H

#include <sys/types.h>

#define CPU_ENUM_MAX 8 /* matches the i386 cpuinfo[8] / g_pcpu[] ceiling */

/* How firmware says a CPU is started — the AP-launch phase dispatches on this. */
enum cpu_enable_method
{
	CPU_ENABLE_UNKNOWN = 0,
	CPU_ENABLE_APIC = 1,     /* i386: INIT-SIPI-SIPI via the Local APIC */
	CPU_ENABLE_PSCI = 2,     /* aarch64: PSCI CPU_ON */
	CPU_ENABLE_SPINTABLE = 3 /* aarch64: spin-table */
};

struct cpu_desc
{
	u_int64_t hwid;  /* hardware id: APIC id (i386) or MPIDR affinity (aarch64) */
	u_int8_t enabled; /* firmware marks the CPU usable (enabled or online-capable) */
	u_int8_t is_boot; /* this is the boot/primary CPU */
	u_int8_t method;  /* enum cpu_enable_method */
};

extern struct cpu_desc g_cpu_desc[CPU_ENUM_MAX];
extern unsigned g_cpu_desc_count; /* slots used in g_cpu_desc[] */

/**
 * Record a firmware-discovered CPU.  Dedupes by hwid (firmware tables can list
 * the same CPU twice), so callers may add freely.
 *
 * @param hwid    APIC id (i386) or MPIDR affinity (aarch64).
 * @param enabled non-zero if firmware marks the CPU usable.
 * @param is_boot non-zero for the boot CPU.
 * @param method  enum cpu_enable_method.
 * @return slot index, or -1 if the table is full.
 */
int cpu_enum_add(u_int64_t hwid, u_int8_t enabled, u_int8_t is_boot, u_int8_t method);

/**
 * @return number of *enabled* CPUs discovered.  At least 1 once enumeration has
 * run (the boot CPU); 0 only if it has not run yet.
 */
unsigned smp_cpu_count(void);

/** kprintf the enumeration table (one line per CPU). */
void cpu_enum_dump(void);

#endif /* _UBIXOS_CPU_ENUM_H */
