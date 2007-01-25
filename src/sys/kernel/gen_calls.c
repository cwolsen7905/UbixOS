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

#include <ubixos/types.h>
#include <sys/thread.h>
#include <sys/gen_calls.h>
#include <sys/kern_descrip.h>
#include <ubixos/sched.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/kpanic.h>
#include <string.h>
#include <assert.h>

/* return the process id */
int getpid(struct thread *td, struct getpid_args *uap) {
  #ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif
  td->td_retval[0] = _current->id;
  return (0);
  }

/* return the process user id */
int getuid(struct thread *td, struct getuid_args *uap) {
  #ifdef DEBUG
  kprintf("[%s:%i]\n",__FILE__,__LINE__);
  #endif
  td->td_retval[0] = _current->uid;
  return (0);
  }

/* return the process group id */
int getgid(struct thread *td, struct getgid_args *uap) {
  #ifdef DEBUG
  kprintf("[%s:%i]\n",__FILE__,__LINE__);
  #endif
  td->td_retval[0] = _current->gid;
  return (0);
  }

int issetugid(register struct thread *td, struct issetugid_args *uap) {
  #ifdef NOTIMP
  kprintf("Not Implimented: issetugid\n");
  kprintf("[%s:%i]\n",__FILE__,__LINE__);
  #endif
  td->td_retval[0] = 0;
  return (0);
  }

int readlink(struct thread *td,struct readlink_args *uap) {
  #ifdef NOTIMP
  kprintf("[%s:%i]\n",__FILE__,__LINE__);
  kprintf("readlink: [%s:%i]\n",uap->path,uap->count);
  #endif
  td->td_retval[0] = -1;
  td->td_retval[1] = 0x0;
  return(0x0);
  }

int gettimeofday_new(struct thread *td, struct gettimeofday_args *uap) {
  #ifdef NOTIMP
  kprintf("Not Implimented: gettimeofday\n");
  kprintf("[%s:%i]\n",__FILE__,__LINE__);
  #endif
  return(0x0);
  }

/*!
 * \brief place holder for now functionality to be added later
 */
int setitimer(struct thread *td, struct setitimer_args *uap) {
  int error = 0x0;

  #ifdef NOTIMP
  kprintf("Not Implimented: setitimer\n");
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif

  return(error);
  }

int access(struct thread *td, struct access_args *uap) {
  int error = 0x0;

  #ifdef NOTIMP
  kprintf("Not Implimented: access\n");
  kprintf("name: [%s]\n",uap->path);
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif

  return(error);
  }

int mprotect(struct thread *td, struct mprotect_args *uap) {
  int error = 0x0;

  #ifdef NOTIMP
  kprintf("Not Implimented: mprotect\n");
  kprintf("[%s:%i]",__FILE__,__LINE__);
  #endif

  return(error);
  }

/***
 END
 ***/
