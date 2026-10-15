/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 machine-dependent physical-memory setup (bring-up Phase 3).  Stages the
 * machine-independent page bitmap (sys/vmm/vmm_memory.c) over RAM and provides
 * the kernel-heap page source kmalloc() needs.  All RAM is identity-mapped by
 * start.S's 1 GB map, so a physical frame is directly usable as a 64-bit pointer
 * (like aarch64; unlike i386's scattered-frame kernel VA).  Sibling of
 * aarch64/vmm/vmm_machdep.c.
 */

#include <ubixos/bootinfo.h>
#include "../x86_64.h"
#include <vmm/vmm.h>
#include <vmm/vm_filecache.h> /* vm_filecache_reclaim_one — clean-file-page eviction */
#include <lib/kmalloc.h>      /* sysID */
#include <string.h>

/* Physmap virtual address of physical @p as an integer (typed-pointer casts stay direct). */
#define PHYS_VA(p) ((uintptr_t)((u64)(p) + PHYSMAP_BASE))

extern char _end[]; /* end of the kernel image (ldscript) */

/*
 * Physical memory.  QEMU boots the kernel via PVH: the boot info pointer is an
 * hvm_start_info whose memmap (version >= 1) is the e820 table.  Only RAM below
 * 4 GB is used — that is what start.S's physmap covers.  Without a PVH map the
 * old bring-up assumption (256 MB) still applies.  (This was a fixed 256 MB, so
 * a 1 or 2 GB guest still had only 256 MB to run programs in.)
 */
#define X86_64_FALLBACK_RAM (256UL * 1024 * 1024)
#define X86_64_PHYSMAP_TOP (4UL * 1024 * 1024 * 1024)
#define HVM_START_MAGIC 0x336ec578U
#define E820_RAM 1

struct hvm_start_info
{
	u32 magic;
	u32 version;
	u32 flags;
	u32 nr_modules;
	u64 modlist_paddr;
	u64 cmdline_paddr;
	u64 rsdp_paddr;
	u64 memmap_paddr;
	u32 memmap_entries;
	u32 reserved;
};

struct hvm_memmap_entry
{
	u64 addr;
	u64 size;
	u32 type;
	u32 reserved;
};

/* Usable RAM ranges, normalised from whichever boot protocol supplied them. */
#define X86_64_MAX_RANGES 256
#define UEFI_LOADER_CODE 1
#define UEFI_LOADER_DATA 2
#define UEFI_BOOT_CODE 3
#define UEFI_BOOT_DATA 4
#define UEFI_CONVENTIONAL 7

struct ram_range
{
	u64 addr;
	u64 end;
};

static struct ram_range g_ranges[X86_64_MAX_RANGES];
static u32 g_nranges;

/* Copy of the loader's boot-info block (its page is ordinary RAM afterwards). */
struct ubix_bootinfo g_x86_64_bootinfo;
int g_x86_64_have_bootinfo;

/** Append [addr,end) to the range table if non-empty and there is room. */
static void range_push(u64 addr, u64 end)
{
	if (addr < end && g_nranges < X86_64_MAX_RANGES)
	{
		g_ranges[g_nranges].addr = addr;
		g_ranges[g_nranges].end = end;
		g_nranges++;
	}
}

/** Add [addr,end) clipped to the physmap and with the ramdisk carved out. */
static void range_add(u64 addr, u64 end)
{
	if (end > X86_64_PHYSMAP_TOP)
		end = X86_64_PHYSMAP_TOP;
	if (addr >= end)
		return;
	if (g_x86_64_have_bootinfo && g_x86_64_bootinfo.ramdisk_size != 0)
	{
		u64 rl = g_x86_64_bootinfo.ramdisk_addr & ~((u64)PAGE_SIZE - 1);
		u64 rh = (g_x86_64_bootinfo.ramdisk_addr + g_x86_64_bootinfo.ramdisk_size + PAGE_SIZE - 1) &
		         ~((u64)PAGE_SIZE - 1);

		if (addr < rh && end > rl)
		{
			range_push(addr, rl < end ? rl : end);
			range_push(rh > addr ? rh : addr, end);
			return;
		}
	}
	range_push(addr, end);
}

/**
 * Fill g_ranges from the boot protocol: the uBix UEFI loader's boot-info
 * (EFI memory map) or the PVH e820.  Leaves g_nranges 0 if neither is present.
 */
static void boot_ranges(u32 boot_magic, u32 boot_info)
{
	u32 i;

	g_nranges = 0;
	if (boot_magic == UBIX_BOOTINFO_MAGIC && boot_info != 0)
	{
		const struct ubix_bootinfo *bi = (const struct ubix_bootinfo *)PHYS_VA(boot_info);
		const u8 *md;

		if (bi->magic != UBIX_BOOTINFO_MAGIC || bi->memmap_addr == 0 || bi->memmap_desc_size < 40)
			return;
		g_x86_64_bootinfo = *bi;
		g_x86_64_have_bootinfo = 1;
		md = (const u8 *)PHYS_VA(bi->memmap_addr);
		for (i = 0; i < bi->memmap_desc_count; i++, md += bi->memmap_desc_size)
		{
			u32 type = *(const u32 *)md;
			u64 start = *(const u64 *)(md + 8);
			u64 pages = *(const u64 *)(md + 24);

			if (type == UEFI_LOADER_CODE || type == UEFI_LOADER_DATA || type == UEFI_BOOT_CODE ||
			    type == UEFI_BOOT_DATA || type == UEFI_CONVENTIONAL)
				range_add(start, start + pages * PAGE_SIZE);
		}
	}
	else
	{
		const struct hvm_start_info *si;
		const struct hvm_memmap_entry *mm;

		if (boot_info == 0)
			return;
		si = (const struct hvm_start_info *)PHYS_VA(boot_info);
		if (si->magic != HVM_START_MAGIC || si->version < 1 || si->memmap_paddr == 0 || si->memmap_entries == 0)
			return;
		mm = (const struct hvm_memmap_entry *)PHYS_VA(si->memmap_paddr);
		for (i = 0; i < si->memmap_entries; i++)
			if (mm[i].type == E820_RAM)
				range_add(mm[i].addr, mm[i].addr + mm[i].size);
	}
}

/**
 * Initialise the physical page allocator: size it to the highest usable RAM
 * frame (below 4 GB), stage the bitmap at _end, then free each RAM range
 * above the bitmap.
 */
void x86_64_mem_init(u32 boot_magic, u32 boot_info)
{
	u32 nmm, i, num, bitmap_end_page;
	u64 top = 0;
	uintptr_t bitmap_phys = (KERN_VIRT_TO_PHYS(_end) + PAGE_SIZE - 1) & ~((uintptr_t)PAGE_SIZE - 1);

	boot_ranges(boot_magic, boot_info);
	nmm = g_nranges;
	for (i = 0; i < nmm; i++)
		if (g_ranges[i].end > top)
			top = g_ranges[i].end;
	if (top == 0)
		top = X86_64_FALLBACK_RAM;
	num = (u32)(top / PAGE_SIZE);

	/* vmm_mem_bitmap_init stores the bitmap *through* the address it is given, so
	 * pass the physmap pointer; the frame-index math stays in physical units. */
	vmm_mem_bitmap_init((uintptr_t)P2V(bitmap_phys), num);
	bitmap_end_page = (u32)((bitmap_phys + (u64)num * sizeof(vmm_page_info_t) + PAGE_SIZE - 1) / PAGE_SIZE);

	if (nmm == 0)
	{
		vmm_mem_mark_available(bitmap_end_page, num);
	}
	else
	{
		for (i = 0; i < nmm; i++)
		{
			u64 first, last;
			first = (g_ranges[i].addr + PAGE_SIZE - 1) / PAGE_SIZE;
			last = g_ranges[i].end / PAGE_SIZE;
			if (first < bitmap_end_page)
				first = bitmap_end_page;
			if (last > num)
				last = num;
			if (first < last)
				vmm_mem_mark_available((u32)first, (u32)last);
		}
	}

	serial_puts("x86_64: phys page allocator up (RAM ");
	serial_putdec(top >> 20);
	serial_puts(nmm ? (g_x86_64_have_bootinfo ? " MB from the UEFI memory map" : " MB from the PVH e820 map")
	                : " MB assumed");
	serial_puts(", bitmap @ ");
	serial_puthex(bitmap_phys);
	serial_puts(", free pages ");
	serial_putdec(vmm_mem_free_pages());
	serial_puts(")\n");
}

/**
 * Allocate @count contiguous physical pages for the kernel heap, zeroed.  The
 * allocator returns a physical address; the kernel reaches it through the physmap.
 */
void *vmm_get_free_malloc_page(u_int16_t count)
{
	uintptr_t base = vmm_find_free_pages_contig(count, sysID);

	if (base == 0)
		return 0;
	memset(P2V(base), 0, (unsigned long)count * PAGE_SIZE);
	return P2V(base);
}

/** Page-eviction hook (allocator OOM path): reclaim one clean cached file page
 * from the file-cache inactive LRU — needs no swap device (re-read from disk if
 * faulted again).  See the aarch64 twin + vm_filecache_reclaim_one(). */
int swap_evict_page(void)
{
	return ((int)vm_filecache_reclaim_one());
}
