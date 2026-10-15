/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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

#ifndef _DEV_PARTITION_H
#define _DEV_PARTITION_H

#include <sys/types.h>
#include <sys/bus.h>

/*
 * Arch-neutral MBR (DOS) partition support.  Given a whole-disk block device
 * (any driver that has registered ubx_blk_ops — IDE, virtio-blk, …), parse the
 * legacy MBR at LBA 0 and produce one "partition device" per non-empty slot.
 *
 * Each partition device is itself a struct ubx_device whose block ops transpose
 * an LBA into the parent disk by adding the partition's start LBA — so the
 * buffer cache and the filesystem drivers read a partition exactly as they read
 * a whole disk, with zero awareness of the offset.  No dependency on the full
 * newbus core (sys/sys/bus.c): the shim only touches the shared ubx_device /
 * ubx_blk_ops structs, so it links on every architecture.
 */

#define MBR_MAX_PARTITIONS 4     /* 4 primary slots in a classic MBR */
#define UBP_MAX_PARTITIONS 8     /* partition devices kept per disk (MBR or GPT) */
#define MBR_TYPE_PROTECTIVE 0xEE /* GPT protective MBR entry */

/* MBR partition type bytes uBixOS cares about (see tools/mkimage*.sh). */
#define MBR_TYPE_EMPTY 0x00
#define MBR_TYPE_FAT32 0x0C  /* LBA FAT32 — the /boot partition */
#define MBR_TYPE_SWAP 0x82   /* Linux swap — uBixOS swap device */
#define MBR_TYPE_UBPOOL 0x9C /* UbixFS pool — the native CoW root */
#define MBR_TYPE_OTHER 0xDA  /* GPT partition of a type uBixOS does not know */

/*
 * One MBR partition exposed as a block device.  `dev` MUST be the first member:
 * the offsetting block ops recover the enclosing ubp_partition by casting the
 * ubx_device* back to ubp_partition*.
 */
struct ubp_partition
{
	struct ubx_device dev;     /* the partition's block device (first member!) */
	struct ubx_blk_ops ops;    /* offsetting read/write */
	struct ubx_device *parent; /* the whole-disk device */
	u_int64_t lba_start;       /* partition's first LBA on the parent */
	u_int64_t lba_count;       /* partition length in sectors */
	int minor;                 /* device minor (= table slot + 1, i.e. sNN) */
	u_int8_t type;             /* MBR type byte (GPT types are mapped onto it) */
	u_int8_t gpt_type[16];     /* GPT partition-type GUID (raw bytes); zero for MBR */
};

/**
 * Parse the MBR on @disk and populate @out with one entry per non-empty primary
 * partition.  Partition N (MBR slot N, 0-based) is assigned minor N+1 to match
 * the i386 `ad0sN` convention, so the pool in slot 3 resolves at minor 3.
 *
 * @param disk  whole-disk block device (must have dev_blk_ops).
 * @param out   caller-provided array of at least @max entries.
 * @param max   capacity of @out (use MBR_MAX_PARTITIONS).
 * @return the number of partitions populated, 0 if the disk has no valid MBR
 *         (no 0x55AA signature), or -1 on a read error.
 */
int mbr_parse_partitions(struct ubx_device *disk, struct ubp_partition *out, int max);

/**
 * As mbr_parse_partitions(), but also recognises a GPT (protective MBR at LBA 0
 * plus a valid GPT header) and knows the disk capacity so the backup GPT header
 * can be used when the primary is damaged.  GPT type GUIDs are mapped onto the
 * MBR_TYPE_* bytes (EFI System -> FAT32, uBix swap -> SWAP, uBix pool -> UBPOOL)
 * so type-based lookups keep working; the raw GUID is kept in gpt_type.
 *
 * @param disk_sectors  total sectors of @disk, 0 if unknown (primary GPT only).
 * @param max           capacity of @out (use UBP_MAX_PARTITIONS).
 * @return as mbr_parse_partitions(); a GPT disk with no valid GPT returns 0.
 */
int ubp_parse_partitions(struct ubx_device *disk, u_int64_t disk_sectors, struct ubp_partition *out, int max);

#endif /* _DEV_PARTITION_H */
