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

#ifndef _FAT_INTERNAL_H
#define _FAT_INTERNAL_H

#include <sys/types.h>
#include <fs/vfs/mount.h>

#define FAT_TYPE_12	12
#define FAT_TYPE_16	16
#define FAT_TYPE_32	32

/* Cluster sentinel values (FAT32 canonical form; mask to 28 bits for FAT32) */
#define FAT_CLUSTER_FREE	0x00000000u
#define FAT_CLUSTER_BAD		0x0FFFFFF7u
#define FAT_CLUSTER_EOC		0x0FFFFFFFu	/* >= this = end of chain */

/* File open modes */
#define FAT_MODE_R	0x01
#define FAT_MODE_W	0x02
#define FAT_MODE_A	0x04

/*
 * Mounted filesystem instance — one per mount point.
 * Populated by fat_bpb_parse(); all other modules treat it as read-only after
 * mount time except free_cluster_hint.
 */
struct fat_fs {
	struct vfs_mountPoint	*mp;
	uint8_t			 type;			/* FAT_TYPE_12/16/32 */
	uint32_t		 bytes_per_sector;	/* always 512 on UbixOS */
	uint8_t			 sectors_per_cluster;
	uint32_t		 fat_begin_lba;		/* first sector of FAT region */
	uint32_t		 fat_sectors;		/* sectors per FAT copy */
	uint8_t			 fat_copies;
	uint32_t		 root_lba;		/* FAT12/16: fixed root dir start */
	uint32_t		 root_sectors;		/* FAT12/16: size of root region */
	uint32_t		 root_cluster;		/* FAT32: cluster 2 = root */
	uint32_t		 data_lba;		/* first data sector (cluster 2) */
	uint32_t		 total_clusters;
	uint32_t		 fsinfo_lba;		/* FAT32 only */
	uint32_t		 free_cluster_hint;	/* FAT32 FSINFO next-free */
};

/* Open file state — stored in fileDescriptor_t.res */
struct fat_file {
	struct fat_fs	*fs;
	uint32_t	 start_cluster;
	uint32_t	 cur_cluster;
	uint32_t	 file_size;
	uint32_t	 position;
	uint32_t	 dir_sector;		/* sector holding directory entry */
	uint16_t	 dir_offset;		/* byte offset within that sector */
	uint8_t		 mode;			/* FAT_MODE_R / _W / _A */
	uint8_t		 size_dirty;		/* directory entry needs update */
	uint8_t		 buf[512];		/* write-behind sector buffer */
	uint8_t		 buf_dirty;
	uint32_t	 buf_lba;		/* sector currently in buf */
};

/* Directory iterator — stored in kDIR_t.dirHandle */
struct fat_dir_iter {
	struct fat_fs	*fs;
	uint32_t	 cluster;		/* 0 = root for FAT12/16 */
	uint32_t	 sector_in_cluster;
	uint16_t	 entry_offset;		/* within sector, 0-480 step 32 */
	char		 lfn[256];
	uint8_t		 lfn_seq;
	uint8_t		 lfn_checksum;
};

/* Raw 32-byte FAT directory entry (on-disk layout) */
struct fat_raw_dirent {
	uint8_t		 name[11];
	uint8_t		 attr;
	uint8_t		 nt_res;
	uint8_t		 crt_time_tenth;
	uint16_t	 crt_time;
	uint16_t	 crt_date;
	uint16_t	 acc_date;
	uint16_t	 clus_hi;
	uint16_t	 wrt_time;
	uint16_t	 wrt_date;
	uint16_t	 clus_lo;
	uint32_t	 file_size;
} __attribute__((packed));

#endif /* _FAT_INTERNAL_H */
