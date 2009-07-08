/*****************************************************************************************
 Copyright (c) 2002-2004,2009 The UbixOS Project
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

/*
   Page fault wrapper this will aquire some values we need for later use
 */

.globl _vmm_pageFault
.text
.code32
_vmm_pageFault:
  xchgl %eax,(%esp)     /* Save EAX                                         */
  movl  4(%esp),%eax   /* Move EIP into EAX to use later                   */
  pushl %ebx                  /* Save EBX                                         */
  movl  20(%esp),%ebx /* Save ESP for ring 3 to use later                 */
  pushl %ecx                  /* Save ECX,EDX                                     */
  pushl %edx
  push  %ebx                 /* Push ESP                                         */
  push  %eax                 /* Push EIP                                         */
  movl  %cr2,%eax        /* Push the faulted address                         */
  pushl %eax
  sti                                /* Turn interrupts back on we are now entrant safe  */
  call  vmm_pageFault     /* Call our page fault handler                      */
  addl  $0xC,%esp         /* Adjust the stack to compensate for pushed values */
  popl  %edx                  /* Restore EAX,EBX,ECX,EDX                          */
  popl  %ecx
  popl  %ebx
  popl  %eax
  iret                               /* Return from the interrupt                        */


/***
 END
 ***/
