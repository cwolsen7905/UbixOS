/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <sys/bus.h>
#include <lib/kprintf.h>
#include <string.h>
#include <fs/fat/fat_bpb.h>
#include <fs/vfs/mount.h>
#include <sys/types.h>

/* Little-endian reads from an unaligned byte buffer */
static u_int16_t
le16(const u_int8_t *p)
{
	return ((u_int16_t)p[0] | ((u_int16_t)p[1] << 8));
}

static u_int32_t
le32(const u_int8_t *p)
{
	return ((u_int32_t)p[0] | ((u_int32_t)p[1] << 8) |
	    ((u_int32_t)p[2] << 16) | ((u_int32_t)p[3] << 24));
}

/*
 * BPB byte offsets (DOS 3.31 / FAT32 layout)
 */
#define BPB_BytsPerSec		11
#define BPB_SecPerClus		13
#define BPB_RsvdSecCnt		14
#define BPB_NumFATs		16
#define BPB_RootEntCnt		17
#define BPB_TotSec16		19
#define BPB_FATSz16		22
#define BPB_TotSec32		32
/* FAT32 extended BPB */
#define BPB32_FATSz32		36
#define BPB32_RootClus		44
#define BPB32_FSInfo		48
/* Boot sector signature */
#define BPB_SigOffset		510

int
fat_bpb_parse(struct fat_fs *fs)
{
	u_int8_t		 buf[512];
	u_int16_t	 bytes_per_sec, rsvd_sec, root_ent_cnt;
	u_int16_t	 fat_sz16, tot_sec16;
	u_int32_t	 fat_sz32, tot_sec32, fat_sz;
	u_int32_t	 tot_sec, root_dir_sectors, data_sec;
	u_int8_t		 num_fats, sec_per_clus;

	if (fs->mp->device->dev_blk_ops->read(fs->mp->device, 0, 1, buf) != 0) {
		kprintf("fat_bpb: sector 0 read failed\n");
		return (-1);
	}
	if (buf[BPB_SigOffset] != 0x55 || buf[BPB_SigOffset + 1] != 0xAA) {
		kprintf("fat_bpb: bad boot signature %02x %02x\n",
		    buf[BPB_SigOffset], buf[BPB_SigOffset + 1]);
		return (-1);
	}

	bytes_per_sec	= le16(buf + BPB_BytsPerSec);
	sec_per_clus	= buf[BPB_SecPerClus];
	rsvd_sec	= le16(buf + BPB_RsvdSecCnt);
	num_fats	= buf[BPB_NumFATs];
	root_ent_cnt	= le16(buf + BPB_RootEntCnt);
	tot_sec16	= le16(buf + BPB_TotSec16);
	fat_sz16	= le16(buf + BPB_FATSz16);
	tot_sec32	= le32(buf + BPB_TotSec32);
	fat_sz32	= le32(buf + BPB32_FATSz32);

	if (bytes_per_sec != 512 || sec_per_clus == 0 || num_fats == 0) {
		kprintf("fat_bpb: invalid BPB fields\n");
		return (-1);
	}

	fat_sz	= (fat_sz16 != 0) ? fat_sz16 : fat_sz32;
	tot_sec	= (tot_sec16 != 0) ? tot_sec16 : tot_sec32;

	if (fat_sz == 0 || tot_sec == 0) {
		kprintf("fat_bpb: zero fat_sz or tot_sec\n");
		return (-1);
	}

	/*
	 * Sector counts for each region.  All values are partition-relative;
	 * the block driver adds parOffset transparently.
	 */
	root_dir_sectors = ((u_int32_t)root_ent_cnt * 32 + bytes_per_sec - 1) /
	    bytes_per_sec;

	fs->bytes_per_sector	= bytes_per_sec;
	fs->sectors_per_cluster	= sec_per_clus;
	fs->fat_begin_lba	= rsvd_sec;
	fs->fat_sectors		= fat_sz;
	fs->fat_copies		= num_fats;
	fs->root_lba		= rsvd_sec + (u_int32_t)num_fats * fat_sz;
	fs->root_sectors	= root_dir_sectors;
	fs->data_lba		= fs->root_lba + root_dir_sectors;

	if (tot_sec <= fs->data_lba) {
		kprintf("fat_bpb: tot_sec <= data_lba\n");
		return (-1);
	}
	data_sec		= tot_sec - fs->data_lba;
	fs->total_clusters	= data_sec / sec_per_clus;

	/* Determine FAT type by cluster count (Microsoft's canonical method) */
	if (fs->total_clusters < 4085) {
		fs->type = FAT_TYPE_12;
	} else if (fs->total_clusters < 65525) {
		fs->type = FAT_TYPE_16;
	} else {
		fs->type		= FAT_TYPE_32;
		fs->root_cluster	= le32(buf + BPB32_RootClus);
		fs->fsinfo_lba		= le16(buf + BPB32_FSInfo);
		/*
		 * For FAT32 the root dir is in the data region; data_lba is
		 * the same formula but root_dir_sectors == 0 (RootEntCnt == 0).
		 */
	}

	kprintf("fat_bpb: FAT%d  clusters=%u  data_lba=%u  sec/clus=%u\n",
	    fs->type, fs->total_clusters, fs->data_lba,
	    fs->sectors_per_cluster);

	/*
	 * Extract the volume label from the extended BPB and use it (lowercased,
	 * spaces stripped) as the VFS mount point name.  This lets a disk with
	 * label "SYS" mount as "sys:" and a USB drive with label "UBIX" mount
	 * as "ubix:" without any hardcoding in the caller.
	 *
	 * FAT32 extended BPB: BS_VolLab at byte 71 (11 bytes)
	 * FAT12/16 extended BPB: BS_VolLab at byte 43 (11 bytes)
	 */
	{
		u_int8_t vol_off = (fs->type == FAT_TYPE_32) ? 71 : 43;
		char    label[12];
		int     i, last;

		memcpy(label, buf + vol_off, 11);
		label[11] = '\0';

		/* strip trailing spaces */
		for (last = 10; last >= 0 && label[last] == ' '; last--)
			;
		label[last + 1] = '\0';

		/* lowercase */
		for (i = 0; label[i]; i++)
			if (label[i] >= 'A' && label[i] <= 'Z')
				label[i] = (char)(label[i] + ('a' - 'A'));

		if (last >= 0 && label[0] != '\0') {
			/* Store label in fat_fs for informational use (e.g. automountd).
			 * Do NOT overwrite mp->mountPoint — that holds the POSIX path. */
			strncpy(fs->vol_label, label, sizeof(fs->vol_label) - 1);
			fs->vol_label[sizeof(fs->vol_label) - 1] = '\0';
			kprintf("fat_bpb: volume label \"%s\", mounted at %s\n",
			    label, fs->mp->mountPoint);
		}
	}

	return (0);
}

int
fat_read_vol_label(struct ubx_device *dev, char *out, size_t len)
{
	u_int8_t  buf[512];
	u_int32_t fat_sz16, fat_sz32, tot_sec16, tot_sec32, fat_sz, tot_sec;
	u_int32_t data_sec, total_clusters;
	u_int16_t root_ent_cnt, bytes_per_sec;
	u_int8_t  num_fats, sec_per_clus, fat_type;
	u_int8_t  vol_off;
	char     label[12];
	int      i, last;

	if (dev == NULL || out == NULL || len == 0)
		return (-1);

	if (dev->dev_blk_ops->read(dev, 0, 1, buf) != 0)
		return (-1);

	if (buf[BPB_SigOffset] != 0x55 || buf[BPB_SigOffset + 1] != 0xAA)
		return (-1);

	bytes_per_sec = le16(buf + BPB_BytsPerSec);
	sec_per_clus  = buf[BPB_SecPerClus];
	num_fats      = buf[BPB_NumFATs];
	root_ent_cnt  = le16(buf + BPB_RootEntCnt);
	tot_sec16     = le16(buf + BPB_TotSec16);
	fat_sz16      = le16(buf + BPB_FATSz16);
	tot_sec32     = le32(buf + BPB_TotSec32);
	fat_sz32      = le32(buf + BPB32_FATSz32);

	if (bytes_per_sec == 0 || sec_per_clus == 0 || num_fats == 0)
		return (-1);

	fat_sz  = (fat_sz16 != 0) ? fat_sz16 : fat_sz32;
	tot_sec = (tot_sec16 != 0) ? tot_sec16 : tot_sec32;

	if (fat_sz == 0 || tot_sec == 0)
		return (-1);

	{
		u_int32_t rsvd       = le16(buf + BPB_RsvdSecCnt);
		u_int32_t root_secs  = ((u_int32_t)root_ent_cnt * 32 +
		    bytes_per_sec - 1) / bytes_per_sec;
		u_int32_t data_lba   = rsvd + (u_int32_t)num_fats * fat_sz + root_secs;

		if (tot_sec <= data_lba)
			return (-1);
		data_sec      = tot_sec - data_lba;
		total_clusters = data_sec / sec_per_clus;
	}

	if (total_clusters < 4085)
		fat_type = 12;
	else if (total_clusters < 65525)
		fat_type = 16;
	else
		fat_type = 32;

	vol_off = (fat_type == 32) ? 71 : 43;

	memcpy(label, buf + vol_off, 11);
	label[11] = '\0';

	for (last = 10; last >= 0 && label[last] == ' '; last--)
		;
	label[last + 1] = '\0';

	if (last < 0 || label[0] == '\0')
		return (-1);

	for (i = 0; label[i]; i++)
		if (label[i] >= 'A' && label[i] <= 'Z')
			label[i] = (char)(label[i] + ('a' - 'A'));

	strncpy(out, label, len - 1);
	out[len - 1] = '\0';
	return (0);
}
