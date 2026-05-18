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

/* Little-endian reads from an unaligned byte buffer */
static uint16_t
le16(const uint8_t *p)
{
	return ((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t
le32(const uint8_t *p)
{
	return ((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
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
	uint8_t		 buf[512];
	uint16_t	 bytes_per_sec, rsvd_sec, root_ent_cnt;
	uint16_t	 fat_sz16, tot_sec16;
	uint32_t	 fat_sz32, tot_sec32, fat_sz;
	uint32_t	 tot_sec, root_dir_sectors, data_sec;
	uint8_t		 num_fats, sec_per_clus;

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

	if (bytes_per_sec == 0 || sec_per_clus == 0 || num_fats == 0) {
		kprintf("fat_bpb: invalid BPB fields\n");
		return (-1);
	}

	fat_sz	= (fat_sz16 != 0) ? fat_sz16 : fat_sz32;
	tot_sec	= (tot_sec16 != 0) ? tot_sec16 : tot_sec32;

	/*
	 * Sector counts for each region.  All values are partition-relative;
	 * the block driver adds parOffset transparently.
	 */
	root_dir_sectors = ((uint32_t)root_ent_cnt * 32 + bytes_per_sec - 1) /
	    bytes_per_sec;

	fs->bytes_per_sector	= bytes_per_sec;
	fs->sectors_per_cluster	= sec_per_clus;
	fs->fat_begin_lba	= rsvd_sec;
	fs->fat_sectors		= fat_sz;
	fs->fat_copies		= num_fats;
	fs->root_lba		= rsvd_sec + (uint32_t)num_fats * fat_sz;
	fs->root_sectors	= root_dir_sectors;
	fs->data_lba		= fs->root_lba + root_dir_sectors;

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

	kprintf("fat_bpb: FAT%d  clusters=%u  data_lba=%u\n",
	    fs->type, fs->total_clusters, fs->data_lba);

	return (0);
}
