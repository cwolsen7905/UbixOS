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

/*
 Page fault wrapper this will aquire some values we need for later use
 */

.globl _vmm_pageFault2
.text
.code32
_vmm_pageFault2:
xchgl %eax,(%esp) /* Save EAX                                         */
movl 4(%esp),%eax /* Move EIP into EAX to use later                   */
pushl %ebx /* Save EBX                                         */
movl 20(%esp),%ebx /* Save ESP for ring 3 to use later                 */
pushl %ecx /* Save ECX,EDX                                     */
pushl %edx
push %ebx /* Push ESP                                         */
push %eax /* Push EIP                                         */
movl %cr2,%eax /* Push the faulted address                         */
pushl %eax
sti /* Turn interrupts back on we are now entrant safe  */
call vmm_pageFault /* Call our page fault handler                      */
addl $0xC,%esp /* Adjust the stack to compensate for pushed values */
popl %edx /* Restore EAX,EBX,ECX,EDX                          */
popl %ecx
popl %ebx
popl %eax
iret /* Return from the interrupt                        */

.globl _vmm_pageFault
.text
.code32

_B4:
_B3:
nop

_ast:

_astRet:

_vmm_pageFault:
pushl $0xC
pushal
push %ds
push %es
push %fs
push %gs
mov $0x10,%eax
mov %eax,%ds
mov %eax,%es
mov %eax,%fs
cld
push %esp
call trap
call _popFS
notyet:
pushl %eax
sti
call vmm_pageFault
/* call syscall */
add $0x4,%esp
cmpb $0x13,0x38(%esp)
je _B4
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
pop %gs
pop %fs
pop %es
pop %ds
popa
add $0x8,%esp
iret

/***
 END
 ***/
