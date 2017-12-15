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

 $Id: syscall.c 183 2016-01-22 04:49:08Z reddawg $

 *****************************************************************************************/

#include <ubixos/syscall.h>
/*
 #include <ubixos/syscalls.h>
 */
#include <ubixos/sched.h>
#include <ubixos/exec.h>
#include <sys/elf.h>
#include <ubixos/endtask.h>
#include <ubixos/time.h>
#include <sys/video.h>
#include <sys/trap.h>
#include <vfs/file.h>
#include <ubixfs/ubixfs.h>
#include <lib/string.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/vitals.h>
/* #include <sde/sde.h> */
#include <mpi/mpi.h>
#include <vmm/vmm.h>

//long fuword( const void *base );

//void sdeTestThread();

int InvalidSystemCall() {
  kprintf( "attempt was made to an invalid system call\n" );
  return (0);
}

typedef struct _UbixUser UbixUser;
struct _UbixUser {
  char *username;
  char *password;
  int uid;
  int gid;
  char *home;
  char *shell;
};

int sysAuth( UbixUser *uu ) {
  kprintf( "authenticating user %s\n", uu->username );

  /* MrOlsen 2016-01-01 uh?
   if(uu->username == "root" && uu->password == "user")
   {
   uu->uid = 0;
   uu->gid = 0;
   }
   */
  uu->uid = -1;
  uu->gid = -1;
  return (0);
}

int sysPasswd( char *passwd ) {
  kprintf( "changing user password for user %d\n", _current->uid );
  return (0);
}

int sysAddModule() {
  return (0);
}

int sysRmModule() {
  return (0);
}

int sysGetpid( int *pid ) {
  if ( pid )
    *pid = _current->id;
  return (0);
}

int sysExit( int status ) {
  endTask( _current->id );
  return (0x0);
}

int sysCheckPid( int pid, int *ptr ) {
  kTask_t *tmpTask = schedFindTask( pid );
  if ( (tmpTask != 0x0) && (ptr != 0x0) )
    *ptr = tmpTask->state;
  else
    *ptr = 0x0;
  return (0);
}

/************************************************************************

 Function: int sysGetFreePage();
 Description: Allocs A Page To The Users VM Space
 Notes:

 ************************************************************************/
int sysGetFreePage( struct thread *td, u_int32_t *count) {
  kprintf("sysGetFreePage - Count: %i\n", *count);
  return((int) vmmGetFreeVirtualPage(_current->id, *count, VM_THRD));
  //return(vmmGetFreeVirtualPage(_current->id, *count, VM_TASK));
}

int sysGetFreePage_OLD( long *ptr, int count, int type ) {
  if ( ptr ) {
    if ( type == 2 )
      *ptr = (long) vmmGetFreeVirtualPage( _current->id, count, VM_THRD );
    else
      *ptr = (long) vmmGetFreeVirtualPage( _current->id, count, VM_TASK );
  }
  return (0);
}

int sysGetDrives( uInt32 *ptr ) {
  if ( ptr )
    *ptr = 0x0; //(uInt32)devices;
  return (0);
}

int sysGetUptime( uInt32 *ptr ) {
  if ( ptr )
    *ptr = systemVitals->sysTicks;
  return (0);
}

int sysGetTime( uInt32 *ptr ) {
  if ( ptr )
    *ptr = systemVitals->sysUptime + systemVitals->timeStart;
  return (0);
}

int sys_getcwd(struct thread *td, struct sys_getcwd_args *args) {
  if ( args->buf )
    sprintf( args->buf, "%s", _current->oInfo.cwd );
  return (0);
}

int sys_sched_yield(struct thread *td, void *args) {
  sched_yield();
  return (0);
}

int sysStartSDE() {
  int i = 0x0;
  for ( i = 0; i < 1400; i++ ) {
    asm("hlt");
  }
  //execThread(sdeThread,(uInt32)(kmalloc(0x2000)+0x2000),0x0);
  for ( i = 0; i < 1400; i++ ) {
    asm("hlt");
  }
  return (0);
}

/*
 int invalidCallINT(int sys_call) {
 kprintf("Invalid Sys Call: [%i]\n",sys_call);
 return(0);
 }


 int invalidCall() {
 int sys_call;

 asm(
 "nop"
 : "=a" (sys_call)
 :
 );

 kprintf("Invalid System Call #[%i]\n",sys_call);
 return(0);
 }
 */

/***
 END
 ***/

