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

#if defined(__x86_64__)
/*
 * x86-64: the kernel fills the struct musl reads as its `struct kstat`
 * (contrib/musl/arch/x86_64/kstat.h — the Linux x86_64 layout), NOT the FreeBSD
 * layout below.  The FreeBSD layout's 8-byte `long` time fields precede st_size
 * on LP64, pushing st_size to byte 72; musl's x86_64 kstat reads st_size at byte
 * 48, so fstat()/stat() reported a bogus size (e.g. the desktop PNG wallpaper
 * failed to load).  This branch is byte-identical to musl's kstat (144 bytes,
 * st_size @ 48) so every fstat-by-size caller is correct.  i386 (ILP32) and
 * aarch64 keep the FreeBSD layout below — unchanged.  sys_fstat/stat/lstat only
 * write the 11 fields named here; the rest mirror musl's kstat padding.
 */
struct stat {
    __uint64_t st_dev;
    __uint64_t st_ino;
    __uint64_t st_nlink;
    __uint32_t st_mode;
    __uint32_t st_uid;
    __uint32_t st_gid;
    __uint32_t __pad0;
    __uint64_t st_rdev;
    __int64_t st_size;
    __int64_t st_blksize;
    __int64_t st_blocks;
    __int64_t st_atime;
    __int64_t __st_atimensec;
    __int64_t st_mtime;
    __int64_t __st_mtimensec;
    __int64_t st_ctime;
    __int64_t __st_ctimensec;
    __int64_t __kstat_pad[3];
};
#else
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
#endif /* __x86_64__ */

/* Linux statx structure — used by sys_statx (slot 383) */
#define AT_EMPTY_PATH   0x1000
#ifndef AT_FDCWD
#define AT_FDCWD        (-100)
#endif

#define STATX_TYPE      0x00000001U
#define STATX_MODE      0x00000002U
#define STATX_NLINK     0x00000004U
#define STATX_UID       0x00000008U
#define STATX_GID       0x00000010U
#define STATX_ATIME     0x00000020U
#define STATX_MTIME     0x00000040U
#define STATX_CTIME     0x00000080U
#define STATX_INO       0x00000100U
#define STATX_SIZE      0x00000200U
#define STATX_BLOCKS    0x00000400U
#define STATX_BASIC_STATS 0x000007ffU

struct statx_timestamp {
	int64_t  tv_sec;
	u_int32_t tv_nsec;
	int32_t  _pad;
};

struct statx {
	u_int32_t stx_mask;
	u_int32_t stx_blksize;
	u_int64_t stx_attributes;
	u_int32_t stx_nlink;
	u_int32_t stx_uid;
	u_int32_t stx_gid;
	u_int16_t stx_mode;
	u_int16_t __spare0;
	u_int64_t stx_ino;
	u_int64_t stx_size;
	u_int64_t stx_blocks;
	u_int64_t stx_attributes_mask;
	struct statx_timestamp stx_atime;
	struct statx_timestamp stx_btime;
	struct statx_timestamp stx_ctime;
	struct statx_timestamp stx_mtime;
	u_int32_t stx_rdev_major;
	u_int32_t stx_rdev_minor;
	u_int32_t stx_dev_major;
	u_int32_t stx_dev_minor;
	u_int64_t stx_mnt_id;
	u_int64_t stx_dio_mem_align;
	u_int64_t __spare3[12];
};

#endif
