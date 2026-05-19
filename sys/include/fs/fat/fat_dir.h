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

#ifndef _FAT_DIR_H
#define _FAT_DIR_H

#include <fs/fat/fat_internal.h>

/* FAT directory entry attribute flags */
#define FAT_ATTR_RO	0x01
#define FAT_ATTR_HIDDEN	0x02
#define FAT_ATTR_SYS	0x04
#define FAT_ATTR_VOL	0x08
#define FAT_ATTR_DIR	0x10
#define FAT_ATTR_ARCH	0x20
#define FAT_ATTR_LFN	0x0F	/* RO|HIDDEN|SYS|VOL = LFN entry */

/*
 * Initialise iterator for the directory at cluster (0 = FAT12/16 root).
 */
void	fat_dir_iter_open(struct fat_fs *fs, uint32_t cluster,
	    struct fat_dir_iter *it);

/*
 * Advance to the next valid (non-deleted, non-LFN) directory entry.
 * name_out receives the resolved name (LFN if present, else 8.3).
 * entry_out, entry_sector, entry_offset locate the raw SFN entry on disk.
 * Returns 0 on success, -1 at end-of-directory or I/O error.
 */
int	fat_dir_iter_next(struct fat_dir_iter *it, char *name_out,
	    struct fat_raw_dirent *entry_out,
	    uint32_t *entry_sector, uint16_t *entry_offset);

/*
 * Case-insensitive search for name in the directory at dir_cluster.
 * Returns 0 and fills out/entry_sector/entry_offset on success, -1 if not found.
 */
int	fat_dir_find(struct fat_fs *fs, uint32_t dir_cluster, const char *name,
	    struct fat_raw_dirent *out,
	    uint32_t *entry_sector, uint16_t *entry_offset);

/*
 * Write new_size into the file_size field of the directory entry located at
 * entry_sector:entry_offset.
 */
int	fat_dir_update_size(struct fat_fs *fs, uint32_t entry_sector,
	    uint16_t entry_offset, uint32_t new_size);

/*
 * Mark the directory entry at entry_sector:entry_offset as deleted (0xE5).
 */
int	fat_dir_delete_entry(struct fat_fs *fs, uint32_t entry_sector,
	    uint16_t entry_offset);

/*
 * Mark the 8.3 entry at (entry_sec, entry_off) and any preceding LFN entries
 * belonging to it as deleted (0xE5).  dir_cluster is the parent directory.
 */
int	fat_dir_delete_with_lfn(struct fat_fs *fs, uint32_t dir_cluster,
	    uint32_t entry_sec, uint16_t entry_off);

/*
 * Create a new directory entry for name in the directory at dir_cluster.
 * attr is a FAT_ATTR_* combination; start_cluster is the file's first cluster
 * (0 for an empty file).  On success returns 0 and fills entry_sector/offset
 * to point at the SFN entry.
 */
int	fat_dir_create_entry(struct fat_fs *fs, uint32_t dir_cluster,
	    const char *name, uint8_t attr, uint32_t start_cluster,
	    uint32_t *entry_sector, uint16_t *entry_offset);

/*
 * Walk path from the filesystem root and return:
 *   dir_cluster_out  — cluster of the directory containing the final component
 *   file_entry_out   — the final component's raw SFN entry (if found)
 *   entry_sector/offset — location of that SFN entry on disk
 *
 * If the final component does not exist, returns -1 with dir_cluster_out set
 * to the parent directory cluster (useful for create-on-open).
 * Returns 0 if the full path resolves to an existing entry.
 */
int	fat_path_resolve(struct fat_fs *fs, const char *path,
	    uint32_t *dir_cluster_out,
	    struct fat_raw_dirent *file_entry_out,
	    uint32_t *entry_sector, uint16_t *entry_offset);

/*
 * Return the cluster number of the directory at path.
 * Used by fat_opendir (Phase 5 VFS glue).
 */
int	fat_path_to_dir_cluster(struct fat_fs *fs, const char *path,
	    uint32_t *cluster_out);

/* Create directory at path.  Returns 0 on success. */
int	fat_dir_mkdir(struct fat_fs *fs, const char *path);

/* Remove empty directory at path.  Returns 0 on success. */
int	fat_dir_rmdir(struct fat_fs *fs, const char *path);

/* Unlink (delete) the file at path.  Returns 0 on success. */
int	fat_dir_unlink(struct fat_fs *fs, const char *path);

#endif /* _FAT_DIR_H */
