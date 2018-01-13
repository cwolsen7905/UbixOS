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

#ifndef _VFS_STAT_H
#define _VFS_STAT_H

#include <sys/types.h>

#define STAT_LSTAT 0x00000001

#define STAT_NO_FOLLOW 0x00000000
#define STAT_FOLLOW    0x00000001

struct __timespec {
    __time_t tv_sec; /* seconds */
    long tv_nsec; /* and nanoseconds */
};

struct stat {
    __dev_t st_dev; /* inode's device */
    ino_t st_ino; /* inode's number */
    __mode_t st_mode; /* inode protection mode */
    __nlink_t st_nlink; /* number of hard links */
    uid_t st_uid; /* user ID of the file's owner */
    gid_t st_gid; /* group ID of the file's group */
    __dev_t st_rdev; /* device type */
    time_t st_atime; /* time of last access */
    long __st_atimensec; /* nsec of last access */
    time_t st_mtime; /* time of last data modification */
    long __st_mtimensec; /* nsec of last data modification */
    time_t st_ctime; /* time of last file status change */
    long __st_ctimensec; /* nsec of last file status change */
    off_t st_size; /* file size, in bytes */
    blkcnt_t st_blocks; /* blocks allocated for file */
    blksize_t st_blksize; /* optimal blocksize for I/O */
    fflags_t st_flags; /* user defined flags for file */
    __uint32_t st_gen; /* file generation number */
    __int32_t st_lspare;
    time_t st_birthtime; /* time of file creation */
    long st_birthtimensec; /* nsec of file creation */
    unsigned int :(8 / 2) * (16 - (int) sizeof(struct __timespec));
    unsigned int :(8 / 2) * (16 - (int) sizeof(struct __timespec));
};

#endif
