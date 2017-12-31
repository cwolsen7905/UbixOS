/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id: kern_descrip.h 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#ifndef _KERN_DESCRIP_H
#define _KERN_DESCRIP_H

#include <sys/thread.h>
#include <sys/sysproto.h>

#include <vfs/file.h>

typedef  __mode_t        mode_t;
typedef  __nlink_t       nlink_t;

/* command values */
#define F_DUPFD         0               /* duplicate file descriptor */
#define F_GETFD         1               /* get file descriptor flags */
#define F_SETFD         2               /* set file descriptor flags */
#define F_GETFL         3               /* get file status flags */
#define F_SETFL         4               /* set file status flags */
#define F_GETOWN        5               /* get SIGIO/SIGURG proc/pgrp */
#define F_SETOWN        6               /* set SIGIO/SIGURG proc/pgrp */
#define F_GETLK         7               /* get record locking information */
#define F_SETLK         8               /* set record locking information */
#define F_SETLKW        9               /* F_SETLK; wait if blocked */

/* Flag Values */
#define FREAD           0x0001
#define FWRITE          0x0002
#define O_NONBLOCK      0x0004          /* no delay */
#define O_APPEND        0x0008          /* set append mode */
#define O_SHLOCK        0x0010          /* open with shared file lock */
#define O_EXLOCK        0x0020          /* open with exclusive file lock */
#define O_ASYNC         0x0040          /* signal pgrp when data ready */
#define O_FSYNC         0x0080          /* synchronous writes */
#define O_SYNC          0x0080          /* POSIX synonym for O_FSYNC */
#define O_NOFOLLOW      0x0100          /* don't follow symlinks */
#define O_CREAT         0x0200          /* create if nonexistent */
#define O_TRUNC         0x0400          /* truncate to zero length */
#define O_EXCL          0x0800          /* error if already exists */
#define O_DIRECT        0x00010000
#define O_RDONLY        0x0000          /* open for reading only */
#define O_WRONLY        0x0001          /* open for writing only */
#define O_RDWR          0x0002          /* open for reading and writing */
#define O_ACCMODE       0x0003          /* mask for above modes */


#define FHASLOCK        0x4000          /* descriptor holds advisory lock */


/* F MAPPERS */
#define FAPPEND         O_APPEND        /* kernel/compat */
#define FASYNC          O_ASYNC         /* kernel/compat */
#define FFSYNC          O_FSYNC         /* kernel */
#define FNONBLOCK       O_NONBLOCK      /* kernel */
#define FNDELAY         O_NONBLOCK      /* compat */
#define O_NDELAY        O_NONBLOCK      /* compat */
#define FPOSIXSHM       O_NOFOLLOW



#define FCNTLFLAGS      (FAPPEND|FASYNC|FFSYNC|FNONBLOCK|FPOSIXSHM|O_DIRECT)

#define FFLAGS(oflags)  ((oflags) + 1)
#define OFLAGS(fflags)  ((fflags) - 1)

struct file {
  int f_flag;
  char path[1024];
  fileDescriptor *fd;
  };

/* TEMP */
struct __timespec {
        __time_t tv_sec;        /* seconds */
        long    tv_nsec;        /* and nanoseconds */
};

struct stat {
        __dev_t   st_dev;               /* inode's device */
        ino_t     st_ino;               /* inode's number */
        mode_t    st_mode;              /* inode protection mode */
        nlink_t   st_nlink;             /* number of hard links */
        uid_t     st_uid;               /* user ID of the file's owner */
        gid_t     st_gid;               /* group ID of the file's group */
        __dev_t   st_rdev;              /* device type */
#if __BSD_VISIBLE
        struct  timespec st_atimespec;  /* time of last access */
        struct  timespec st_mtimespec;  /* time of last data modification */
        struct  timespec st_ctimespec;  /* time of last file status change */
#else
        time_t    st_atime;             /* time of last access */
        long      __st_atimensec;       /* nsec of last access */
        time_t    st_mtime;             /* time of last data modification */
        long      __st_mtimensec;       /* nsec of last data modification */
        time_t    st_ctime;             /* time of last file status change */
        long      __st_ctimensec;       /* nsec of last file status change */
#endif
        off_t     st_size;              /* file size, in bytes */
        blkcnt_t st_blocks;             /* blocks allocated for file */
        blksize_t st_blksize;           /* optimal blocksize for I/O */
        fflags_t  st_flags;             /* user defined flags for file */
        __uint32_t st_gen;              /* file generation number */
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
        time_t    st_birthtime;         /* time of file creation */
        long      st_birthtimensec;     /* nsec of file creation */
        unsigned int :(8 / 2) * (16 - (int)sizeof(struct __timespec));
        unsigned int :(8 / 2) * (16 - (int)sizeof(struct __timespec));
#endif
};


int fcntl(struct thread *, struct fcntl_args *);
int close(struct thread *,struct close_args *);
int falloc(struct thread *, struct file **, int *);
int getdtablesize(struct thread *, struct getdtablesize_args *);
int fstat(struct thread *, struct fstat_args *);
int ioctl(struct thread *, struct ioctl_args *);
int getfd(struct thread *td,struct file **fp,int fd);


#endif

/***
 END
 ***/
