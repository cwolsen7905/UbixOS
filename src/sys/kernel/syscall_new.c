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

#include <ubixos/syscalls_new.h>
#include <ubixos/sched.h>
#include <ubixos/types.h>
#include <ubixos/endtask.h>
#include <ubixos/spinlock.h>
#include <sys/trap.h>
#include <lib/string.h>
#include <lib/kprintf.h>

spinLock_t Master = SPIN_LOCK_INITIALIZER;

void syscall(struct trapframe frame) {
  uInt32 code = 0x0;
  caddr_t params;
  struct thread *td = &_current->td;
  int args[8];
  int error = 0x0;

  params = (caddr_t)frame.tf_esp + sizeof(int);
 
  code = frame.tf_eax;
  if (code == 198) {
    memcpy(&code,params,sizeof(int));
    params += sizeof(quad_t);
    }

  if (code > totalCalls_new) {
    kprintf("Invalid Call: [%i]\n",frame.tf_eax);
    }
  else if ((uInt32)systemCalls_new[code] == 0x0) {
    kprintf("Invalid Call: [%i][0x%X]\n",code,(uInt32)systemCalls_new[code]);
    frame.tf_eax = -1;
    frame.tf_edx = 0x0;
    }
  else {
    memcpy(args,params,8 * sizeof(int));
    td->td_retval[0] = 0;
    td->td_retval[1] = frame.tf_edx;

    error = (int)systemCalls_new[code](td,args);
    switch (error) {
      case 0:
        frame.tf_eax = td->td_retval[0];
        frame.tf_edx = td->td_retval[1];
        frame.tf_eflags &= ~PSL_C;
        break;
      case ERESTART:
        frame.tf_eip -= frame.tf_err;
        break;
      case EJUSTRETURN:
        break;
      default:
        frame.tf_eax = error;
        frame.tf_eflags |= PSL_C;
        break;

      }
    }
  }


/***
 END
 ***/

