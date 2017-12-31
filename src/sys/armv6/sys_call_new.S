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

 $Id: sys_call_new.S 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#define FAKE_MCOUNT(caller)     pushl caller ; call __mcount ; popl %ecx

.globl _sysCall_new
.text
.code32
_sysCall_new:
  pushl   $2                      /* sizeof "int 0x80" */
  subl    $4,%esp                 /* skip over tf_trapno */
  pushal
  pushl   %ds
  pushl   %es
  pushl   %fs
  /* switch to kernel segments */
  movl    $0x10,%eax
  movl    %eax,%ds
  movl    %eax,%es
  movl    %eax,%fs
  //FAKE_MCOUNT(TF_EIP(%esp))
  call    syscall
  //MEXITCOUNT
  //jmp     doreti
  popl    %fs
  popl    %es
  popl    %ds
  popal
  addl    $8,%esp
  iret       
             
invalidSysCall:
  push %eax
  call invalidCall
  pop %eax
  movl $-1,%eax
  iret

/***
 END
 ***/

