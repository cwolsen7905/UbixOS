/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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
#ifndef _FS_MSDOS_FS_H
#define _FS_MSDOS_FS_H

/*
 * MS-DOS file system in-core superblock data
 */

struct msdos_sb_info {
    unsigned short cluster_size; /* sectors/cluster */
    unsigned char fats, fat_bits; /* number of FATs, FAT bits (12 or 16) */
    unsigned short fat_start, fat_length; /* FAT start & length (sec.) */
    unsigned short dir_start, dir_entries; /* root dir start & entries */
    unsigned short data_start; /* first data sector */
    unsigned long clusters; /* number of clusters */
    uid_t fs_uid;
    gid_t fs_gid;
    int quiet; /* fake successful chmods and chowns */
    unsigned short fs_umask;
    unsigned char name_check; /* r = relaxed, n = normal, s = strict */
    unsigned char conversion; /* b = binary, t = text, a = auto */
    struct wait_queue *fat_wait;
    int fat_lock;
    int prev_free; /* previously returned free cluster number */
    int free_clusters; /* -1 if undefined */
};

/*
 * MS-DOS file system inode data in memory
 */

struct msdos_inode_info {
    int i_start; /* first cluster or 0 */
    int i_attrs; /* unused attribute bits */
    int i_busy; /* file is either deleted but still open, or
     inconsistent (mkdir) */
    struct inode *i_depend; /* pointer to inode that depends on the
     current inode */
    struct inode *i_old; /* pointer to the old inode this inode
     depends on */
    int i_binary; /* file contains non-text data */
};

#endif
