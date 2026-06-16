/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ACPI MADT (Multiple APIC Description Table) parser — x86_64 CPU enumeration
 * (smp-plan Phase 1).  Walks RSDP -> RSDT -> MADT and records each Processor Local
 * APIC entry in the MI cpu_enum table (<ubixos/cpu_enum.h>), the foundation for
 * starting the application processors.  Discovery only — it launches nothing.
 *
 * The logic mirrors i386's madt.c; the only difference is table access: the
 * x86_64 kernel direct-maps all low RAM through the physmap (P2V), so the RSDP
 * (in the first 1 MB) and the RSDT/MADT (in ACPI-reclaim RAM, low memory under
 * QEMU) are read via P2V(phys) rather than i386's per-table identity remap.
 */

#include "../x86_64.h"
#include <ubixos/cpu_enum.h>

/** Read a little-endian 32-bit value from a kernel pointer. */
static u32 rd32(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/** Compare a 4-byte ACPI table signature. */
static int sig4(const u8 *p, const char *s)
{
	return p[0] == (u8)s[0] && p[1] == (u8)s[1] && p[2] == (u8)s[2] && p[3] == (u8)s[3];
}

/** True if the 8-byte RSDP signature matches "RSD PTR ". */
static int rsdp_sig(const u8 *p)
{
	static const char sig[8] = {'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '};
	for (int i = 0; i < 8; i++)
		if (p[i] != (u8)sig[i])
			return 0;
	return 1;
}

/** ACPI checksum: the bytes of a structure sum to 0 (mod 256). */
static int csum_ok(const u8 *p, u32 len)
{
	u8 sum = 0;
	for (u32 i = 0; i < len; i++)
		sum = (u8)(sum + p[i]);
	return sum == 0;
}

/**
 * Scan the BIOS EBDA + ROM area (both below 1 MB, in the physmap) for the ACPI
 * RSDP.  @return a P2V pointer to the RSDP, or NULL.
 */
static const u8 *find_rsdp(void)
{
	uintptr_t a;
	/* EBDA: its segment is at physical 0x40E (the BDA word).  Scan the EBDA and
	 * the BIOS ROM area 0xE0000..0xFFFFF on 16-byte boundaries. */
	u16 ebda_seg = *(volatile u16 *)P2V(0x40E);
	uintptr_t ebda = (uintptr_t)ebda_seg << 4;
	if (ebda >= 0x400 && ebda < 0xA0000)
	{
		for (a = ebda; a < ebda + 1024; a += 16)
		{
			const u8 *p = (const u8 *)P2V(a);
			if (rsdp_sig(p) && csum_ok(p, 20))
				return p;
		}
	}
	for (a = 0xE0000; a < 0x100000; a += 16)
	{
		const u8 *p = (const u8 *)P2V(a);
		if (rsdp_sig(p) && csum_ok(p, 20))
			return p;
	}
	return 0;
}

/** @return the boot CPU's initial Local APIC id (CPUID leaf 1, EBX[31:24]). */
static u8 bsp_apic_id(void)
{
	u32 a = 1, b = 0, c = 0, d = 0;
	__asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
	return (u8)(b >> 24);
}

/**
 * Enumerate CPUs from the ACPI MADT into the MI cpu_enum table.  Discovery only;
 * call once at boot.  Falls back to the BSP alone if ACPI can't be parsed.
 */
void x86_64_enum_cpus(void)
{
	const u8 *rsdp, *rsdt, *madt = 0;
	uintptr_t rsdt_phys, madt_phys = 0;
	u32 rsdt_len, madt_len, nent, i, off;
	u8 bsp = bsp_apic_id();

	rsdp = find_rsdp();
	if (rsdp == 0)
	{
		kprintf("madt: no ACPI RSDP found; enumerating the BSP only\n");
		cpu_enum_add(bsp, 1, 1, CPU_ENABLE_APIC);
		return;
	}

	rsdt_phys = rd32(rsdp + 16); /* RsdtAddress (32-bit) */
	if (rsdt_phys == 0)
		goto fallback;
	rsdt = (const u8 *)P2V(rsdt_phys);
	if (!sig4(rsdt, "RSDT"))
		goto fallback;
	rsdt_len = rd32(rsdt + 4);
	if (rsdt_len < 36 || rsdt_len > 0x10000)
		goto fallback;

	/* RSDT entries are 32-bit physical pointers; find the one signed "APIC". */
	nent = (rsdt_len - 36) / 4;
	for (i = 0; i < nent; i++)
	{
		uintptr_t tp = rd32(rsdt + 36 + i * 4);
		if (tp == 0)
			continue;
		if (sig4((const u8 *)P2V(tp), "APIC"))
		{
			madt_phys = tp;
			break;
		}
	}
	if (madt_phys == 0)
		goto fallback;

	madt = (const u8 *)P2V(madt_phys);
	madt_len = rd32(madt + 4);
	if (madt_len < 44 || madt_len > 0x10000)
		goto fallback;

	/* Variable-length entries begin at offset 44 (36-byte header + the 32-bit
	 * Local APIC address + the 32-bit flags). */
	off = 44;
	while (off + 2 <= madt_len)
	{
		u8 type = madt[off];
		u8 elen = madt[off + 1];
		if (elen < 2) /* malformed: avoid an infinite loop */
			break;
		/* Type 0 = Processor Local APIC: [3]=APIC id, [4..7]=flags (bit0=Enabled). */
		if (type == 0 && off + 8 <= madt_len)
		{
			u8 apic_id = madt[off + 3];
			u32 flags = rd32(madt + off + 4);
			cpu_enum_add(apic_id, (flags & 1) ? 1 : 0, (apic_id == bsp) ? 1 : 0, CPU_ENABLE_APIC);
		}
		off += elen;
	}

	if (g_cpu_desc_count != 0)
	{
		kprintf("madt: enumerated %u CPU(s) from ACPI\n", g_cpu_desc_count);
		return;
	}

fallback:
	kprintf("madt: ACPI parse incomplete; enumerating the BSP only\n");
	cpu_enum_add(bsp, 1, 1, CPU_ENABLE_APIC);
}
