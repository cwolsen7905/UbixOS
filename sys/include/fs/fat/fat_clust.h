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

#ifndef _FAT_CLUST_H
#define _FAT_CLUST_H

#include <fs/fat/fat_internal.h>

/*
 * Read the FAT entry for cluster n.  Returns 0 on I/O error (cluster 0 is
 * never valid in a chain, so 0 unambiguously signals failure here).
 */
uint32_t	fat_cluster_next(struct fat_fs *fs, uint32_t cluster);

/* Convert cluster number to the absolute LBA of its first sector. */
uint32_t	fat_cluster_to_lba(struct fat_fs *fs, uint32_t cluster);

/*
 * Write value into the FAT entry for cluster.  Handles FAT12/16/32 encoding.
 * Returns 0 on success, -1 on error.
 */
int		fat_cluster_write_entry(struct fat_fs *fs, uint32_t cluster,
		    uint32_t value);

/*
 * Allocate a free cluster.  If after != 0 the new cluster is appended to that
 * chain (after's FAT entry is updated to point here).  Returns the new cluster
 * number, or 0 on failure (disk full or I/O error).
 */
uint32_t	fat_cluster_alloc(struct fat_fs *fs, uint32_t after);

/* Free the entire cluster chain starting at start.  Returns 0 on success. */
int		fat_cluster_free_chain(struct fat_fs *fs, uint32_t start);

#endif /* _FAT_CLUST_H */
