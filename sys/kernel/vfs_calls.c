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
#include <sys/pipe.h>
#include <string.h>
#include <ufs/ufs.h>

int sys_open(struct thread *td, struct sys_open_args *args) {
  int error = 0x0;
  int fd = 0x0;
  struct file *nfp = 0x0;

  error = falloc(td, &nfp, &fd);

  if (error) {
    td->td_retval[0] = -1;
    return (error);
  }


  nfp->fd = fopen(args->path, "rb");

  if (nfp->fd == 0x0) {
    fdestroy(td, nfp, fd);

    td->td_retval[0] = -1;
    error = -1;
  }
  else {
    td->td_retval[0] = fd;
  }

  kprintf("sO: 0x%X:%s:%i", args->mode, args->path, td->td_retval[0]);

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
/*

    kprintf("[sOA: 0x%X:%s:%s:%i]", args->flag, args->mode, args->path, td->td_retval[0]);

    if ((args->flag & O_RDONLY) == O_RDONLY)
      kprintf("O_RDONLY");

    if ((args->flag & O_WRONLY) == O_WRONLY)
      kprintf("O_WRONLY");

    if ((args->flag & O_RDWR) == O_RDWR)
      kprintf("O_RDWR");

    if ((args->flag & O_ACCMODE) == O_ACCMODE)
      kprintf("O_ACCMODE");
*/

  }
  else {
    td->td_retval[0] = fd;
  }

    kprintf("[sOA: 0x%X:%s:%s:%i]", args->flag, args->mode, args->path, td->td_retval[0]);

  return (error);
}

int sys_close(struct thread *td, struct sys_close_args *args) {
  struct file *fd = 0x0;
  struct pipeInfo *pFD = 0x0;

  getfd(td, &fd, args->fd);

#ifdef DEBUG_VFS_CALLS
  kprintf("[sC:%i:0x%X:0x%X]", args->fd, fd, fd->fd);
#endif

  if (fd == 0x0) {
    kprintf("COULDN'T FIND FD: %i", args->fd);
    td->td_retval[0] = -1;
  }
  else {
    switch (fd->fd_type) {
      case 3:
        pFD = fd->data;
        if (args->fd == pFD->rFD) {
           if (pFD->rfdCNT < 2)
             fdestroy(td, fd, args->fd);
           pFD->rfdCNT--;
        }

        if (args->fd == pFD->wFD) {
           if (pFD->wfdCNT < 2)
             fdestroy(td, fd, args->fd);
           pFD->wfdCNT--;
        }

        break;
      default:
        if (args->fd < 3)
          td->td_retval[0] = 0;
        else {
          if (!fclose(fd->fd))
            td->td_retval[0] = -1;
          else {
            kprintf("DESTROY: %i!!!!!!!!!!!!!!!!!!!!!!!!!!!!", args->fd);
            fdestroy(td, fd, args->fd);
            td->td_retval[0] = 0;
          }
        }
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

  struct pipeInfo *pFD = 0x0;
  struct pipeInfo *rpFD = 0x0;

  size_t nbytes;

  int rpCNT = 0;

  getfd(td, &fd, args->fd);

  if (args->fd > 3) {
    switch (fd->fd_type) {
      case 3: /* XXX - Pipe2 Handling */
      pFD = fd->data;
      while (pFD->bCNT == 0 && rpCNT < 100) {
        sched_yield();
        rpCNT++;
      }

      if (rpCNT >= 100 && pFD->bCNT == 0) {
        td->td_retval[0] = 0;
      }
      else {       
        nbytes = (args->nbyte - (pFD->headPB->nbytes - pFD->headPB->offset) <= 0) ? args->nbyte : (pFD->headPB->nbytes - pFD->headPB->offset);
      //kprintf("[unb: %i, nbs: %i, bf: 0x%X]", args->nbyte, nbytes, fd->fd->buffer);
      //kprintf("PR: [%i]", nbytes);
      memcpy(args->buf, pFD->headPB->buffer + pFD->headPB->offset, nbytes);
      pFD->headPB->offset += nbytes;

      if (pFD->headPB->offset >= pFD->headPB->nbytes) {
        rpFD = pFD->headPB;
        pFD->headPB = pFD->headPB->next;
        kfree(rpFD);
        pFD->bCNT--;
      }


        td->td_retval[0] = nbytes;
}
        break;
      default:
        //kprintf("[r:0x%X:%i:%i:%s]",fd->fd, args->fd, fd->fd_type, fd->fd->fileName);
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

  struct pipeInfo *pFD = 0x0;
  struct pipeBuf *pBuf = 0x0;

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

    kprintf("[fd: %i:0x%X, fd_type: %i]", uap->fd, fd, fd->fd_type);

    switch (fd->fd_type) {
      case 3: /* XXX - Temp Pipe Stuff */
      pFD = fd->data;
      pBuf = (struct pipeBuf *) kmalloc(sizeof(struct pipeBuf));
      pBuf->buffer = kmalloc(uap->nbyte);

      //kprintf("[unb: %i, nbs: %i, bf: 0x%X]", uap->nbyte, nbytes, fd->fd->buffer);
      memcpy(pBuf->buffer, uap->buf, uap->nbyte);

      pBuf->nbytes = uap->nbyte;

      if (pFD->tailPB)
        pFD->tailPB->next = pBuf;

      pFD->tailPB = pBuf;

      if (!pFD->headPB)
        pFD->headPB = pBuf;

        pFD->bCNT++;

        td->td_retval[0] = nbytes;
      //kprintf("[PW: %i:%i]", nbytes, fd->fd->offset);
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
