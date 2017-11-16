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

 $Id: sys_call.S 199 2016-01-23 14:47:53Z reddawg $

 *****************************************************************************************/

.globl _sysCall
.text
.code32

_BALLS:
_B3:
nop

_ast:

_astRet:

_sysCall:
push $0x2
sub $0x4,%esp
pusha
push %ds
push %es
push %fs
mov $0x10,%eax
mov %eax,%ds
mov %eax,%es
mov %eax,%fs
cld
push %esp
call syscall
add $0x4,%esp
cmpb $0x13,0x38(%esp)
je _BALLS
testl $0x2000,0x3c(%esp) /* Test If VM */
jz _notVM
jmp _isVM
_notVM:
testb $0x3,0x38(%esp) /* See If We're In User CS (GDT Entry 5) */
jz _popFS
cli
mov %fs:0x0,%eax
testl $0x10800,0x80(%eax) /* Document This */
je _popFS
sti
push %esp
call _ast
add $0x4,%esp
jmp _astRet
_isVM:
hlt

_popFS:
pop %fs
pop %es
pop %ds
popa
add $0x8,%esp
iret

/***
 END
 ***/
