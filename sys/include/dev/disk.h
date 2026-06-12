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

#ifndef _DEV_DISK_H
#define _DEV_DISK_H

#include <sys/types.h>
#include <sys/bus.h>

/*
 * Kernel-side disk enumeration for sys_disk_query (native syscall 68).
 *
 * struct ubix_disk_info MUST keep the identical byte layout to the userland
 * copy in <api/ubix_disk.h>; the two trees can't share one header (kernel uses
 * -nostdinc), so the layout is duplicated and kept in lockstep — a size check in
 * disk_query.c guards it.  Field order matches the uapi header exactly.
 */

#define UBIX_DK_DISK 0
#define UBIX_DK_PART 1

#define UBIX_DF_REMOVABLE 0x1
#define UBIX_DF_MOUNTED 0x2
#define UBIX_DF_READONLY 0x4

struct ubix_disk_info
{
	char name[32];
	u_int64_t lba_start;
	u_int64_t sectors;
	u_int32_t sector_size;
	u_int32_t flags;
	u_int16_t major;
	u_int16_t minor;
	u_int8_t kind;
	u_int8_t parent;
	u_int8_t mbr_type;
	u_int8_t _pad;
	char fstype[16];
	char mountpoint[64];
	char model[40];
};

/*
 * A whole disk as reported by the per-arch hardware layer.  md_disk_list() is
 * the only architecture-specific piece — it finds the physical drives and their
 * geometry; disk_query() does the (shared) MBR parse + DTO fill on top.
 */
#define DISK_HW_MAX 4

struct disk_hw
{
	struct ubx_device *dev; /* whole-disk block device (has dev_blk_ops) */
	char name[16];          /* "ad0", "vtblk0" */
	u_int64_t sectors;      /* total sectors, 0 if unknown */
	char model[40];         /* bus/model id, "" if unknown */
};

/**
 * Per-architecture: list the whole disks present.  i386 -> sys/pci/hd.c;
 * aarch64 -> sys/arch/aarch64/dev/virtio_blk.c.
 *
 * @return the number of disks written to @out (capped at @max).
 */
int md_disk_list(struct disk_hw *out, int max);

/**
 * Fill @buf with up to @max enumeration entries (whole disks + their MBR
 * partitions).  The worker behind sys_disk_query.
 *
 * @return the number of entries written.
 */
int disk_query(void *buf, int max);

#endif /* _DEV_DISK_H */
