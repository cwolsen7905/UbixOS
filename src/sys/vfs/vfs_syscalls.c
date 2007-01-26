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

/*!
 * \brief entry point for lseek syscall
 *
 * \param *td pointer to callers thread
 * \param *uap pointer to user space arguements for call
 *
 * \return new offset
 */
int lseek(struct thread *td, struct lseek_args *uap) {
  int error = 0x0;
  struct file *fd    = 0x0;

  #ifdef VFSDEBUG
  kprintf("[%s:%i:%s]",__FILE__,__LINE__,__FUNCTION__);
  #endif

  getfd(td,&fd,uap->fd);
  switch (uap->whence) {
    case SEEK_END:
      K_PANIC("UNHANDLED WHENCE");
      break;
    case SEEK_CUR:
      fd->offset += uap->offset;
      break;
    case SEEK_SET:
      fd->offset = uap->offset;
      break;
    default:
      kprintf("offset: [%i], whence: [%i]\n",uap->offset,uap->whence);
      K_PANIC("Invalid whence");
      break;
    }

  td->td_retval[0] = fd->offset;

  return(error);
  } /* end func */

/*!
 * \brief entry point for read syscall
 *
 * \param *td pointer to callers thread
 * \param *uap pointer to user space arguements for call
 *
 * \return bytes read
 */
int read(struct thread *td,struct read_args *uap) {
  int          error = 0x0;
  int          ch    = 0x0;
  int          i     = 0x0;
  size_t       count = 0x0;
  struct file *fd    = 0x0;
  char        *data  = 0x0;

  #ifdef VFSDEBUG
  kprintf("[%s:%i:%s]",__FILE__,__LINE__,__FUNCTION__);
  #endif

  if (uap->fd < 5) {
    /* Special File Descriptors */
    switch (uap->fd) {
      case 0:
        data = uap->buf;
        for (i = 0x0;i < uap->nbyte;i++) {
          while ((ch = getch()) == 0x0);
          data[i] = ch;
          kprintf("%c",ch);
          if (data[i] == '\n')
             break;
          }
        //kprintf("Read: [%i]\n",i + 1);
        count = i + 1;
        break;
      default:
        kprintf("Invalid Descriptor\n");
        count = 0;
        break;
      }
    }
  else {
    /* Standard File Descriptors */
    error = getfd(td,&fd,uap->fd);

    if (error)
      return(error);

    count = fread(uap->buf,uap->nbyte,0x1,fd);
    }

  #ifdef VFSDEBUG
  kprintf("count: %i - %i\n",count,uap->nbyte);
  #endif
  td->td_retval[0] = count;
  return(error);
  } /* end func */

/*!
 * \brief entry point for write syscall
 *
 * \param *td pointer to callers thread
 * \param *uap pointer to user space arguements for call
 *
 * \return bytes written
 */
int write(struct thread *td, struct write_args *uap) {
  char *buffer = 0x0;
  char *in     = 0x0;

  #ifdef VFSDEBUG
  kprintf("[%s:%i:%s]",__FILE__,__LINE__,__FUNCTION__);
  #endif

  if (uap->fd == 2) {
    in = (char *)uap->buf;
    if (uap->nbyte > 1) {
      buffer = kmalloc(1024);
      memcpy(buffer,uap->buf,uap->nbyte);
      kprintf("STDERR: %s\n",buffer); 
      kfree(buffer);
      }
    td->td_retval[0] = uap->nbyte;
    }
  else if (uap->fd == 1) {
    buffer = kmalloc(uap->nbyte);
    memcpy(buffer,uap->buf,uap->nbyte);
    kprint(buffer);
    kfree(buffer);
    td->td_retval[0] = uap->nbyte;
    }
  else {
    kprintf("[%i]",uap->nbyte);
    buffer = kmalloc(uap->nbyte);
    memcpy(buffer,uap->buf,uap->nbyte);
    //kprint(buffer);
    kfree(buffer);
    kprintf("(%i) %s",uap->fd,uap->buf);
    td->td_retval[0] = uap->nbyte;
    }
  return(0x0);
  } /* end func */

/*!
 * \brief entry point for open syscall
 *
 * \param *td pointer to callers thread
 * \param *uap pointer to user space arguements for call
 *
 * \return index to file descriptor
 */
int open(struct thread *td, struct open_args *uap) {
  int          error = 0x0;
  int          index = 0x0;
  struct file *nfp   = 0x0;

  #ifdef VFSDEBUG
  kprintf("[%s:%i:%s]",__FILE__,__LINE__,__FUNCTION__);
  #endif

  error = falloc(td,&nfp,&index);

  if (error)
     return(error);

  strcpy(nfp->path,uap->path);

  #ifdef VFSDEBUG
  kprintf("OPEN FLAGS: [0x%X],Path: [%s]\n",uap->flags,uap->path);
  #endif
  if (uap->flags != 0x0) {
    kprintf("BAD!\n");
    while (1);
    }
  //BUG make fopen return 0 or -1 if error;
  fopen(nfp,uap->path,"r");
  if (nfp == 0x0)
    td->td_retval[0] = -1;
  else
    td->td_retval[0] = index;
  return (error);
  } /* end func open */

int close(struct thread *td,struct close_args *uap) {
  #ifdef VFSDEBUG
  kprintf("[%s:%i:%s]",__FILE__,__LINE__,__FUNCTION__);
  #endif

  kfree((void *)td->o_files[uap->fd]);
  td->o_files[uap->fd] = 0x0;
  td->td_retval[0] = 0x0;  
  return(0x0);
  } /* end func close */

/***
 END
 ***/
