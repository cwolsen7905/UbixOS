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

#include <dev/partition.h>
#include <lib/kprintf.h>
#include <string.h>

/* Classic MBR layout: 4 × 16-byte partition records at offset 0x1BE, then the
 * 0x55 0xAA boot signature.  Each record's bytes we use: type at +4, the LBA
 * start (little-endian u32) at +8, the sector count at +12. */
#define MBR_PART_TABLE_OFF 0x1BE
#define MBR_PART_RECORD_SZ 16
#define MBR_SIG_OFF 0x1FE

/**
 * Read a little-endian u32 from a byte buffer (MBR fields are unaligned).
 */
static u_int32_t le32(const u_int8_t *p)
{
	return ((u_int32_t)p[0]) | ((u_int32_t)p[1] << 8) | ((u_int32_t)p[2] << 16) | ((u_int32_t)p[3] << 24);
}

/**
 * Partition block read: transpose the partition-relative LBA onto the parent
 * disk and delegate.  The enclosing ubp_partition is recovered by casting the
 * ubx_device* (the first member of ubp_partition) back to ubp_partition*.
 */
static int part_read(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf)
{
	struct ubp_partition *p = (struct ubp_partition *)dev;

	return (p->parent->dev_blk_ops->read(p->parent, p->lba_start + lba, count, buf));
}

/**
 * Partition block write: the write-side twin of part_read.
 */
static int part_write(struct ubx_device *dev, u_int32_t lba, u_int32_t count, void *buf)
{
	struct ubp_partition *p = (struct ubp_partition *)dev;

	return (p->parent->dev_blk_ops->write(p->parent, p->lba_start + lba, count, buf));
}

int mbr_parse_partitions(struct ubx_device *disk, struct ubp_partition *out, int max)
{
	u_int8_t sector[512];
	int i, n;

	if (disk == 0 || disk->dev_blk_ops == 0 || disk->dev_blk_ops->read == 0 || out == 0 || max <= 0)
		return (-1);

	if (disk->dev_blk_ops->read(disk, 0, 1, sector) != 0)
		return (-1);

	/* No 0x55AA signature -> not a partitioned disk. */
	if (sector[MBR_SIG_OFF] != 0x55 || sector[MBR_SIG_OFF + 1] != 0xAA)
		return (0);

	/* A FAT boot sector (BPB) also ends in 0x55AA, so the signature alone is not
	 * enough: distinguish a real MBR by its 4 boot-flag bytes, which must each be
	 * 0x00 (inactive) or 0x80 (active).  A BPB has arbitrary code/data there and
	 * almost never satisfies this — so we treat such a disk as unpartitioned (the
	 * bare-FAT whole-disk path) rather than fabricate garbage partitions. */
	for (i = 0; i < MBR_MAX_PARTITIONS; i++)
	{
		u_int8_t flag = sector[MBR_PART_TABLE_OFF + (i * MBR_PART_RECORD_SZ)];
		if (flag != 0x00 && flag != 0x80)
			return (0);
	}

	n = 0;
	for (i = 0; i < MBR_MAX_PARTITIONS && i < max; i++)
	{
		const u_int8_t *rec = sector + MBR_PART_TABLE_OFF + (i * MBR_PART_RECORD_SZ);
		u_int8_t type = rec[4];
		struct ubp_partition *p;

		if (type == MBR_TYPE_EMPTY)
			continue;

		p = &out[n];
		memset(p, 0, sizeof(*p));
		p->parent = disk;
		p->lba_start = le32(rec + 8);
		p->lba_count = le32(rec + 12);
		p->type = type;
		p->minor = i + 1; /* MBR slot i -> sNN (1-based), matching i386 ad0sN */
		p->ops.read = part_read;
		p->ops.write = part_write;
		p->dev.dev_parent = disk;
		p->dev.dev_blk_ops = &p->ops;
		p->dev.dev_softc = p; /* self-reference; handy for callers */
		snprintf(p->dev.dev_nameunit, sizeof(p->dev.dev_nameunit), "%ss%i", disk->dev_nameunit, i + 1);

		kprintf("partition: %s type=0x%02X start=%u sectors=%u (minor %i)\n",
		        p->dev.dev_nameunit,
		        type,
		        p->lba_start,
		        p->lba_count,
		        p->minor);
		n++;
	}

	return (n);
}
