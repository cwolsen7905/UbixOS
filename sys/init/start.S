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

 $Id: start.S 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

.globl _start
.text
.code32
_start:
  movw  $0x1234,0x472
  pushl %ebp
  movl  %esp,%ebp
  pushl $0x00000002
  popfl
  /* Clear the BSS */
  movl  $(_end),%ecx
  movl  $(__bss_start),%edi
  subl  %edi,%ecx
  xorl  %eax,%eax
  cld
  rep
  stosb
 
  /* Load GDT */
  lgdtl (loadGDT)
  mov   $0x10,%eax 
  mov   %eax,%ds
  mov   %eax,%es
  mov   %eax,%fs
  mov   %eax,%gs
  mov   %eax,%ss
  mov   $kStack,%eax
  addl  $0x2000,%eax
  mov   %esp,%edx
  mov   %eax,%esp
  mov   %eax,%ebp 
  mov   $0x18,%ax
  lldt  %ax
  mov   $0x20,%ax
  ltr   %ax
  ljmp  $0x08,$start_next
start_next:
  push 8(%edx)
  call kmain

.data
.comm kStack,0x2000

/***
 END
 ***/
