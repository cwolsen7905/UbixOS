/*****************************************************************************************
 Copyright (c) 2016 The UbixOS Project
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

 $Id: syscall_new.c 172 2016-01-20 13:57:36Z reddawg $

 *****************************************************************************************/

#include <sys/trap.h>
#include <sys/gdt.h>
#include <ubixos/sched.h>
#include <lib/kprintf.h>

void trap( struct trapframe *frame ) {
  u_int trap_code;
  u_int cr2 = 0;

  struct thread *td = &_current->td;

  trap_code = frame->tf_trapno;

  if ( (frame->tf_eflags & PSL_I) == 0 ) {
    if (SEL_GET_PL(frame->tf_cs) == SEL_PL_USER || (frame->tf_eflags & PSL_VM))
      kpanic( "INT OFF! USER" );
    else
      kpanic( "INT OFF! KERN" );
  }

  kprintf("trap_code: %i, EIP: 0x%X\n", frame->tf_trapno, frame->tf_eip);

  switch (trap_code) {
    case 0xC:
       cr2 = rcr2();
       asm("sti"); /* Turn Back On Ints! */
       vmm_pageFault(frame, cr2);
    default:
      break;
  }

  kprintf("GOTTA RETURN!\n");
  while(1)
;
}

/***
 END
 ***/

