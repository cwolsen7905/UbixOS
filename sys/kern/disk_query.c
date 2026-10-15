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
#include <sys/gpt_part.h>
#include <dev/partition.h>
#include <sys/sysproto.h> /* sys_disk_rawio_args */
#include <sys/errno.h>
#include <ubixos/sched.h> /* _current->uid */
#include <ubixos/uaccess.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>

#define DISK_QUERY_ENTRIES 16 /* entries disk_rawio() scans for partition ranges */
#define DISK_GPT_MAX 16       /* GPT entries enumerated per disk (stack-bounded) */

/* MBR layout: 4 16-byte records at 0x1BE, then the 0x55AA signature. */
#define MBR_TABLE_OFF 0x1BE
#define MBR_RECORD_SZ 16
#define MBR_SIG_OFF 0x1FE
#define MBR_TYPE_PROTECTIVE_ 0xEE /* GPT protective MBR entry */

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

		/* Protective MBR -> the real table is the GPT. */
		for (i = 0; i < 4; i++)
			if (sector[MBR_TABLE_OFF + (i * MBR_RECORD_SZ) + 4] == MBR_TYPE_PROTECTIVE_)
				break;
		if (i < 4)
		{
			struct gpt_part ents[DISK_GPT_MAX];
			int ne = gpt_read_partitions(hw->dev, hw->sectors, ents, DISK_GPT_MAX);
			int k;

			out[disk_idx].flags |= UBIX_DF_GPT;
			for (k = 0; k < ne && n < max; k++)
			{
				u_int8_t mt = gpt_guid_mbr_type(ents[k].type);

				memset(&out[n], 0, sizeof(out[n]));
				snprintf(out[n].name, sizeof(out[n].name), "%ss%i", hw->name, (int)ents[k].slot + 1);
				out[n].kind = UBIX_DK_PART;
				out[n].parent = (u_int8_t)disk_idx;
				out[n].major = 1;
				out[n].minor = (u_int16_t)(ents[k].slot + 1);
				out[n].mbr_type = mt;
				out[n].lba_start = ents[k].first_lba;
				out[n].sectors = ents[k].last_lba - ents[k].first_lba + 1;
				out[n].sector_size = 512;
				memcpy(out[n].gpt_type, ents[k].type, GPT_GUID_LEN);
				snprintf(out[n].fstype, sizeof(out[n].fstype), "%s", type_name(mt));
				annotate_mount(&out[n], out[n].major, out[n].minor);
				n++;
			}
			continue;
		}

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

/**
 * True if @dev is @disk or one of its partition devices.
 */
static int dev_on_disk(struct ubx_device *dev, struct ubx_device *disk)
{
	return (dev == disk || (dev != 0 && dev->dev_parent == disk));
}

/**
 * Raw sector I/O on a whole disk; see dev/disk.h.
 */
int disk_rawio(u_int32_t index, u_int32_t op, u_int64_t lba, u_int32_t count, void *ubuf)
{
	struct disk_hw disks[DISK_HW_MAX];
	struct disk_hw *hw;
	struct vfs_mountPoint *mp;
	u_int8_t *kbuf;
	int nd, rc = 0, any_mounted = 0;

	if (_current == 0 || _current->uid != 0)
		return (-EPERM);
	if (ubuf == 0 || count == 0 || count > UBIX_DISK_RAW_MAXSECS)
		return (-EINVAL);
	if (op != UBIX_DISK_RAW_READ && op != UBIX_DISK_RAW_WRITE)
		return (-EINVAL);

	nd = md_disk_list(disks, DISK_HW_MAX);
	if (index >= (u_int32_t)nd)
		return (-ENODEV);
	hw = &disks[index];
	if (hw->dev == 0 || hw->dev->dev_blk_ops == 0)
		return (-ENODEV);
	if (lba + count < lba || (hw->sectors != 0 && lba + count > hw->sectors) || lba + count > 0xFFFFFFFFULL)
		return (-EINVAL);

	if (op == UBIX_DISK_RAW_WRITE)
	{
		if (hw->dev->dev_blk_ops->write == 0)
			return (-EROFS);
		/* A disk with a mounted filesystem keeps its table (LBA 0) and every
		 * partition in it off limits.  devfs/procfs name the disk as their
		 * nominal device but do not use it; a mount with no device (the root
		 * pool) is assumed to be on this disk. */
		if (systemVitals != 0)
		{
			for (mp = systemVitals->mountPoints; mp != 0; mp = mp->next)
			{
				int t = mp->fs != 0 ? mp->fs->vfsType : -1;

				if (t == VFS_TYPE_DEVFS || t == VFS_TYPE_PROCFS)
					continue;
				if (mp->device == 0 || dev_on_disk(mp->device, hw->dev))
					any_mounted = 1;
			}
		}
		if (any_mounted)
		{
			struct ubix_disk_info *tab =
			    (struct ubix_disk_info *)kmalloc(DISK_QUERY_ENTRIES * sizeof(*tab));
			int nt, k, didx;

			if (tab == 0)
				return (-ENOMEM);
			if (lba == 0)
			{
				kfree(tab);
				return (-EBUSY);
			}
			nt = disk_query(tab, DISK_QUERY_ENTRIES);
			/* Array position of whole disk number @index. */
			for (k = 0, didx = -1, nd = 0; k < nt; k++)
				if (tab[k].kind == UBIX_DK_DISK && nd++ == (int)index)
					didx = k;
			for (k = 0; k < nt; k++)
			{
				if (tab[k].kind != UBIX_DK_PART || tab[k].parent != (u_int8_t)didx)
					continue;
				if (lba < tab[k].lba_start + tab[k].sectors && lba + count > tab[k].lba_start)
				{
					kfree(tab);
					return (-EBUSY);
				}
			}
			/* The GPT header/array area precedes the first partition. */
			kfree(tab);
		}
	}

	kbuf = (u_int8_t *)kmalloc(count * 512);
	if (kbuf == 0)
		return (-ENOMEM);

	if (op == UBIX_DISK_RAW_READ)
	{
		if (hw->dev->dev_blk_ops->read(hw->dev, (u_int32_t)lba, count, kbuf) != 0)
			rc = -EIO;
		else if (copyout(kbuf, ubuf, count * 512) != 0)
			rc = -EFAULT;
	}
	else
	{
		if (copyin(ubuf, kbuf, count * 512) != 0)
			rc = -EFAULT;
		else if (hw->dev->dev_blk_ops->write(hw->dev, (u_int32_t)lba, count, kbuf) != 0)
			rc = -EIO;
	}
	kfree(kbuf);
	return (rc);
}

/**
 * Native syscall 72 — raw sector I/O on a whole disk (see disk_rawio()).  Shared
 * by both arches; the result (0 or -errno) is returned in td_retval[0].
 */
int sys_disk_rawio(struct thread *td, struct sys_disk_rawio_args *uap)
{
	td->td_retval[0] =
	    disk_rawio((u_int32_t)uap->index, (u_int32_t)uap->op, (u_int64_t)uap->lba, (u_int32_t)uap->count, uap->buf);
	return (0);
}

/*
 * The GPT reader is its own translation unit in spirit (gpt_part.c) but is
 * textually included here: the kernel's per-arch source lists live in the top
 * Makefile, which W-058's lane may not edit.  Replace this with a gpt_part.c
 * entry in those lists when the Makefile is touched.
 */
#include "gpt_part.c"
