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

 $Id$

*****************************************************************************************/

#include <sys/kern_descrip.h>
#include <ubixos/types.h>
#include <sys/sysproto.h>
#include <sys/thread.h>
#include <lib/kprintf.h>
#include <ubixos/endtask.h>
#include <lib/kmalloc.h>
#include <assert.h>

int fcntl(struct thread *td, struct fcntl_args *uap) {
  struct file *fp = 0x0;

  #ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif

  if (td->o_files[uap->fd] == 0x0) {
    kprintf("ERROR: No %i\n",uap->fd);
while(1);
    return(-1);
    }

  fp = (struct file *)td->o_files[uap->fd];
  switch (uap->cmd) {
    case 3:
      td->td_retval[0] = fp->f_flag;
      break;
    case 4:
       fp->f_flag &= ~FCNTLFLAGS;
       fp->f_flag |= FFLAGS(uap->arg & ~O_ACCMODE) & FCNTLFLAGS;
       break;
    default:
      kprintf("ERROR DEFAULT");
    }

  return(0x0);
  }

int falloc(struct thread *td,struct file **resultfp, int *resultfd) {
  struct file *fp = 0x0;
  int i = 0;

  #ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif

  fp = (struct file *)kmalloc(sizeof(struct file));
  /* First 5 Descriptors Are Reserved */
  for (i = 5;i<1024;i++) {
    if (td->o_files[i] == 0x0) {
       td->o_files[i] = (uInt32)fp;
       if (resultfd)
         *resultfd = i;
       if (resultfp)
         *resultfp = fp;
      break;
      }
    } 
  return(0x0);
  }

int close(struct thread *td,struct close_args *uap) {
  #ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif
  kfree((void *)td->o_files[uap->fd]);
  td->o_files[uap->fd] = 0x0;
  td->td_retval[0] = 0x0;  
  return(0x0);
  }

/*!
 * \brief return data table size
 */
int getdtablesize(struct thread *td, struct getdtablesize_args *uap) {
  #ifdef NOTIMP
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif
  td->td_retval[0] = O_FILES;
  return (0);
  }

/* HACK */
int fstat(struct thread *td,struct fstat_args *uap) {
  struct file *fp = 0x0;

  #ifdef NOTIMP
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif

  fp = (struct file *)_current->td.o_files[uap->fd];
  uap->sb->st_mode    = 0x2180;
  uap->sb->st_blksize = 0x1000;
  #ifdef NOTIMP
  kprintf("fstat: %i",uap->fd);
  #endif
  return(0x0);
  }

/*!
 * \brief ioctl functionality not implimented yet
 *
 * \returns NULL for now
 */
int ioctl(struct thread *td, struct ioctl_args *uap) {
  td->td_retval[0] = 0x0;
  return(0x0);
  }

/*!
 * \brief get pointer to file fd in specified thread
 *
 * \return returns fp
 */
int getfd(struct thread *td,struct file **fp,int fd) {
  int error = 0x0;

  #ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif

  *fp = (struct file *)td->o_files[fd];

  if (fp == 0x0)
    error = -1;

  return(error);
  }

/***
 END
 ***/

