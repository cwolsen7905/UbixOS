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

#include <ubixos/fork.h>
#include <ubixos/types.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>
#include <ubixos/vitals.h>
#include <vmm/vmm.h>
#include <string.h>
#include <assert.h>

/*****************************************************************************************
 Functoin: static int fork_copyProcess(struct taskStruct *newProcess,long ebp,long edi,
 long esi, long none,long ebx,long ecx,long edx,long eip,long cs,long eflags,
 long esp,long ss)
 
 Desc: This function will copy a process

 Notes:

 *****************************************************************************************/
/* Had to remove static though tihs function is only used in this file */
int fork_copyProcess(struct taskStruct *newProcess, long ebp, long edi, long esi, long none, long ebx, long ecx, long edx, long eip, long cs, long eflags, long esp, long ss) {
  volatile struct taskStruct * tmpProcPtr = newProcess;
  assert(newProcess);
  assert(_current);

  /* Set Up New Tasks Information */
  memcpy(newProcess->oInfo.cwd, _current->oInfo.cwd, 1024);

  newProcess->tss.eip = eip;
  newProcess->oInfo.vmStart = _current->oInfo.vmStart;
  newProcess->term = _current->term;
  newProcess->term->owner = newProcess->id;
  newProcess->uid = _current->uid;
  newProcess->gid = _current->gid;
  newProcess->tss.back_link = 0x0;
  newProcess->tss.esp0 = _current->tss.esp0;
  newProcess->tss.ss0 = 0x10;
  newProcess->tss.esp1 = 0x0;
  newProcess->tss.ss1 = 0x0;
  newProcess->tss.esp2 = 0x0;
  newProcess->tss.ss2 = 0x0;
  newProcess->tss.eflags = eflags;
  newProcess->tss.eax = 0x0;
  newProcess->tss.ebx = ebx;
  newProcess->tss.ecx = ecx;
  newProcess->tss.edx = edx;
  newProcess->tss.esi = esi;
  newProcess->tss.edi = edi;
  newProcess->tss.ebp = ebp;
  newProcess->tss.esp = esp;
  newProcess->tss.cs = cs & 0xFF;
  newProcess->tss.ss = ss & 0xFF;
  newProcess->tss.ds = _current->tss.ds & 0xFF;
  newProcess->tss.fs = _current->tss.fs & 0xFF;
  newProcess->tss.gs = _current->tss.gs & 0xFF;
  newProcess->tss.es = _current->tss.es & 0xFF;
  newProcess->tss.ldt = 0x18;
  newProcess->tss.trace_bitmap = 0x0000;
  newProcess->tss.io_map = 0x8000;
  /* Create A Copy Of The VM Space For New Task */
  newProcess->tss.cr3 = (uInt32) vmmCopyVirtualSpace(newProcess->id);
  newProcess->state = FORK;

  /* Fix gcc optimization problems */
  while (tmpProcPtr->state == FORK)
    sched_yield();

  /* Return Id of Proccess */
  return (newProcess->id);
}

/*****************************************************************************************
 Functoin: void sysFork();
 
 Desc: This function will fork a new task

 Notes:
 
 08/01/02 - This Seems To Be Working Fine However I'm Not Sure If I
 Chose The Best Path To Impliment It I Guess We Will See
 What The Future May Bring

 *****************************************************************************************/
asm(
  ".globl sysFork           \n"
  "sysFork:                 \n"
  "  xor   %eax,%eax        \n"
  "  call  schedNewTask     \n"
  "  testl %eax,%eax        \n"
  "  je fork_ret            \n"
  "  pushl %esi             \n"
  "  pushl %edi             \n"
  "  pushl %ebp             \n"
  "  pushl %eax             \n"
  "  call  fork_copyProcess \n"
  "  movl  %eax,(%ebx)      \n"
  "  addl  $16,%esp         \n"
  "fork_ret:                \n"
  "  ret                    \n"
);
