/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * uBix UEFI loader (machine-independent part).  Written from the UEFI
 * specification: open the volume the image was loaded from, load the kernel
 * ELF64 to its physical load address, gather the memory map, GOP framebuffer and
 * ACPI RSDP, exit boot services and hand over a struct ubix_bootinfo.
 */

#include "stand.h"
#include <ubixos/bootinfo.h>

#define PAGE_SZ 4096ULL
#define KERNEL_RESERVE (32ULL * 1024 * 1024) /* best-effort free run after the image (kernel's page bitmap) */
#define MEMMAP_PAGES 32ULL

typedef struct
{
	uint8_t ident[16];
	uint16_t type, machine;
	uint32_t version;
	uint64_t entry, phoff, shoff;
	uint32_t flags;
	uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} elf64_ehdr_t;

typedef struct
{
	uint32_t type, flags;
	uint64_t offset, vaddr, paddr, filesz, memsz, align;
} elf64_phdr_t;

#define PT_LOAD 1U

static efi_system_table_t *g_st;
static efi_boot_services_t *g_bs;

static const efi_guid_t g_guid_loaded_image = {
    0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};
static const efi_guid_t g_guid_simple_fs = {
    0x964E5B22, 0x6459, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};
static const efi_guid_t g_guid_gop = {0x9042A9DE, 0x23DC, 0x4A38, {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A}};
static const efi_guid_t g_guid_file_info = {
    0x09576E92, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};
static const efi_guid_t g_guid_acpi20 = {0x8868E871, 0xE4F1, 0x11D3, {0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81}};
static const efi_guid_t g_guid_acpi10 = {0xEB9D2D30, 0x2D88, 0x11D3, {0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}};

/** Print an ASCII string on the firmware console. */
static void say(const char *s)
{
	char16_t buf[2];

	if (g_st == 0 || g_st->con_out == 0)
		return;
	buf[1] = 0;
	for (; *s; s++)
	{
		if (*s == '\n')
		{
			buf[0] = '\r';
			g_st->con_out->output_string(g_st->con_out, buf);
		}
		buf[0] = (char16_t)*s;
		g_st->con_out->output_string(g_st->con_out, buf);
	}
}

/** Print @v as 0x-prefixed hex. */
static void say_hex(uint64_t v)
{
	char b[19];
	int i;

	b[0] = '0';
	b[1] = 'x';
	for (i = 0; i < 16; i++)
		b[2 + i] = "0123456789abcdef"[(v >> (60 - 4 * i)) & 0xF];
	b[18] = 0;
	say(b);
}

/** Compare 8 bytes. */
static int same8(const uint8_t *a, const uint8_t *b)
{
	int i;

	for (i = 0; i < 8; i++)
		if (a[i] != b[i])
			return 0;
	return 1;
}

/** Compare two GUIDs. */
static int guid_eq(const efi_guid_t *a, const efi_guid_t *b)
{
	return a->d1 == b->d1 && a->d2 == b->d2 && a->d3 == b->d3 && same8(a->d4, b->d4);
}

/**
 * Read the whole file @name from @root into freshly allocated pages.
 * @return 0 on success (*addr, *size set), -1 if absent or unreadable.
 */
static int read_file(efi_file_t *root, const char16_t *name, uint64_t *addr, uint64_t *size)
{
	efi_file_t *f;
	uint8_t info[256];
	uintn_t isz = sizeof(info), n;
	uint64_t buf, len;

	if (root->open(root, &f, name, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS)
		return -1;
	if (f->get_info(f, &g_guid_file_info, &isz, info) != EFI_SUCCESS)
	{
		f->close(f);
		return -1;
	}
	len = ((efi_file_info_t *)info)->file_size;
	if (g_bs->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, (len + PAGE_SZ - 1) / PAGE_SZ + 1, &buf) !=
	    EFI_SUCCESS)
	{
		f->close(f);
		return -1;
	}
	n = len;
	if (f->read(f, &n, (void *)(uintptr_t)buf) != EFI_SUCCESS || n != len)
	{
		f->close(f);
		return -1;
	}
	f->close(f);
	*addr = buf;
	*size = len;
	return 0;
}

/**
 * Place the PT_LOAD segments of the ELF64 image at @img at their physical
 * addresses.  @return 0 on success, -1 on error; *entry is the physical entry
 * point, [*lo,*hi) the loaded physical extent.
 */
static int load_elf(const uint8_t *img, uint64_t imgsz, uint64_t *entry, uint64_t *lo, uint64_t *hi)
{
	const elf64_ehdr_t *eh = (const elf64_ehdr_t *)img;
	const elf64_phdr_t *ph;
	uint64_t first = ~0ULL, last = 0, base, pages, reserve, i;
	uint64_t ent = 0;

	if (imgsz < sizeof(*eh) || img[0] != 0x7F || img[1] != 'E' || img[2] != 'L' || img[3] != 'F' || img[4] != 2 ||
	    eh->machine != md_elf_machine || eh->phentsize != sizeof(elf64_phdr_t) ||
	    eh->phoff + (uint64_t)eh->phnum * sizeof(elf64_phdr_t) > imgsz)
	{
		say("loader: kernel is not a valid ELF64 for this machine\n");
		return -1;
	}
	ph = (const elf64_phdr_t *)(img + eh->phoff);
	for (i = 0; i < eh->phnum; i++)
	{
		if (ph[i].type != PT_LOAD || ph[i].memsz == 0)
			continue;
		if (ph[i].offset + ph[i].filesz > imgsz || ph[i].filesz > ph[i].memsz)
			return -1;
		if (ph[i].paddr < first)
			first = ph[i].paddr;
		if (ph[i].paddr + ph[i].memsz > last)
			last = ph[i].paddr + ph[i].memsz;
		if (eh->entry >= ph[i].vaddr && eh->entry < ph[i].vaddr + ph[i].memsz)
			ent = eh->entry - ph[i].vaddr + ph[i].paddr;
	}
	if (first >= last || ent == 0 || first >= 0x100000000ULL)
		return -1;
	base = first & ~(PAGE_SZ - 1);
	pages = (last - base + PAGE_SZ - 1) / PAGE_SZ;
	reserve = KERNEL_RESERVE / PAGE_SZ;
	/* Claim the image plus a free run after it; fall back to the image alone. */
	{
		uint64_t a = base;

		if (g_bs->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, pages + reserve, &a) != EFI_SUCCESS)
		{
			a = base;
			say("loader: warning: no reserve after kernel image\n");
			if (g_bs->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, pages, &a) != EFI_SUCCESS)
			{
				say("loader: cannot allocate kernel load address ");
				say_hex(base);
				say("\n");
				return -1;
			}
		}
	}
	memset((void *)(uintptr_t)base, 0, pages * PAGE_SZ);
	for (i = 0; i < eh->phnum; i++)
		if (ph[i].type == PT_LOAD && ph[i].filesz)
			memcpy((void *)(uintptr_t)ph[i].paddr, img + ph[i].offset, ph[i].filesz);
	*entry = ent;
	*lo = base;
	*hi = last;
	return 0;
}

/** Find the ACPI RSDP in the configuration table (2.0 first). @return phys or 0. */
static uint64_t find_rsdp(void)
{
	uintn_t i;
	uint64_t v1 = 0;

	for (i = 0; i < g_st->number_of_table_entries; i++)
	{
		if (guid_eq(&g_st->configuration_table[i].guid, &g_guid_acpi20))
			return (uint64_t)(uintptr_t)g_st->configuration_table[i].table;
		if (guid_eq(&g_st->configuration_table[i].guid, &g_guid_acpi10))
			v1 = (uint64_t)(uintptr_t)g_st->configuration_table[i].table;
	}
	return v1;
}

/** Fill the framebuffer fields from GOP, if present. */
static void get_fb(struct ubix_bootinfo *bi)
{
	efi_gop_t *gop = 0;

	bi->fb_format = UBIX_FB_NONE;
	if (g_bs->locate_protocol(&g_guid_gop, 0, (void **)&gop) != EFI_SUCCESS || gop == 0 || gop->mode == 0 ||
	    gop->mode->info == 0)
		return;
	if (gop->mode->info->pixel_format == EFI_GOP_RGBX)
		bi->fb_format = UBIX_FB_RGBX;
	else if (gop->mode->info->pixel_format == EFI_GOP_BGRX)
		bi->fb_format = UBIX_FB_BGRX;
	else
		return;
	bi->fb_base = gop->mode->fb_base;
	bi->fb_width = gop->mode->info->h_res;
	bi->fb_height = gop->mode->info->v_res;
	bi->fb_pitch = gop->mode->info->pixels_per_scan_line * 4;
}

/**
 * Loader entry, called by the firmware.  Only returns (with an error status)
 * if the kernel could not be started.
 */
efi_status_t efi_main(efi_handle_t image, efi_system_table_t *st)
{
	static const char16_t *const kpaths[] = {
	    u"\\boot\\kernel\\kernel", u"\\kernel\\kernel", u"\\EFI\\BOOT\\kernel", 0};
	efi_loaded_image_t *li = 0;
	efi_simple_fs_t *fs = 0;
	efi_file_t *root = 0;
	struct ubix_bootinfo *bi;
	uint64_t kaddr = 0, ksize = 0, entry, lo, hi, bia, mma;
	uintn_t mapsz, key, dsz;
	uint32_t dver;
	int i;

	g_st = st;
	g_bs = st->boot_services;
	g_bs->set_watchdog_timer(0, 0, 0, 0);
	say("uBixOS UEFI loader\n");

	if (g_bs->handle_protocol(image, &g_guid_loaded_image, (void **)&li) != EFI_SUCCESS ||
	    g_bs->handle_protocol(li->device_handle, &g_guid_simple_fs, (void **)&fs) != EFI_SUCCESS ||
	    fs->open_volume(fs, &root) != EFI_SUCCESS)
	{
		say("loader: cannot open the boot volume\n");
		return EFI_NOT_FOUND;
	}
	for (i = 0; kpaths[i]; i++)
		if (read_file(root, kpaths[i], &kaddr, &ksize) == 0)
			break;
	if (kpaths[i] == 0)
	{
		say("loader: kernel not found on the boot volume\n");
		return EFI_NOT_FOUND;
	}
	if (load_elf((const uint8_t *)(uintptr_t)kaddr, ksize, &entry, &lo, &hi) != 0)
		return EFI_ERR(2);

	bia = 0xFFFFFFFFULL;
	if (g_bs->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, 1, &bia) != EFI_SUCCESS)
		return EFI_ERR(9);
	bi = (struct ubix_bootinfo *)(uintptr_t)bia;
	memset(bi, 0, PAGE_SZ);
	bi->magic = UBIX_BOOTINFO_MAGIC;
	bi->version = UBIX_BOOTINFO_VERSION;
	bi->size = sizeof(*bi);
	bi->kernel_start = lo;
	bi->kernel_end = hi;
	bi->acpi_rsdp = find_rsdp();
	get_fb(bi);
	/* Optional live-root ramdisk (B3). */
	if (read_file(root, u"\\boot\\ramdisk", &bi->ramdisk_addr, &bi->ramdisk_size) != 0)
	{
		bi->ramdisk_addr = 0;
		bi->ramdisk_size = 0;
	}
	root->close(root);

	mma = 0xFFFFFFFFULL;
	if (g_bs->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, MEMMAP_PAGES, &mma) != EFI_SUCCESS)
		return EFI_ERR(9);
	if (md_prepare(g_bs) != 0)
		return EFI_ERR(9);

	say("loader: starting kernel entry ");
	say_hex(entry);
	say("\n");

	/* The map key changes with any allocation; fetch, then exit, retrying on a
	 * stale key (spec 7.4.6). */
	for (i = 0; i < 8; i++)
	{
		mapsz = MEMMAP_PAGES * PAGE_SZ;
		if (g_bs->get_memory_map(&mapsz, (efi_memory_descriptor_t *)(uintptr_t)mma, &key, &dsz, &dver) !=
		    EFI_SUCCESS)
			return EFI_ERR(9);
		if (g_bs->exit_boot_services(image, key) == EFI_SUCCESS)
			break;
	}
	if (i == 8)
		return EFI_ERR(9);
	bi->memmap_addr = mma;
	bi->memmap_desc_size = (uint32_t)dsz;
	bi->memmap_desc_count = (uint32_t)(mapsz / dsz);

	md_handoff(bia, entry);
}
