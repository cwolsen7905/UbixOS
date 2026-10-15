/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * uBix boot-info block: the hand-off record the UEFI loader (stand/efi) passes
 * to the kernel.  Shared by loader and kernel, so it is self-contained (no
 * kernel headers) and fixed-layout.  Passed as: eax = UBIX_BOOTINFO_MAGIC,
 * ebx = physical address of the block (below 4 GB).
 */

#ifndef _UBIXOS_BOOTINFO_H
#define _UBIXOS_BOOTINFO_H

#define UBIX_BOOTINFO_MAGIC 0x55424958U /* "UBIX" */
#define UBIX_BOOTINFO_VERSION 1U

#define UBIX_FB_NONE 0U
#define UBIX_FB_RGBX 1U /* byte order R,G,B,x */
#define UBIX_FB_BGRX 2U /* byte order B,G,R,x */

/** Fixed-layout boot-info block; all addresses are physical. */
struct ubix_bootinfo
{
	unsigned int magic;   /* UBIX_BOOTINFO_MAGIC */
	unsigned int version; /* UBIX_BOOTINFO_VERSION */
	unsigned int size;    /* sizeof(struct ubix_bootinfo) as built */
	unsigned int flags;   /* reserved, 0 */

	/* Firmware memory map: UEFI EFI_MEMORY_DESCRIPTORs, verbatim. */
	unsigned long long memmap_addr;
	unsigned int memmap_desc_size;
	unsigned int memmap_desc_count;

	/* Linear framebuffer from GOP (fb_format == UBIX_FB_NONE if none). */
	unsigned long long fb_base;
	unsigned int fb_width;
	unsigned int fb_height;
	unsigned int fb_pitch; /* bytes per scanline */
	unsigned int fb_format;

	unsigned long long acpi_rsdp;    /* 0 if absent */
	unsigned long long ramdisk_addr; /* 0 if absent */
	unsigned long long ramdisk_size;
	unsigned long long cmdline_addr; /* NUL-terminated, 0 if absent */
	unsigned long long kernel_start; /* physical extent of the loaded kernel */
	unsigned long long kernel_end;
};

#endif /* _UBIXOS_BOOTINFO_H */
