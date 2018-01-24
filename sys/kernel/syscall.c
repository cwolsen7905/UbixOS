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

#include <ubixos/syscalls.h>
#include <ubixos/syscall.h>
#include <ubixos/sched.h>
#include <ubixos/endtask.h>
#include <ubixos/spinlock.h>
#include <ubixos/vitals.h>
#include <sys/trap.h>
#include <sys/elf.h>
#include <string.h>
#include <lib/kprintf.h>
#include <ubixos/kpanic.h>
/* #include <sde/sde.h> */
#include <vmm/vmm.h>

void sys_call(struct trapframe *frame) {
  uint32_t code = 0x0;
  caddr_t params;

  struct thread *td = &_current->td;

  td->frame = frame;

  int error = 0x0;

  params = (caddr_t) frame->tf_esp + sizeof(int);

  code = frame->tf_eax;

  if (code > totalCalls) {
    die_if_kernel("Invalid System Call", frame, frame->tf_eax);
    kpanic("PID: %i", _current->id);
  }
  else if ((uint32_t) systemCalls[code].sc_status == SYSCALL_INVALID) {
    kprintf("Invalid Call: [%i][0x%X]\n", code, (uint32_t) systemCalls[code].sc_name);
    frame->tf_eax = -1;
    frame->tf_edx = 0x0;
  }
  else {
    td->td_retval[0] = 0;
    td->td_retval[1] = frame->tf_edx;

    if (systemCalls[code].sc_status == SYSCALL_DUMMY)
      kprintf("Syscall->abi: [%i], PID: [%i], Code: %i, Call: %s\n", td->abi, _current->id, frame->tf_eax, systemCalls[code].sc_name);
/*
    if (td->abi == ELFOSABI_UBIXOS)
     error = (int) systemCalls[code].sc_entry( frame->tf_ebx, frame->tf_ecx, frame->tf_edx );
    else */if (td->abi == ELFOSABI_FREEBSD)
      error = (int) systemCalls[code].sc_entry(td, params);
    else
      error = (int) systemCalls[code].sc_entry(td, params);

    if (systemCalls[code].sc_status == SYSCALL_DUMMY) {
      kprintf("DUMMY CALL: (%i)\n", code);
      return;
    }

kprintf("ERROR: 0x%X",error);
    switch (error) {
      case 0:
        frame->tf_eax = td->td_retval[0];
  //      frame->tf_edx = td->td_retval[1];
        frame->tf_eflags &= ~PSL_C;
      break;
      default:
        frame->tf_eax = td->td_retval[0];
        frame->tf_edx = td->td_retval[1];
        frame->tf_eflags |= PSL_C;
      break;
    }
  }
}

int invalidCall() {
  int sys_call;

  asm(
    "nop"
    : "=a" (sys_call)
    :
  );

  kprintf("Invalid System Call #[%i], PID: %i\n", sys_call, _current->id);
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

int sysAuth(UbixUser *uu) {
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
  return (0);
}

int sysPasswd(char *passwd) {
  kprintf("changing user password for user %d\n", _current->uid);
  return (0);
}

int sysAddModule() {
  return (0);
}

int sysRmModule() {
  return (0);
}

int sysGetpid(int *pid) {
  if (pid)
    *pid = _current->id;
  return (0);
}

int sysExit(int status) {
  endTask(_current->id);
  return (0x0);
}

int sysCheckPid(int pid, int *ptr) {
  kTask_t *tmpTask = schedFindTask(pid);
  if ((tmpTask != 0x0) && (ptr != 0x0))
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
int sysGetFreePage(struct thread *td, uint32_t *count) {

  td->td_retval[0] = vmm_getFreeVirtualPage(_current->id, *count, VM_THRD);
  return(0);
  //return(vmm_getFreeVirtualPage(_current->id, *count, VM_TASK));
}

int sysGetDrives(uInt32 *ptr) {
  if (ptr)
    *ptr = 0x0; //(uInt32)devices;
  return (0);
}

int sysGetUptime(uInt32 *ptr) {
  if (ptr)
    *ptr = systemVitals->sysTicks;
  return (0);
}

int sysGetTime(uInt32 *ptr) {
  if (ptr)
    *ptr = systemVitals->sysUptime + systemVitals->timeStart;
  return (0);
}

int sys_getcwd(struct thread *td, struct sys_getcwd_args *args) {
  char *buf = (char *) args->buf;
  char *cwd = _current->oInfo.cwd; 

  while (cwd[0] != '/')
    cwd++;

  if (args->buf) {
    sprintf(buf, "%s", cwd);
    buf[strlen(cwd)] = '\0';
 
    //sprintf(buf, "%s", _current->oInfo.cwd);
    //buf[strlen(_current->oInfo.cwd)] = '\0';
    //MrOlsen (2018-01-01) - Why is sprintf not null terminating
  }
 // kprintf("GETCWD: [%s][0x%X]\n", _current->oInfo.cwd, args->buf);
 // kprintf("[%s]", args->buf);
  return (0);
}

int sys_sched_yield(struct thread *td, void *args) {
  sched_yield();
  return (0);
}

int sysStartSDE() {
  int i = 0x0;
  for (i = 0; i < 1400; i++) {
    asm("hlt");
  }
  //execThread(sdeThread,0x2000),0x0);
  for (i = 0; i < 1400; i++) {
    asm("hlt");
  }
  return (0);
}
