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

/*
 * disk_query — the architecture-neutral half of the Disk Utility data layer
 * (native syscall 68).  md_disk_list() (per-arch) finds the physical drives; this
 * file reads each drive's MBR, builds one DTO per whole disk + per partition, and
 * cross-references the mount table for fstype/mountpoint.  See dev/disk.h and
 * docs/design/disk-utility-plan.md.
 */

#include <dev/disk.h>
#include <sys/bus.h>
#include <sys/descrip.h> /* g_device_find — (major,minor) -> ubx_device */
#include <ubixos/vitals.h>
#include <fs/vfs/vfs.h>   /* VFS_TYPE_* */
#include <fs/vfs/mount.h> /* struct vfs_mountPoint */
#include <string.h>

/* MBR layout: 4 16-byte records at 0x1BE, then the 0x55AA signature. */
#define MBR_TABLE_OFF 0x1BE
#define MBR_RECORD_SZ 16
#define MBR_SIG_OFF 0x1FE

/**
 * Little-endian u32 from an unaligned MBR field.
 */
static u_int32_t le32(const u_int8_t *p)
{
	return ((u_int32_t)p[0]) | ((u_int32_t)p[1] << 8) | ((u_int32_t)p[2] << 16) | ((u_int32_t)p[3] << 24);
}

/**
 * Map a VFS type id to a short filesystem name.
 */
static const char *vfs_name(int vfs_type)
{
	switch (vfs_type)
	{
		case VFS_TYPE_FAT:
			return ("fat");
		case VFS_TYPE_UBIXFS:
			return ("ubixfs");
		case VFS_TYPE_DEVFS:
			return ("devfs");
		case VFS_TYPE_PROCFS:
			return ("procfs");
		default:
			return ("");
	}
}

/**
 * Fallback filesystem name from the MBR partition type byte (when not mounted).
 */
static const char *type_name(u_int8_t t)
{
	if (t == 0x0C || t == 0x0B || t == 0x06 || t == 0x01)
		return ("fat");
	if (t == 0x82)
		return ("swap");
	if (t == 0x9C)
		return ("ubixfs");
	return ("");
}

/**
 * If the device at (major,minor) is mounted, copy its mount path + fstype into
 * the entry and flag it mounted.
 */
static void annotate_mount(struct ubix_disk_info *e, int major, int minor)
{
	struct ubx_device *dev = g_device_find ? (struct ubx_device *)g_device_find(major, minor) : 0;
	struct vfs_mountPoint *mp;

	if (dev == 0 || systemVitals == 0)
		return;
	for (mp = systemVitals->mountPoints; mp != 0; mp = mp->next)
	{
		if (mp->device == dev)
		{
			e->flags |= UBIX_DF_MOUNTED;
			snprintf(e->mountpoint, sizeof(e->mountpoint), "%s", mp->mountPoint);
			if (mp->fs != 0)
				snprintf(e->fstype, sizeof(e->fstype), "%s", vfs_name(mp->fs->vfsType));
			return;
		}
	}
}

int disk_query(void *buf, int max)
{
	struct ubix_disk_info *out = (struct ubix_disk_info *)buf;
	struct disk_hw disks[DISK_HW_MAX];
	u_int8_t sector[512];
	int nd, di, n = 0;

	if (out == 0 || max <= 0)
		return (0);

	nd = md_disk_list(disks, DISK_HW_MAX);
	for (di = 0; di < nd && n < max; di++)
	{
		struct disk_hw *hw = &disks[di];
		int disk_idx = n;
		int i;

		/* Whole-disk entry. */
		memset(&out[n], 0, sizeof(out[n]));
		snprintf(out[n].name, sizeof(out[n].name), "%s", hw->name);
		out[n].kind = UBIX_DK_DISK;
		out[n].parent = (u_int8_t)disk_idx;
		out[n].major = 1;
		out[n].minor = 0;
		out[n].sector_size = 512;
		out[n].sectors = hw->sectors;
		snprintf(out[n].model, sizeof(out[n].model), "%s", hw->model);
		n++;

		/* Read + parse the MBR; skip silently if absent/unreadable. */
		if (hw->dev == 0 || hw->dev->dev_blk_ops == 0 || hw->dev->dev_blk_ops->read == 0)
			continue;
		if (hw->dev->dev_blk_ops->read(hw->dev, 0, 1, sector) != 0)
			continue;
		if (sector[MBR_SIG_OFF] != 0x55 || sector[MBR_SIG_OFF + 1] != 0xAA)
			continue;

		for (i = 0; i < 4 && n < max; i++)
		{
			const u_int8_t *rec = sector + MBR_TABLE_OFF + (i * MBR_RECORD_SZ);
			u_int8_t flag = rec[0];
			u_int8_t type = rec[4];

			/* Validate the boot flag so a FAT BPB (also 0x55AA) is not misread. */
			if (flag != 0x00 && flag != 0x80)
				break;
			if (type == 0x00)
				continue;

			memset(&out[n], 0, sizeof(out[n]));
			snprintf(out[n].name, sizeof(out[n].name), "%ss%i", hw->name, i + 1);
			out[n].kind = UBIX_DK_PART;
			out[n].parent = (u_int8_t)disk_idx;
			out[n].major = 1;
			out[n].minor = (u_int16_t)(i + 1);
			out[n].mbr_type = type;
			out[n].lba_start = le32(rec + 8);
			out[n].sectors = le32(rec + 12);
			out[n].sector_size = 512;
			snprintf(out[n].fstype, sizeof(out[n].fstype), "%s", type_name(type));
			annotate_mount(&out[n], out[n].major, out[n].minor);
			n++;
		}
	}

	return (n);
}
