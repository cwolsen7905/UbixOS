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

#include <ubixos/syscall.h>
#include <ubixos/syscalls.h>
#include <ubixos/sched.h>
#include <ubixos/types.h>
#include <ubixos/exec.h>
#include <ubixos/elf.h>
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

long   fuword(const void *base);

void sdeTestThread();

asm(
  ".globl _sysCallNew           \n"
  "_sysCallNew:                 \n"
  "  pusha                      \n"
  "  push %ss                   \n"
  "  push %ds                   \n"
  "  push %es                   \n"
  "  push %fs                   \n"
  "  push %gs                   \n"
  "  cmpl totalCalls,%eax       \n"
  "  jae  invalidSysCallNew     \n"
  "  mov  %esp,%ebx             \n"
  "  add  $12,%ebx              \n"
  "  push (%ebx)                \n"
  "  call *systemCalls(,%eax,4) \n"
  "  add  $4,%esp               \n"
  "  jmp  doneNew               \n"
  "invalidSysCallNew:           \n"
  "  call InvalidSystemCall     \n"
  "doneNew:                     \n"
  "  pop %gs                    \n"
  "  pop %fs                    \n" 
  "  pop %es                    \n"
  "  pop %ds                    \n"
  "  pop %ss                    \n"
  "  popa                       \n"
  "  iret                       \n"
  );

void InvalidSystemCall()
{
	kprintf("attempt was made to an invalid system call\n");
	return;
}

typedef struct _UbixUser UbixUser;
struct _UbixUser
{
	char *username;
	char *password;
	int uid;
	int gid;
	char *home;
	char *shell;
};

void sysAuth(UbixUser *uu)
{
	kprintf("authenticating user %s\n", uu->username);

	/* MrOlsen 2016-01-01 uh? 
	if(uu->username == "root" && uu->password == "user")
	{
		uu->uid = 0;
		uu->gid = 0;
	}
*/
	uu->uid = -1;
	uu->gid = -1;
	return;
}

void sysPasswd(char *passwd)
{
	kprintf("changing user password for user %d\n", _current->uid);
	return;
}

void sysAddModule()
{
	return;
}

void sysRmModule()
{
	return;
}

void sysGetpid(int *pid)
{
	if (pid)
		*pid = _current->id;
		return;
}

void sysGetUid(int *uid) {
  if (uid)
    *uid = _current->uid;
  return;
  }

void sysGetGid(int *gid) {
  if (gid)
    *gid = _current->gid;
  return;
  }

void sysSetUid(int uid,int *status) {
  if (_current->uid == 0x0) {
    _current->uid = uid;
    if (status)
      *status = 0x0;
    }
  else {
    if (status)
      *status = 1;
    }
  return;
  }

void sysSetGid(int gid,int *status) {
  if (_current->gid == 0x0) {
    _current->gid = gid;
    if (status)
      *status = 0x0;
    }
  else {
    if (status)
      *status = 1;
    }
  return;
  }  

void sysExit(int status)
{
	endTask(_current->id);
}

void sysCheckPid(int pid,int *ptr)
{
	kTask_t *tmpTask = schedFindTask(pid);
	if ((tmpTask != 0x0) && (ptr != 0x0))
		*ptr = tmpTask->state;
	else
		*ptr = 0x0;
	return;
}

/************************************************************************

Function: void sysGetFreePage();
Description: Allocs A Page To The Users VM Space
Notes:

************************************************************************/  
void sysGetFreePage(long *ptr,int count,int type) {
  if (ptr) {
    if (type == 2)
      *ptr = (long) vmmGetFreeVirtualPage(_current->id,count,VM_THRD);
    else
      *ptr = (long) vmmGetFreeVirtualPage(_current->id,count,VM_TASK);
    }
  return;
  }

void sysGetDrives(uInt32 *ptr)
{
	if (ptr)
		*ptr = 0x0;//(uInt32)devices;
	return;
}

void sysGetUptime(uInt32 *ptr)
{
	if (ptr)
		*ptr = systemVitals->sysTicks;
	return;
}

void sysGetTime(uInt32 *ptr)
{
	if (ptr)
		*ptr = systemVitals->sysUptime + systemVitals->timeStart;
	return;
}


void sysGetCwd(char *data,int len)
{
	if (data)
		sprintf(data,"%s", _current->oInfo.cwd);
	return;
}

void sysSchedYield() {
  sched_yield();
  }

void sysStartSDE() {
  int i = 0x0;
  for (i=0;i<1400;i++) {
    asm("hlt");
    }
  //execThread(sdeThread,(uInt32)(kmalloc(0x2000)+0x2000),0x0);
  for (i=0;i<1400;i++) {
    asm("hlt");
    }
  return;
  }

void invalidCall(int sys_call) {
  kprintf("Invalid System Call #[%i]\n",sys_call);
  return;
  }

/***
 END
 ***/

