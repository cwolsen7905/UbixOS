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
#include <sys/trap.h>
#include <sys/elf.h>
#include <string.h>
#include <lib/kprintf.h>
#include <ubixos/kpanic.h>

struct spinLock Master = SPIN_LOCK_INITIALIZER;

void sys_call_posix(struct trapframe *frame) {
  uint32_t code = 0x0;
  caddr_t params;

  /*
   if (_current->id == 6)
   kprintf("SYSCALL: 0x%X.", frame->tf_eip);
   */

  struct thread *td = &_current->td;

  td->frame = frame;

  int args[8];
  int error = 0x0;

  params = (caddr_t) frame->tf_esp + sizeof(int);

  code = frame->tf_eax;

  if (code == 198) {
    memcpy(&code, params, sizeof(int));
    params += sizeof(quad_t);
  }

  if (code > totalCalls_posix) {
    kprintf("pCall [%i]\n", code);
    die_if_kernel("Invalid System pCall", frame, frame->tf_eax);
    kpanic("PID: %i", _current->id);
  }
  else if ((uint32_t) systemCalls_posix[code].sc_status == SYSCALL_INVALID) {
    kprintf("Invalid Call: [%i][%s]\n", code, systemCalls[code].sc_name);
    frame->tf_eax = -1;
    frame->tf_edx = 0x0;
  }
  else if ((uint32_t) systemCalls_posix[code].sc_status == SYSCALL_NOTIMP) {
    kprintf("Not Implemented Call: [%i][%s]\n", code, systemCalls[code].sc_name);
    frame->tf_eax = -1;
    frame->tf_edx = 0x0;
  }
  else {
    td->td_retval[0] = 0;
    td->td_retval[1] = frame->tf_edx;

    if (systemCalls_posix[code].sc_status == SYSCALL_DUMMY)
      kprintf("Syscall->abi: [%i], PID: [%i], Code: %i, Call: %s\n", td->abi, _current->id, frame->tf_eax, systemCalls[code].sc_name);

    /*
     if ( td->abi == ELFOSABI_UBIXOS )
     error = (int) systemCalls[code].sc_entry( frame->tf_ebx, frame->tf_ecx, frame->tf_edx );
     */

    if (td->abi == ELFOSABI_FREEBSD)
      error = (int) systemCalls_posix[code].sc_entry(td, params);
    else
      error = (int) systemCalls_posix[code].sc_entry(td, params);

    if (systemCalls_posix[code].sc_status == SYSCALL_DUMMY) {
      kprintf("RET(%i)1", code);
      return;
    }

    switch (error) {
      case 0:
        frame->tf_eax = td->td_retval[0];
        frame->tf_edx = td->td_retval[1];
        frame->tf_eflags &= ~PSL_C;
        if (systemCalls_posix[code].sc_status == SYSCALL_DUMMY)
          kprintf("RET3");
      break;
      default:
        frame->tf_eax = td->td_retval[0];
        frame->tf_edx = td->td_retval[1];
        frame->tf_eflags |= PSL_C;
      break;
    }
  }
}

int invalidCall_posix() {
  int sys_call;

  asm(
    "nop"
    : "=a" (sys_call)
    :
  );

  kprintf("Invalid System Call #[%i], PID: %i\n", sys_call, _current->id);
  return (0);
}
