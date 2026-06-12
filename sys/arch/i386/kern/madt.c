/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ACPI MADT (Multiple APIC Description Table) parser — i386 CPU enumeration for
 * smp-plan Phase 1.  Walks RSDP -> RSDT -> MADT and records each Processor Local
 * APIC entry in the MI cpu_enum table (<ubixos/cpu_enum.h>), the x86 counterpart
 * of the aarch64 device-tree /cpus reader.
 *
 * Discovery only: this neither starts nor schedules an application processor.
 * It replaces firing INIT-SIPI blindly and self-registering whoever answers with
 * an authoritative, firmware-supplied CPU list (count + APIC ids).  Any parse
 * failure falls back to "the BSP only" so the table is never empty and the
 * existing self-register boot path is unaffected.
 *
 * The RSDP lives in the first 1 MB (identity-mapped); the RSDT/MADT live in ACPI
 * reclaim RAM that may be above the identity map, so each table is mapped with
 * vmm_remap_io_page (identity, VA = phys) before it is read.
 */

#include <i386/smp.h>
#include <ubixos/cpu_enum.h>
#include <vmm/paging.h>  /* vmm_remap_io_page, KERNEL_PAGE_DEFAULT */
#include <lib/kmalloc.h> /* sysID */
#include <lib/kprintf.h>
#include <sys/types.h>

/** Read a little-endian 32-bit field (ACPI fields may be unaligned; x86 allows it). */
static u_int32_t rd32(const u_int8_t *p)
{
	return (u_int32_t)p[0] | ((u_int32_t)p[1] << 8) | ((u_int32_t)p[2] << 16) | ((u_int32_t)p[3] << 24);
}

/** Compare a 4-byte ACPI table signature. */
static int sig4(const u_int8_t *p, const char *s)
{
	return p[0] == (u_int8_t)s[0] && p[1] == (u_int8_t)s[1] && p[2] == (u_int8_t)s[2] && p[3] == (u_int8_t)s[3];
}

/** True if the 8-byte RSDP signature matches "RSD PTR ". */
static int rsdp_sig(const u_int8_t *p)
{
	const char s[8] = {'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '};
	int i;
	for (i = 0; i < 8; i++)
		if (p[i] != (u_int8_t)s[i])
			return 0;
	return 1;
}

/** True if the first @len bytes at @p sum to 0 mod 256 (ACPI checksum). */
static int csum_ok(const u_int8_t *p, u_int32_t len)
{
	u_int8_t sum = 0;
	u_int32_t i;
	for (i = 0; i < len; i++)
		sum = (u_int8_t)(sum + p[i]);
	return sum == 0;
}

/** Identity-map every page covering [phys, phys+len) so the table is readable. */
static void acpi_map(uintptr_t phys, u_int32_t len)
{
	uintptr_t a = phys & ~(uintptr_t)0xFFF;
	uintptr_t e = (phys + len + 0xFFF) & ~(uintptr_t)0xFFF;
	for (; a < e; a += 0x1000)
		vmm_remap_io_page(a, KERNEL_PAGE_DEFAULT, sysID);
}

/**
 * Scan the BIOS EBDA and ROM area for the ACPI RSDP.  Both regions are below
 * 1 MB and identity-mapped, so they are read directly.
 *
 * @return pointer to the RSDP, or NULL if none is found.
 */
static const u_int8_t *find_rsdp(void)
{
	uintptr_t a;
	u_int16_t ebda_seg = *(volatile u_int16_t *)0x40E;
	uintptr_t ebda = (uintptr_t)ebda_seg << 4;

	if (ebda >= 0x80000 && ebda < 0xA0000)
	{
		for (a = ebda; a < ebda + 1024; a += 16)
			if (rsdp_sig((const u_int8_t *)a) && csum_ok((const u_int8_t *)a, 20))
				return (const u_int8_t *)a;
	}
	for (a = 0xE0000; a < 0x100000; a += 16)
		if (rsdp_sig((const u_int8_t *)a) && csum_ok((const u_int8_t *)a, 20))
			return (const u_int8_t *)a;
	return 0;
}

/**
 * Enumerate CPUs from the ACPI MADT into the MI cpu_enum table (smp-plan
 * Phase 1).  Call after the BSP's APIC id is known (cpuInfo()) and before
 * apicMagic().  Discovery only — launches nothing.
 *
 * @param bsp_apic_id the boot CPU's Local APIC id, so its entry is flagged boot.
 */
void acpi_enum_cpus(u_int8_t bsp_apic_id)
{
	const u_int8_t *rsdp, *rsdt, *madt = 0;
	uintptr_t rsdt_phys, madt_phys = 0;
	u_int32_t rsdt_len, madt_len, nent, i, off;

	rsdp = find_rsdp();
	if (rsdp == 0)
	{
		kprintf("madt: no ACPI RSDP found; enumerating the BSP only\n");
		cpu_enum_add(bsp_apic_id, 1, 1, CPU_ENABLE_APIC);
		return;
	}

	rsdt_phys = rd32(rsdp + 16); /* RsdtAddress */
	if (rsdt_phys == 0)
		goto fallback;

	acpi_map(rsdt_phys, 36);
	rsdt = (const u_int8_t *)rsdt_phys;
	if (!sig4(rsdt, "RSDT"))
		goto fallback;
	rsdt_len = rd32(rsdt + 4);
	if (rsdt_len < 36 || rsdt_len > 0x10000)
		goto fallback;
	acpi_map(rsdt_phys, rsdt_len);

	/* RSDT entries are 32-bit physical pointers; find the one signed "APIC". */
	nent = (rsdt_len - 36) / 4;
	for (i = 0; i < nent; i++)
	{
		uintptr_t tp = rd32(rsdt + 36 + i * 4);
		if (tp == 0)
			continue;
		acpi_map(tp, 36);
		if (sig4((const u_int8_t *)tp, "APIC"))
		{
			madt_phys = tp;
			break;
		}
	}
	if (madt_phys == 0)
		goto fallback;

	madt = (const u_int8_t *)madt_phys;
	madt_len = rd32(madt + 4);
	if (madt_len < 44 || madt_len > 0x10000)
		goto fallback;
	acpi_map(madt_phys, madt_len);

	/* Variable-length entries begin at offset 44 (after the 36-byte header,
	 * the 32-bit Local APIC address, and the 32-bit flags). */
	off = 44;
	while (off + 2 <= madt_len)
	{
		u_int8_t type = madt[off];
		u_int8_t elen = madt[off + 1];
		if (elen < 2) /* malformed: avoid an infinite loop */
			break;
		/* Type 0 = Processor Local APIC: [2]=ACPI id, [3]=APIC id, [4..7]=flags
		 * (bit0 = Enabled). */
		if (type == 0 && off + 8 <= madt_len)
		{
			u_int8_t apic_id = madt[off + 3];
			u_int32_t flags = rd32(madt + off + 4);
			cpu_enum_add(apic_id,
			             (flags & 1) ? 1 : 0,
			             (apic_id == bsp_apic_id) ? 1 : 0,
			             CPU_ENABLE_APIC);
		}
		off += elen;
	}

	if (g_cpu_desc_count != 0)
		return;

fallback:
	kprintf("madt: ACPI parse incomplete; enumerating the BSP only\n");
	cpu_enum_add(bsp_apic_id, 1, 1, CPU_ENABLE_APIC);
}
