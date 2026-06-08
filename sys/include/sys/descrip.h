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

#ifndef _KERN_DESCRIP_H
#define _KERN_DESCRIP_H

#include <sys/thread.h>
#include <sys/sysproto_posix.h>

#include <fs/vfs/file.h>
#include <fs/vfs/stat.h>

#include <sys/fcntl.h>

/* Limits */
//#define MAX_FILES 256
#define MAX_FILES 256

typedef __nlink_t nlink_t;

struct fileOps;
struct file;
struct uio;
//TMP
struct ucred;
//TMP

/*
 * fileops vector — per-fd-type read/write/close handlers (path B of the
 * cross-arch file-syscall plan).  Each handler follows the kernel syscall
 * convention: it sets td->td_retval[0] (bytes transferred, etc.) and returns 0
 * on success or a positive errno (e.g. EINTR).  Dispatching sys_read/write/close
 * through these lets the generic syscall core stop referencing the TTY/socket/
 * pipe subsystems directly — each fd-type module registers its own ops, so an
 * arch that lacks (say) lwIP or the TTY layer simply never links those ops.
 */
typedef int fo_read_t(struct file *fp, struct thread *td, void *buf, size_t nbyte);
typedef int fo_write_t(struct file *fp, struct thread *td, const void *buf, size_t nbyte);
typedef int fo_close_t(struct file *fp, struct thread *td, int fd); /* fd = the descriptor number, for fdestroy */

struct ucred {
    char pad;
};

struct uio {
    char pad;
};

/* fd_type values for struct file */
#define FD_TYPE_FILE   1   /* regular VFS file */
#define FD_TYPE_SOCKET 2   /* lwIP socket */
#define FD_TYPE_PIPE   3   /* pipe (struct pipeInfo in fd->data) */
#define FD_TYPE_DIR    4   /* open directory (kDIR_t in fd->data) */
#define FD_TYPE_TTY    5   /* opened via /dev/tty — uses process ct_tty */
#define FD_TYPE_TTYV   6   /* opened via /dev/ttyX — specific tty_term in fd->data */

/* Pipe direction for FD_TYPE_PIPE struct files.  Stored per-file rather
 * than per-pipeInfo because dup2/fork/dup may give a single pipeInfo
 * multiple struct file aliases — each alias still represents exactly one
 * read end or write end and needs to decrement the matching refcount on
 * close.  Zero means "not a pipe". */
#define PIPE_END_READ   1
#define PIPE_END_WRITE  2

struct file {
    u_int32_t f_flag;
    u_int16_t f_type;
    struct fileOps *f_ops;
    fileDescriptor_t *fd;
    int fd_type;
    int socket;
    void *data;
    int pipe_end;
};

struct fileOps {
    fo_read_t *read;
    fo_write_t *write;
    fo_close_t *close;
};

/* Console/TTY fileops, installed by tty_init() (sys/posix/tty.c).  NULL on
 * architectures without a TTY layer; the generic syscall core registers it on
 * tty fds and uses it for the controlling-terminal placeholder fall-through. */
extern struct fileOps *g_console_ops;

/* tty_find hook (returns a tty_term* for a slot index; void* to avoid coupling
 * the fd layer to the TTY types).  Installed by tty_init(); NULL on arches
 * without a TTY layer, where /dev/console and /dev/ttyvN opens return ENODEV. */
extern void *(*g_tty_find)(u_int16_t slot);

/* sys_ioctl hooks: g_tty_inject pushes a byte into the tty line discipline
 * (TIOCSTI); g_dev_char_ioctl routes a char-device ioctl to its driver
 * (returns the driver result, or -1 if the fd is not a char device / unhandled).
 * Installed by the tty + devfs layers; NULL where those aren't linked. */
extern void (*g_tty_inject)(void *term, char ch);
extern void (*g_tty_signal)(void *term, int sig);
extern int (*g_dev_char_ioctl)(struct file *fp, u_int32_t com, void *data);

/* sys_select/sys_poll hooks: g_socket_select is lwIP's select (installed by the
 * socket layer); g_console_stdin_ready drains the controlling terminal and
 * returns non-zero when a line is ready (installed by tty_init).  NULL where
 * those layers aren't linked. */
struct fd_set;
struct timeval;
extern int (*g_socket_select)(int nfds, struct fd_set *r, struct fd_set *w, struct fd_set *e, struct timeval *tv);
extern int (*g_console_stdin_ready)(void);

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
int fdestroy(struct thread *td, struct file *fp, int fd);

int kern_openat(struct thread *, int, char *, int, int);

#endif

/***
 END
 ***/
