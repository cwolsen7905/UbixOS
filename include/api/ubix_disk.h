/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubix_disk — userland (uapi) view of the kernel block-device / partition
 * enumeration, used by the Disk Utility app and a future `diskutil` CLI.  The
 * kernel fills this struct verbatim (see sys/include/dev/disk.h, which MUST keep
 * the identical byte layout) via the native API syscall 68.
 *
 * See docs/design/disk-utility-plan.md.
 */
#ifndef _API_UBIX_DISK_H
#define _API_UBIX_DISK_H

#include <stdint.h>

#define UBIX_SYS_DISK_QUERY 68

#define UBIX_DISK_MAX 16     /* cap on entries returned in one query */

/* entry kind */
#define UBIX_DK_DISK 0 /* a whole drive */
#define UBIX_DK_PART 1 /* a partition on a drive */

/* flags */
#define UBIX_DF_REMOVABLE 0x1 /* USB / hotplug media */
#define UBIX_DF_MOUNTED 0x2   /* partition is mounted */
#define UBIX_DF_READONLY 0x4

/* MBR partition type bytes uBixOS uses (see tools/mkimage*.sh). */
#define UBIX_PT_FAT32 0x0C
#define UBIX_PT_SWAP 0x82
#define UBIX_PT_UBPOOL 0x9C

/*
 * One enumeration entry.  Whole disks and their partitions both appear in the
 * array; a partition's `parent` is the array index of its owning disk.  64-bit
 * fields are placed first so the layout is identical on the 32- and 64-bit ABIs
 * with no implicit padding — the kernel and userland share this struct verbatim.
 */
struct ubix_disk_info
{
	char     name[32];      /* "ad0", "ad0s1", "vtblk0", "vtblk0s3" */
	uint64_t lba_start;     /* first LBA on the parent disk (PART; 0 for a disk) */
	uint64_t sectors;       /* length in sectors */
	uint32_t sector_size;   /* bytes per sector (512) */
	uint32_t flags;         /* UBIX_DF_* */
	uint16_t major;         /* device major */
	uint16_t minor;         /* device minor (0 = whole disk) */
	uint8_t  kind;          /* UBIX_DK_DISK | UBIX_DK_PART */
	uint8_t  parent;        /* owning-disk index (PART only) */
	uint8_t  mbr_type;      /* MBR partition type byte (PART) */
	uint8_t  _pad;          /* keep the struct naturally aligned */
	char     fstype[16];    /* "fat", "ubixfs", "swap", "" (best-effort) */
	char     mountpoint[64]; /* mount path if mounted, else "" */
	char     model[40];     /* drive model / bus id (DISK; "" if unknown) */
};

/**
 * Enumerate block devices: fills @buf with up to @max entries (whole disks and
 * their partitions).  Native API syscall 68.
 *
 * @return the number of entries written, or a negative errno.
 */
int ubix_disk_query(struct ubix_disk_info *buf, uint32_t max);

#endif /* _API_UBIX_DISK_H */
