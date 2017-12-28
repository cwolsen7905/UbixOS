/*****************************************************************************************
 Copyright (c) 2002-2004, 2016 The UbixOS Project
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

 $Id: gen_calls.c 169 2016-01-20 01:10:22Z reddawg $

 *****************************************************************************************/

#include <sys/gen_calls.h>
#include <sys/thread.h>
#include <sys/kern_descrip.h>
#include <ubixos/sched.h>
#include <ubixos/endtask.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <assert.h>

/* Exit Syscall */
int sys_exit( struct thread *td, struct sys_exit_args *args ) {
  endTask( _current->id );
  return (0x0);
}

/* return the process id */
int getpid( struct thread *td, struct getpid_args *uap ) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  td->td_retval[0] = _current->id;
  return (0);
}

/* return the process user id */
int getuid( struct thread *td, struct getuid_args *uap ) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  td->td_retval[0] = _current->uid;
  return (0);
}

/* return the process group id */
int getgid( struct thread *td, struct getgid_args *uap ) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  td->td_retval[0] = _current->gid;
  return (0);
}

int sys_write( struct thread *td, struct sys_write_args *uap ) {
  char *buffer = 0x0;
  char *in = 0x0;

#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif

  if ( uap->fd == 2 ) {
    kprintf( "stderr: %s", uap->buf );
  }

  if ( uap->fd == 1 ) {
    /* This is Horrible! */
    in = (char *) uap->buf;
    buffer = kmalloc( 1024 );
    memcpy( buffer, uap->buf, uap->nbyte );
    kprintf( "%s", buffer );
    kfree( buffer );
    td->td_retval[0] = uap->nbyte;
  }
  else {
    kprintf( "[%i]", uap->nbyte );
    buffer = kmalloc( uap->nbyte );
    memcpy( buffer, uap->buf, uap->nbyte );
    //kprint(buffer);
    kfree( buffer );
    kprintf( "(%i) %s", uap->fd, uap->buf );
    td->td_retval[0] = uap->nbyte;
  }
  return (0x0);
}

int issetugid( register struct thread *td, struct issetugid_args *uap ) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  td->td_retval[0] = 0;
  return (0);
}

int readlink( struct thread *td, struct readlink_args *uap ) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  kprintf( "readlink: [%s:%i]\n", uap->path, uap->count );
  td->td_retval[0] = -1;
  td->td_retval[1] = 0x0;
  return (0x0);
}

int gettimeofday_new( struct thread *td, struct gettimeofday_args *uap ) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  return (0x0);
}

int read( struct thread *td, struct read_args *uap ) {
  int error = 0x0;
  size_t count = 0x0;
  struct file *fd = 0x0;

#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif

  error = getfd( td, &fd, uap->fd );

  if ( error )
    return (error);

  count = fread( uap->buf, uap->nbyte, 0x1, fd->fd );
  kprintf( "count: %i\n", count );
  td->td_retval[0] = count;

  return (error);
}

/*!
 * \brief place holder for now functionality to be added later
 */
int setitimer( struct thread *td, struct setitimer_args *uap ) {
  int error = 0x0;

  return (error);
}

int access( struct thread *td, struct access_args *uap ) {
  int error = 0x0;
  kprintf( "name: [%s]\n", uap->path );
  return (error);
}

int mprotect( struct thread *td, struct mprotect_args *uap ) {
  int error = 0x0;
  return (error);
}

/* MrOlsen 2016-01-18 */
int sys_invalid( struct thread *td, void *args ) {
  kprintf( "Invalid System Call #[%i]\n", td->frame->tf_eax );
kpanic("PID: %i, File: %s, Line: %i", _current->id, __FILE__, __LINE__);
  return (0);
}

int sys_read( struct thread *td, struct sys_read_args *args ) {
  kprintf( "DUMMY FUNC FIX ME! sys_read" );
  return (0);
}

int sys_wait4( struct thread *td, struct sys_wait4_args *args ) {
  //kprintf("DUMMY FUNC FIX ME! sys_wait4");
  kTask_t *tmpTask = schedFindTask( args->pid );
  if ( tmpTask != 0x0 )
    return (tmpTask->state);
  return (0);
}

int sys_close( struct thread *td, struct sys_close_args *args ) {
  kprintf( "DUMMY FUNC FIX ME! sys_close" );
  return (0);
}

/***
 END
 ***/
