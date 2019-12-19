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

#ifndef _SYS_FCNTL_H_
#define _SYS_FCNTL_H_

#define AT_FDCWD                -100

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

#define O_DIRECTORY     0x00020000      /* Fail if not directory */
#define O_EXEC          0x00040000      /* Open for execute only */

#define FCNTLFLAGS      (FAPPEND|FASYNC|FFSYNC|FNONBLOCK|FPOSIXSHM|O_DIRECT)

/*
 #define FFLAGS(oflags)  ((oflags) + 1)
 #define OFLAGS(fflags)  ((fflags) - 1)
 */

/* convert from open() flags to/from fflags; convert O_RD/WR to FREAD/FWRITE */
#define FFLAGS(oflags)  ((oflags) & O_EXEC ? (oflags) : (oflags) + 1)
#define OFLAGS(fflags)  ((fflags) & O_EXEC ? (fflags) : (fflags) - 1)

#define FEXEC O_EXEC

#define FMASK (FREAD|FWRITE|FAPPEND|FASYNC|FFSYNC|FNONBLOCK|O_DIRECT|FEXEC)

#endif /* !_SYS_FCNTL_H_ */
