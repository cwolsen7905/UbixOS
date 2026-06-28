/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 machine-dependent physical-memory detection and page-bitmap layout
 * for the QEMU `virt` platform.
 *
 * The arch-neutral bitmap allocator lives in sys/vmm/vmm_memory.c
 * (vmm_find_free_page / free_page / COW / share / audit); this file supplies the
 * two machine-dependent pieces it needs — count_memory() and
 * vmm_mem_map_init() — the aarch64 counterparts of the i386 versions guarded out
 * of vmm_memory.c.
 *
 * QEMU `virt` lays RAM contiguously at 0x40000000 (no low ISA/VGA/BIOS hole).
 * The kernel is linked at the RAM base, so free RAM is simply everything above
 * the page bitmap (which is staged at _end).  The page-index convention is the
 * same as i386: physical address = index * PAGE_SIZE, so RAM pages start at
 * index 0x40000 and the low indices below the RAM base stay permanently
 * unavailable.  All RAM falls inside the 1 GB identity block the MMU maps
 * (mmu.c), so a returned physical page address is directly usable as a kernel
 * pointer — no separate mapping step for the kernel heap.
 */

#include "bringup.h"
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h>     /* sysID */
#include <string.h>          /* memset */
#include <ubixos/cpu_enum.h> /* cpu_enum_add (smp-plan Phase 1) */

/* QEMU `virt` lays physical RAM at 0x40000000.  The size is whatever QEMU was
 * given with -m; aarch64_probe_memory() reads the real figure from the DTB
 * /memory node at boot.  The 512 MB fallback covers a missing/garbled DTB so the
 * kernel still boots (just capped). */
#define AARCH64_RAM_BASE 0x40000000ULL
#define AARCH64_RAM_SIZE_DEFAULT (512ULL * 1024 * 1024)
/* Sanity clamp for a parsed size: never below the 512 MB the desktop needs, and
 * never so large the page bitmap (one vmm_page_info_t per frame) is absurd.  The
 * MMU identity-maps all of PA space (mmu.c), so VA coverage is not the limit. */
#define AARCH64_RAM_SIZE_MIN (512ULL * 1024 * 1024)
#define AARCH64_RAM_SIZE_MAX (16ULL * 1024 * 1024 * 1024)

extern char _end[]; /* end of the kernel image (ldscript.aarch64), page-aligned */

/* Top of usable RAM (physical), in the page-index convention's address space.
 * Defaults to the 512 MB fallback; aarch64_probe_memory() raises it from the DTB
 * before vmm_mem_map_init() stages the bitmap. */
static u_int64_t g_ram_top = AARCH64_RAM_BASE + AARCH64_RAM_SIZE_DEFAULT;

/* Physical RAM base, DTB-driven (the /memory reg base): 0x40000000 on QEMU virt +
 * the Allwinner H618, 0x0 on the Raspberry Pi.  Defaults to AARCH64_RAM_BASE until
 * aarch64_probe_memory() reads the DTB. */
static u_int64_t g_ram_base = AARCH64_RAM_BASE;

/* Physical base of the located device tree, cached by aarch64_probe_memory() so
 * aarch64_enum_cpus() can re-walk the same tree for /cpus.  0 = not found. */
static uintptr_t g_dtb_base = 0;

/* ── minimal flattened-device-tree (FDT) reader ──────────────────────────────
 * Just enough to pull base+size out of the /memory node.  The DTB is big-endian;
 * QEMU `virt` uses #address-cells = #size-cells = 2 (64-bit) at the root, which
 * is what we assume here.  References: the devicetree spec, §5 (the flattened
 * format). */

#define FDT_MAGIC 0xD00DFEEDU
#define FDT_BEGIN_NODE 0x1U
#define FDT_END_NODE 0x2U
#define FDT_PROP 0x3U
#define FDT_NOP 0x4U
#define FDT_END 0x9U

struct fdt_header
{
	u_int32_t magic;
	u_int32_t totalsize;
	u_int32_t off_dt_struct;
	u_int32_t off_dt_strings;
	u_int32_t off_mem_rsvmap;
	u_int32_t version;
	u_int32_t last_comp_version;
	u_int32_t boot_cpuid_phys;
	u_int32_t size_dt_strings;
	u_int32_t size_dt_struct;
};

/** Byte-swap a big-endian 32-bit FDT cell to host order. */
static u_int32_t be32(u_int32_t v)
{
	return ((v & 0xFFU) << 24) | ((v & 0xFF00U) << 8) | ((v & 0xFF0000U) >> 8) | ((v & 0xFF000000U) >> 24);
}

/** Read a big-endian 32-bit cell from a (possibly unaligned) byte pointer. */
static u_int32_t fdt_cell(const u_int8_t *p)
{
	return ((u_int32_t)p[0] << 24) | ((u_int32_t)p[1] << 16) | ((u_int32_t)p[2] << 8) | (u_int32_t)p[3];
}

/**
 * Validate an FDT header at @base and return its /memory size, or 0 if @base
 * does not hold a sane device tree.
 *
 * "Sane" means: FDT magic, a plausible totalsize (4 KB .. 2 MB), and the struct
 * + strings blocks lying within it.  Used both for the pointer the firmware may
 * pass and for the RAM scan fallback, so it must reject false positives firmly.
 */
static u_int64_t fdt_memory_size(uintptr_t base)
{
	const struct fdt_header *h = (const struct fdt_header *)base;
	const u_int8_t *strings, *p, *end;
	u_int32_t totalsize, off_struct, size_struct, off_strings;
	u_int64_t size = 0;
	u_int32_t addr_cells = 2, size_cells = 2; /* root #address/#size-cells (QEMU 2/2, Pi 1/1) */
	int in_memory = 0, depth = 0;

	if (be32(h->magic) != FDT_MAGIC)
		return 0;
	totalsize = be32(h->totalsize);
	off_struct = be32(h->off_dt_struct);
	size_struct = be32(h->size_dt_struct);
	off_strings = be32(h->off_dt_strings);
	/* Reject garbage that merely happens to start with the magic word. */
	if (totalsize < 0x1000 || totalsize > 0x200000)
		return 0;
	if (off_struct + size_struct > totalsize || off_strings > totalsize)
		return 0;

	p = (const u_int8_t *)base + off_struct;
	end = p + size_struct;
	strings = (const u_int8_t *)base + off_strings;

	while (p + 4 <= end)
	{
		u_int32_t token = fdt_cell(p);
		p += 4;

		if (token == FDT_BEGIN_NODE)
		{
			const char *name = (const char *)p;
			/* A node named "memory" or "memory@<addr>". */
			in_memory = (name[0] == 'm' && name[1] == 'e' && name[2] == 'm' && name[3] == 'o' &&
			             name[4] == 'r' && name[5] == 'y' && (name[6] == '\0' || name[6] == '@'));
			/* Skip the null-terminated name, padded up to a 4-byte boundary. */
			while (p < end && *p != '\0')
				p++;
			p = (const u_int8_t *)(((uintptr_t)p + 4) & ~(uintptr_t)3);
			depth++;
		}
		else if (token == FDT_PROP)
		{
			u_int32_t len = fdt_cell(p);
			u_int32_t nameoff = fdt_cell(p + 4);
			const char *pname = (const char *)(strings + nameoff);
			const u_int8_t *val = p + 8;

			/* Root (depth 1) #address-cells / #size-cells govern the /memory reg
			 * layout — QEMU virt uses 2/2, the Raspberry Pi 1/1.  Honor them so the
			 * RAM base/size parse correctly on both. */
			if (depth == 1 && len >= 4 && pname[0] == '#')
			{
				if (pname[1] == 'a') /* #address-cells */
					addr_cells = fdt_cell(val);
				else if (pname[1] == 's') /* #size-cells */
					size_cells = fdt_cell(val);
			}
			/* /memory reg = <base (addr_cells) size (size_cells)>: capture both. */
			if (in_memory && addr_cells >= 1 && addr_cells <= 2 && size_cells >= 1 && size_cells <= 2 &&
			    len >= (addr_cells + size_cells) * 4 && pname[0] == 'r' && pname[1] == 'e' &&
			    pname[2] == 'g' && pname[3] == '\0')
			{
				u_int64_t base = 0;
				u_int32_t i;
				for (i = 0; i < addr_cells; i++)
					base = (base << 32) | fdt_cell(val + i * 4);
				size = 0;
				for (i = 0; i < size_cells; i++)
					size = (size << 32) | fdt_cell(val + (addr_cells + i) * 4);
				g_ram_base = base; /* DTB-driven RAM base (QEMU/H618 0x40000000, Pi 0x0) */
			}
			p = (const u_int8_t *)(((uintptr_t)val + len + 3) & ~(uintptr_t)3);
		}
		else if (token == FDT_END_NODE)
		{
			in_memory = 0;
			depth--;
		}
		else if (token == FDT_END)
		{
			break;
		}
		/* FDT_NOP: nothing to skip. */
	}

	return size;
}

/**
 * Locate the device tree and raise g_ram_top to the real RAM size.
 *
 * QEMU passes the DTB physical address in x0 only under the Linux boot protocol
 * (a flat image with the arm64 header); a raw-ELF kernel like ours is entered
 * with x0 = 0.  So we try the passed pointer first, then scan low RAM for the
 * FDT magic — QEMU `virt` lays the device tree just past the loaded kernel.  On
 * total failure the 512 MB fallback stands and the kernel still boots.  Must run
 * before vmm_mem_map_init().
 *
 * @param dtb_phys x0 at kernel entry — the firmware DTB pointer, or 0.
 */
void aarch64_probe_memory(u_int64_t dtb_phys)
{
	uintptr_t found = 0;
	u_int64_t size = 0;

	/* 1. Trust the firmware pointer if it points at a valid tree. */
	/* Read the DTB through the TTBR1 physmap (PHYSMAP_BASE + phys): the kernel runs
	 * high-half, so a raw physical pointer is not the reliable kernel mapping (it
	 * read as garbage on the Pi, where the firmware parks the DTB low in block 0). */
	if (dtb_phys != 0 && fdt_memory_size(AARCH64_VIRT_OF(dtb_phys)) != 0)
		found = (uintptr_t)dtb_phys;

	/* 2. Otherwise scan low RAM (page-aligned) for the FDT magic.  256 MB is well
	 * clear of where `virt` parks the tree relative to the kernel, and stays
	 * inside the 512 MB fallback window we know exists. */
	if (found == 0)
	{
		uintptr_t a;
		for (a = AARCH64_RAM_BASE; a < AARCH64_RAM_BASE + AARCH64_RAM_SIZE_DEFAULT; a += PAGE_SIZE)
		{
			if (be32(((const struct fdt_header *)AARCH64_VIRT_OF(a))->magic) == FDT_MAGIC &&
			    fdt_memory_size(AARCH64_VIRT_OF(a)) != 0)
			{
				found = a;
				break;
			}
		}
	}

	g_dtb_base = found; /* cache for aarch64_enum_cpus() (0 if nothing found) */

	if (found != 0)
		size = fdt_memory_size(AARCH64_VIRT_OF(found));

	if (size == 0)
	{
		kprintf("vmm(aarch64): no DTB found (x0=0x%lX), RAM capped at 512 MB\n", (u_int64_t)dtb_phys);
		return; /* g_ram_top keeps its 512 MB default */
	}

	if (size < AARCH64_RAM_SIZE_MIN)
		size = AARCH64_RAM_SIZE_MIN;
	if (size > AARCH64_RAM_SIZE_MAX)
		size = AARCH64_RAM_SIZE_MAX;

	g_ram_top = g_ram_base + size;
	kprintf("vmm(aarch64): DTB at 0x%lX, /memory size=%lu MB (RAM 0x%lX..0x%lX)\n",
	        (u_int64_t)found,
	        (u_int64_t)(size / (1024ULL * 1024)),
	        g_ram_base,
	        g_ram_top);
}

/** Compare a NUL-terminated FDT string against @b (exact match). */
static int fdt_streq(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b)
	{
		a++;
		b++;
	}
	return *a == *b;
}

/** True if FDT string @a begins with @prefix. */
static int fdt_startswith(const char *a, const char *prefix)
{
	while (*prefix != '\0')
	{
		if (*a++ != *prefix++)
			return 0;
	}
	return 1;
}

/**
 * Enumerate CPUs from the device-tree /cpus node into the MI cpu_enum table
 * (smp-plan Phase 1) — the aarch64 counterpart of the i386 ACPI MADT parse.
 *
 * Reuses the DTB located by aarch64_probe_memory().  Each child cpu node's `reg`
 * is the MPIDR affinity (the hardware id), `enable-method` is psci/spin-table,
 * and the FDT header's boot_cpuid_phys names the boot CPU.  Parse-and-record
 * only: nothing is launched here.  On a missing/garbled tree it falls back to a
 * single boot CPU so the table is never empty.  Safe to run with the MMU's
 * identity map up (the DTB is in identity-mapped RAM); must run after
 * aarch64_probe_memory().
 */
void aarch64_enum_cpus(void)
{
	const struct fdt_header *h;
	const u_int8_t *strings, *p, *end;
	u_int32_t totalsize, off_struct, size_struct, off_strings;
	u_int64_t boot_hwid;
	int depth = 0;
	int cpus_depth = -1;      /* depth of /cpus, -1 = not inside it */
	int in_cpu = 0;           /* inside a child cpu node */
	u_int32_t addr_cells = 1; /* /cpus #address-cells (ARM/QEMU virt: 1) */
	/* Accumulators for the cpu node currently being walked. */
	u_int64_t cur_hwid = 0;
	int cur_have_reg = 0, cur_is_cpu = 0, cur_disabled = 0;
	u_int8_t cur_method = CPU_ENABLE_UNKNOWN;

	if (g_dtb_base == 0)
	{
		kprintf("cpu_enum(aarch64): no DTB found; assuming 1 boot CPU\n");
		cpu_enum_add(0, 1, 1, CPU_ENABLE_PSCI);
		return;
	}

	h = (const struct fdt_header *)g_dtb_base;
	if (be32(h->magic) != FDT_MAGIC)
	{
		cpu_enum_add(0, 1, 1, CPU_ENABLE_PSCI);
		return;
	}
	totalsize = be32(h->totalsize);
	off_struct = be32(h->off_dt_struct);
	size_struct = be32(h->size_dt_struct);
	off_strings = be32(h->off_dt_strings);
	if (totalsize < 0x1000 || totalsize > 0x200000 || off_struct + size_struct > totalsize ||
	    off_strings > totalsize)
	{
		cpu_enum_add(0, 1, 1, CPU_ENABLE_PSCI);
		return;
	}
	boot_hwid = be32(h->boot_cpuid_phys);

	p = (const u_int8_t *)g_dtb_base + off_struct;
	end = p + size_struct;
	strings = (const u_int8_t *)g_dtb_base + off_strings;

	while (p + 4 <= end)
	{
		u_int32_t token = fdt_cell(p);
		p += 4;

		if (token == FDT_BEGIN_NODE)
		{
			const char *name = (const char *)p;
			depth++;
			/* /cpus is a direct child of the root node (root is depth 1). */
			if (depth == 2 && fdt_streq(name, "cpus"))
			{
				cpus_depth = depth;
				addr_cells = 1; /* reset; #address-cells prop (if any) sets it */
			}
			else if (cpus_depth >= 0 && depth == cpus_depth + 1)
			{
				/* A child of /cpus: start a fresh cpu accumulator. */
				in_cpu = 1;
				cur_hwid = 0;
				cur_have_reg = 0;
				cur_disabled = 0;
				cur_method = CPU_ENABLE_UNKNOWN;
				cur_is_cpu = fdt_startswith(name, "cpu@") || fdt_streq(name, "cpu");
			}
			while (p < end && *p != '\0')
				p++;
			p = (const u_int8_t *)(((uintptr_t)p + 4) & ~(uintptr_t)3);
		}
		else if (token == FDT_PROP)
		{
			u_int32_t len = fdt_cell(p);
			u_int32_t nameoff = fdt_cell(p + 4);
			const char *pname = (const char *)(strings + nameoff);
			const u_int8_t *val = p + 8;

			if (cpus_depth >= 0 && depth == cpus_depth && fdt_streq(pname, "#address-cells") && len >= 4)
			{
				addr_cells = fdt_cell(val);
			}
			else if (in_cpu && depth == cpus_depth + 1)
			{
				if (fdt_streq(pname, "reg") && len >= 4)
				{
					if (addr_cells >= 2 && len >= 8)
						cur_hwid = ((u_int64_t)fdt_cell(val) << 32) | fdt_cell(val + 4);
					else
						cur_hwid = fdt_cell(val);
					cur_have_reg = 1;
				}
				else if (fdt_streq(pname, "device_type") && fdt_streq((const char *)val, "cpu"))
				{
					cur_is_cpu = 1;
				}
				else if (fdt_streq(pname, "enable-method"))
				{
					if (fdt_streq((const char *)val, "psci"))
						cur_method = CPU_ENABLE_PSCI;
					else if (fdt_streq((const char *)val, "spin-table"))
						cur_method = CPU_ENABLE_SPINTABLE;
				}
				else if (fdt_streq(pname, "status") && fdt_streq((const char *)val, "disabled"))
				{
					cur_disabled = 1;
				}
			}
			p = (const u_int8_t *)(((uintptr_t)val + len + 3) & ~(uintptr_t)3);
		}
		else if (token == FDT_END_NODE)
		{
			if (in_cpu && depth == cpus_depth + 1)
			{
				if (cur_is_cpu && cur_have_reg)
					cpu_enum_add(cur_hwid,
					             cur_disabled ? 0 : 1,
					             (cur_hwid == boot_hwid) ? 1 : 0,
					             cur_method != CPU_ENABLE_UNKNOWN ? cur_method : CPU_ENABLE_PSCI);
				in_cpu = 0;
			}
			if (cpus_depth >= 0 && depth == cpus_depth)
				cpus_depth = -1;
			depth--;
		}
		else if (token == FDT_END)
		{
			break;
		}
		/* FDT_NOP: nothing. */
	}

	if (g_cpu_desc_count == 0)
	{
		kprintf("cpu_enum(aarch64): /cpus held no usable cpu node; assuming 1 boot CPU\n");
		cpu_enum_add(boot_hwid, 1, 1, CPU_ENABLE_PSCI);
	}
}

/**
 * Report total physical page count, using the i386 index convention (page
 * index = phys / PAGE_SIZE), so the count spans index 0 .. top-of-RAM.
 *
 * @return number of page-frame slots the bitmap must cover.
 */
u_int32_t count_memory(void)
{
	return (u_int32_t)(g_ram_top / PAGE_SIZE);
}

/**
 * Initialise the physical page bitmap for QEMU `virt`.
 *
 * Stages the bitmap at _end (page-aligned), marks every frame unavailable, then
 * frees the RAM above the bitmap.  The kernel image + bitmap (RAM base .. bitmap
 * end) and everything below the RAM base stay reserved.
 *
 * @return 0 on success.
 */
int vmm_mem_map_init(void)
{
	uintptr_t bitmap_phys;
	u_int32_t bitmap_size, bitmap_end_page;
	u_int32_t num = count_memory();

	/* The page bitmap is staged at the PHYSICAL address just past the kernel image,
	 * and the frame accounting below needs that physical value.  When the kernel is
	 * linked higher-half, _end is a high physmap VMA → convert it; when linked low
	 * (VIRT_OFFSET=0), _end is already physical.  Detect by range so this one site
	 * works for both builds. */
	{
		uintptr_t e = (uintptr_t)_end;
		uintptr_t ephys = (e >= PHYSMAP_BASE) ? AARCH64_PHYS_OF(e) : e;
		bitmap_phys = (ephys + PAGE_SIZE - 1) & ~((uintptr_t)PAGE_SIZE - 1);
	}
	/* Pass the bitmap's *physmap VA* (Convention B), mirroring x86_64's
	 * P2V(bitmap_phys): the MI allocator then reaches vmmMemoryMap through the
	 * TTBR1 physmap, so it stays mapped once TTBR0 is user-only (Phase 4).  The
	 * local bitmap_phys stays physical for the page-index math below. */
	vmm_mem_bitmap_init(AARCH64_VIRT_OF(bitmap_phys), num);

	bitmap_size = num * sizeof(vmm_page_info_t);
	bitmap_end_page = (u_int32_t)((bitmap_phys + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE);

	/* Free RAM = everything from the end of the bitmap to the top of RAM.  The
	 * kernel + bitmap (RAM base .. bitmap end) and everything below the RAM base
	 * stay reserved. */
	vmm_mem_mark_available(bitmap_end_page, num);

	kprintf("vmm(aarch64): RAM 0x%lX..0x%lX, bitmap phys=0x%lX end_page=%u pages=%u free=%u\n",
	        g_ram_base,
	        g_ram_top,
	        (u_int64_t)bitmap_phys,
	        bitmap_end_page,
	        num,
	        vmm_mem_free_pages());

	return 0;
}

/**
 * Allocate @count contiguous physical pages for the kernel heap and return a
 * usable pointer.
 *
 * All RAM is identity-mapped by the 1 GB block MMU (mmu.c), so a physical frame
 * address is directly usable as a kernel virtual pointer — no separate
 * page-table mapping step is needed (unlike i386, which maps scattered frames
 * into contiguous kernel VA).  The pages are zeroed before return.
 *
 * @return pointer to the zeroed run, or NULL if no contiguous run is free.
 */
void *vmm_get_free_malloc_page(u_int16_t count)
{
	uintptr_t base = vmm_find_free_pages_contig(count, sysID);
	void *va;

	if (base == 0)
		return NULL;

	/* Hand out a physmap VA (Convention B): kmalloc'd memory then lives in the
	 * TTBR1 physmap and stays reachable once TTBR0 is user-only (Phase 4).
	 * kmalloc never returns these pages to the allocator, so no reverse map. */
	va = (void *)(uintptr_t)AARCH64_VIRT_OF(base);
	memset(va, 0, (size_t)count * PAGE_SIZE);
	return va;
}

/**
 * Page-eviction hook — no swap device on aarch64 yet, so eviction always fails.
 *
 * @return 0 (no page evicted).
 */
int swap_evict_page(void)
{
	return 0;
}
