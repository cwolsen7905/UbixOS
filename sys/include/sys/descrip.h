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

#ifndef _KERN_DESCRIP_H
#define _KERN_DESCRIP_H

#include <sys/thread.h>
#include <sys/sysproto_posix.h>

#include <vfs/file.h>
#include <vfs/stat.h>

#include <sys/fcntl.h>

/* Limits */
//#define MAX_FILES 256
#define MAX_FILES 256

typedef __mode_t mode_t;
typedef __nlink_t nlink_t;

struct fileOps;
struct file;
struct uio;
//TMP
struct ucred;
//TMP

/* Function Protos */
typedef int fo_rdwr_t(struct file *fp, struct uio *uio, struct ucred *active_cred, int flags, struct thread *td);
typedef int fo_stat_t(struct file *fp, struct stat *sb, struct ucred *active_cred, struct thread *td);
typedef int fo_close_t(struct file *fp, struct thread *td);

struct ucred {
    char pad;
};

struct uio {
    char pad;
};

struct file {
    uint32_t f_flag;
    uint16_t f_type;
    struct fileOps *f_ops;
    fileDescriptor_t *fd;
    int fd_type;
    int socket;
    void *data;
};

struct fileOps {
    fo_rdwr_t *read;
    fo_rdwr_t *write;
    fo_stat_t *stat;
    fo_close_t *close;
};

#ifdef _BALLS
struct stat {
  __dev_t st_dev; /* inode's device */
  ino_t st_ino; /* inode's number */
  mode_t st_mode; /* inode protection mode */
  nlink_t st_nlink; /* number of hard links */
  uid_t st_uid; /* user ID of the file's owner */
  gid_t st_gid; /* group ID of the file's group */
  __dev_t st_rdev; /* device type */
#if __BSD_VISIBLE
  struct timespec st_atimespec; /* time of last access */
  struct timespec st_mtimespec; /* time of last data modification */
  struct timespec st_ctimespec; /* time of last file status change */
#else
  time_t st_atime; /* time of last access */
  long __st_atimensec; /* nsec of last access */
  time_t st_mtime; /* time of last data modification */
  long __st_mtimensec; /* nsec of last data modification */
  time_t st_ctime; /* time of last file status change */
  long __st_ctimensec; /* nsec of last file status change */
#endif
  off_t st_size; /* file size, in bytes */
  blkcnt_t st_blocks; /* blocks allocated for file */
  blksize_t st_blksize; /* optimal blocksize for I/O */
  fflags_t st_flags; /* user defined flags for file */
  __uint32_t st_gen; /* file generation number */
  __int32_t st_lspare;
#if __BSD_VISIBLE
  struct timespec st_birthtimespec; /* time of file creation */
  /*
   * Explicitly pad st_birthtimespec to 16 bytes so that the size of
   * struct stat is backwards compatible.  We use bitfields instead
   * of an array of chars so that this doesn't require a C99 compiler
   * to compile if the size of the padding is 0.  We use 2 bitfields
   * to cover up to 64 bits on 32-bit machines.  We assume that
   * CHAR_BIT is 8...
   */
  unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec));
  unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec));
#else
  time_t st_birthtime; /* time of file creation */
  long st_birthtimensec; /* nsec of file creation */
  unsigned int :(8 / 2) * (16 - (int)sizeof(struct __timespec));
  unsigned int :(8 / 2) * (16 - (int)sizeof(struct __timespec));
#endif
};
#endif

int fcntl(struct thread*, struct sys_fcntl_args*);
int close(struct thread *, struct close_args *);
int falloc(struct thread *, struct file **, int *);
int getdtablesize(struct thread *, struct getdtablesize_args *);
int fstat(struct thread *, struct sys_fstat_args *);
int ioctl(struct thread *, struct ioctl_args *);
int getfd(struct thread *td, struct file **fp, int fd);

int_kern_openat(struct thread *, int, char *, int);

#endif

/***
 END
 ***/
