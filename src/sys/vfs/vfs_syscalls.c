/*****************************************************************************************
 Copyright (c) 2007 The UbixOS Project
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

#include <vfs/vfs.h>
#include <sys/kern_descrip.h>
#include <ubixos/kpanic.h>
#include <lib/kprint.h>

int lseek(struct thread *td, struct lseek_args *uap) {
  int error = 0x0;
  struct file *fd    = 0x0;

  getfd(td,&fd,uap->fd);
  switch (uap->whence) {
    case SEEK_END:
      K_PANIC("UNHANDLED WHENCE");
      break;
    case SEEK_CUR:
      fd->fd->offset += uap->offset;
      break;
    case SEEK_SET:
      fd->fd->offset = uap->offset;
      break;
    default:
      kprintf("offset: [%i], whence: [%i]\n",uap->offset,uap->whence);
      K_PANIC("Invalid whence");
      break;
    }

  return(error);
  } /* end func */

/***
 END
 ***/
