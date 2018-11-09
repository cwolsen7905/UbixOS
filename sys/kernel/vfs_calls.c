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

#include <ubixos/sched.h>
#include <sys/thread.h>
#include <sys/sysproto_posix.h>
#include <sys/descrip.h>
#include <sys/video.h>
#include <string.h>
#include <ufs/ufs.h>

int sys_open(struct thread *td, struct sys_open_args *args) {
  int error = 0x0;
  int fd = 0x0;
  struct file *nfp = 0x0;

  error = falloc(td, &nfp, &fd);

  if (error)
    return (error);

  kprintf("sO: 0x%X:%s", args->mode, args->path);

  nfp->fd = fopen(args->path, "rb");

  if (nfp->fd == 0x0) {
    fdestroy(td, nfp, fd);

    td->td_retval[0] = -1;
    error = -1;
  }
  else {
    td->td_retval[0] = fd;
  }

  return (error);
}

int sys_openat(struct thread *td, struct sys_openat_args *args) {
  int error = 0x0;
  int fd = 0x0;
  struct file *nfp = 0x0;

#ifdef DEBUG_OPENAT
  kprintf("openat");
#endif

  error = falloc(td, &nfp, &fd);

  if (error)
    return (error);

  if ((args->flag & O_WRONLY) == O_WRONLY)
    nfp->fd = fopen(args->path, "w");
  else if ((args->flag & O_RDWR) == O_RDWR)
    nfp->fd = fopen(args->path, "a");
  else
    nfp->fd = fopen(args->path, "r");

  if (nfp->fd == 0x0) {
    fdestroy(td, nfp, fd);

    td->td_retval[0] = -1;
    error = -1;

#ifdef DEBUG_OPENAT
    kprintf("[sOA: 0x%X:%s:%s:%i]", args->flag, args->mode, args->path, td->td_retval[0]);
#endif

    if ((args->flag & O_RDONLY) == O_RDONLY)
      kprintf("O_RDONLY");

    if ((args->flag & O_WRONLY) == O_WRONLY)
      kprintf("O_WRONLY");

    if ((args->flag & O_RDWR) == O_RDWR)
      kprintf("O_RDWR");

    if ((args->flag & O_ACCMODE) == O_ACCMODE)
      kprintf("O_ACCMODE");

  }
  else {
    td->td_retval[0] = fd;
  }

  return (error);
}

int sys_close(struct thread *td, struct sys_close_args *args) {
  struct file *fd = 0x0;
  getfd(td, &fd, args->fd);

  kprintf("[sC:%i:0x%X:0x%X]", args->fd, fd, fd->fd);

  if (fd == 0x0) {
    td->td_retval[0] = -1;
  }
  else {
    if (!fclose(fd->fd))
      td->td_retval[0] = -1;
    else {
      fdestroy(td, fd, args->fd);
      td->td_retval[0] = 0;
    }
  }
  return (0);
}

int sys_read(struct thread *td, struct sys_read_args *args) {
  int x = 0;
  char c = 0x0;
  char bf[2];
  volatile char *buf = args->buf;

  struct file *fd = 0x0;

  size_t nbytes;

  getfd(td, &fd, args->fd);

  if (args->fd > 3) {
    switch (fd->fd_type) {
      case 3: /* XXX - Pipe2 Handling */
        nbytes = (args->nbyte - (1024 - fd->fd->offset) < 0) ? args->nbyte : (1024 - fd->fd->offset);
        kprintf("PR: [%i]", nbytes);
        td->td_retval[0] = nbytes;
        break;
      default:
        td->td_retval[0] = fread(args->buf, args->nbyte, 1, fd->fd);
    }
  }
  else {
    bf[1] = '\0';
    if (_current->term == tty_foreground)
      c = getchar();

    for (x = 0; x < args->nbyte && c != '\n';) {
      if (_current->term == tty_foreground) {

        if (c != 0x0) {
          buf[x++] = c;
          bf[0] = c;
          kprintf(bf);
        }

        if (c == '\n') {
          buf[x++] = c;
          break;
        }

        sched_yield();
        c = getchar();
      }
      else {
        sched_yield();
      }
    }
    if (c == '\n')
      buf[x++] = '\n';

    bf[0] = '\n';
    kprintf(bf);

    td->td_retval[0] = x;
  }
  return (0);
}

int sys_pread(struct thread *td, struct sys_pread_args *args) {
  int offset = 0;
  int x = 0;
  char c = 0x0;
  char bf[2];
  volatile char *buf = args->buf;

  struct file *fd = 0x0;

  getfd(td, &fd, args->fd);

  if (args->fd > 3) {
    offset = fd->fd->offset;
    fd->fd->offset = args->offset;
    td->td_retval[0] = fread(args->buf, args->nbyte, 1, fd->fd);
    fd->fd->offset = offset;
  }
  else {
    bf[1] = '\0';
    if (_current->term == tty_foreground)
      c = getchar();

    for (x = 0; x < args->nbyte && c != '\n';) {
      if (_current->term == tty_foreground) {

        if (c != 0x0) {
          buf[x++] = c;
          bf[0] = c;
          kprintf(bf);
        }

        if (c == '\n') {
          buf[x++] = c;
          break;
        }

        sched_yield();
        c = getchar();
      }
      else {
        sched_yield();
      }
    }
    if (c == '\n')
      buf[x++] = '\n';

    bf[0] = '\n';
    kprintf(bf);

    td->td_retval[0] = x;
  }
  return (0);
}

int sys_write(struct thread *td, struct sys_write_args *uap) {
  char *buffer = 0x0;
  struct file *fd = 0x0;

  size_t nbytes;

  if (uap->fd == 2) {
    buffer = kmalloc(1024);
    memcpy(buffer, uap->buf, uap->nbyte);
    printColor += 1;
    kprintf(buffer);
    printColor = defaultColor;
    kfree(buffer);
    td->td_retval[0] = uap->nbyte;
  }
  else if (uap->fd == 1) {
    buffer = kmalloc(1024);
    memcpy(buffer, uap->buf, uap->nbyte);
    kprintf(buffer);
    kfree(buffer);
    td->td_retval[0] = uap->nbyte;
  }
  else {
    getfd(td, &fd, uap->fd);

    kprintf("fd: %i:0x%X, fd_type: %i", uap->fd, fd, fd->fd_type);

    switch (fd->fd_type) {
      case 3: /* XXX - Temp Pipe Stuff */
        nbytes = (uap->nbyte - (1024 - fd->fd->offset) < 0) ? uap->nbyte : (1024 - fd->fd->offset);
        memcpy(fd->fd->buffer, uap->buf, nbytes);
        fd->fd->offset += nbytes;
        td->td_retval[0] = nbytes;
        kprintf("PW: [%i]", nbytes);
        break;
      default:
        kprintf("[%i]", uap->nbyte);
        buffer = kmalloc(uap->nbyte);
        memcpy(buffer, uap->buf, uap->nbyte);
        kprintf("(%i) %s", uap->fd, uap->buf);
        kfree(buffer);
        td->td_retval[0] = uap->nbyte;
    }


  }
  return (0x0);
}

int sys_access(struct thread *td, struct sys_access_args *args) {
  kprintf("%s:%i", args->path, args->amode);
  td->td_retval[0] = 0;
  return (0);
}

int sys_getdirentries(struct thread *td, struct sys_getdirentries_args *args) {
  //kprintf("GDE: [%i:%i:0x%X]", args->fd, args->count, args->basep);
  struct file *fd = 0x0;
  getfd(td, &fd, args->fd);

  char buf[DEV_BSIZE];
  struct dirent *d;
  char *s;
  ssize_t n;

  //fd->offset = 0;
  td->td_retval[0] = fread(args->buf, args->count, 1, fd->fd);
  //n = fsread(fd->fd->ino, args->buf, DEV_BSIZE, fd->fd);
  //td->td_retval[0] = n;

  /*
   while ((n = fsread(*ino, buf, DEV_BSIZE, fd)) > 0)
   for (s = buf; s < buf + DEV_BSIZE;) {
   d = (void *) s;
   if (!strcmp(name, d->d_name)) {
   *ino = d->d_fileno;
   return d->d_type;
   }
   s += d->d_reclen;
   }
   */

  return (0);
}

int sys_readlink(struct thread *thr, struct sys_readlink_args *args) {
  kprintf("RL: %s:%i\n", args->path, args->count);

  //Return Error
  thr->td_retval[0] = 2;
  return (-1);
}

int sys_pipe2(struct thread *thr, struct sys_pipe2_args *args) {
  int error = 0x0;

  int fd1 = 0x0;
  int fd2 = 0x0;

  struct file *nfp1 = 0x0;
  struct file *nfp2 = 0x0;

  fileDescriptor_t *pipeDesc = kmalloc(sizeof(fileDescriptor_t));

  error = falloc(thr, &nfp1, &fd1);
  error = falloc(thr, &nfp2, &fd2);

  nfp1->fd = pipeDesc;
  nfp2->fd = pipeDesc;

  pipeDesc->buffer = kmalloc(1024);

  nfp1->fd_type = 3;
  nfp2->fd_type = 3;


  args->fildes[0] = fd1;
  args->fildes[1] = fd2;

  kprintf("P2: %i, fd1: %i:0x%X, fd1t: %i, fd2: %i:0x%X, fd2t: %i\n", args->flags, fd1, nfp1, nfp1->fd_type, fd2, nfp2, nfp2->fd_type);

  thr->td_retval[0] = 0;
  return (0x0);
}
