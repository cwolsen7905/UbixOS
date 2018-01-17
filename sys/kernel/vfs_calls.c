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
#include <sys/sysproto.h>
#include <sys/descrip.h>
#include <sys/video.h>
#include <string.h>

int sys_open(struct thread *td, struct sys_open_args *args) {
  int error = 0x0;
  int fd = 0x0;
  struct file *nfp = 0x0;

  error = falloc(td, &nfp, &fd);

  if (error)
    return (error);

  nfp->fd = fopen(args->path, "r");

  if (nfp->fd == 0x0) {
    fdestroy(td, nfp, fd);

    td->td_retval[0] = -1;
    error = -1;
  }
  else {
    td->td_retval[0] = fd; //nfp->fd; //MrOlsen 2018index;
  }

  return (error);
}

int sys_close(struct thread *td, struct sys_close_args *args) {
  struct file *fd = 0x0;

  getfd(td, &fd, args->fd);

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

  getfd(td, &fd, args->fd);

  if (args->fd > 3) {
    td->td_retval[0] = fread(args->buf, args->nbyte, 1, fd->fd);
  }
  else {
   bf[1] = '\0';
   if ( _current->term == tty_foreground )
     c = getchar();

    for (x = 0; x < args->nbyte && c != '\n';) {
      if ( _current->term == tty_foreground ) {

        if ( c != 0x0 ) {
          buf[x++] = c;
          bf[0] = c;
          kprint(bf);
        }

        if ( c == '\n') {
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
    if ( c == '\n')
      buf[x++] = '\n';

   //MROlsen 2018 kprintf("READ: %i", x);

    td->td_retval[0] = x;
  }
  return (0);
}

int sys_write(struct thread *td, struct sys_write_args *uap) {
  char *buffer = 0x0;

  if (uap->fd == 2) {
    buffer = kmalloc(1024);
    memcpy(buffer, uap->buf, uap->nbyte);
    printColor += 1;
    kprint(buffer);
    printColor = defaultColor;
    kfree(buffer);
    td->td_retval[0] = uap->nbyte;
  }
  else if (uap->fd == 1) {
    buffer = kmalloc(1024);
    memcpy(buffer, uap->buf, uap->nbyte);
    kprint(buffer);
    kfree(buffer);
    td->td_retval[0] = uap->nbyte;
  }
  else {
    kprintf("[%i]", uap->nbyte);
    buffer = kmalloc(uap->nbyte);
    memcpy(buffer, uap->buf, uap->nbyte);
    //kprint(buffer);
    kfree(buffer);
    kprintf("(%i) %s", uap->fd, uap->buf);
    td->td_retval[0] = uap->nbyte;
  }
  return (0x0);
}

int sys_access(struct thread *td, struct sys_access_args *args) {
  kprintf("%s:%i", args->path, args->amode);
  td->td_retval[0] = 0;
  return(0);
}
