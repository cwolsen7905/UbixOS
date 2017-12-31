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

#define FIRST_TSS_ENTRY 6
#define VM_MASK 0x00020000

#define store_TR(n) \
__asm__("str %%ax\n\t" \
        "subl %2,%%eax\n\t" \
        "shrl $4,%%eax" \
        :"=a" (n) \
        :"0" (0),"i" (FIRST_TSS_ENTRY<<3))

#define get_seg_long(seg,addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

#define get_seg_byte(seg,addr) ({ \
register char __res; \
__asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

void die_if_kernel(char *str, struct trapframe *regs, long err) {
	int i;
	unsigned long esp;
	unsigned short ss;

	esp = (unsigned long) &regs->tf_esp;
	//ss = KERNEL_DS;
        ss = 0x10;
	if ((regs->tf_eflags & VM_MASK) || (3 & regs->tf_cs) == 3)
		return;
	if (regs->tf_cs & 3) {
		esp = regs->tf_esp;
		ss = regs->tf_ss;
	}

	kprintf("%s: %04lx\n", str, err & 0xffff);
	kprintf("EIP:    %04x:%08lx\nEFLAGS: %08lx\n", 0xffff & regs->tf_cs,regs->tf_eip,regs->tf_eflags);
	kprintf("eax: %08lx   ebx: %08lx   ecx: %08lx   edx: %08lx\n", regs->tf_eax, regs->tf_ebx, regs->tf_ecx, regs->tf_edx);
	kprintf("esi: %08lx   edi: %08lx   ebp: %08lx   esp: %08lx\n", regs->tf_esi, regs->tf_edi, regs->tf_ebp, esp);
	kprintf("ds: %04x   es: %04x   fs: %04x   gs: %04x   ss: %04x\n", regs->tf_ds, regs->tf_es, regs->tf_fs, regs->tf_gs, ss);
	store_TR(i);
	//kprintf("Pid: %d, process nr: %d (%s)\nStack: ", _current->id, 0xffff & i, _current->comm);
	kprintf("Pid: %d, process nr: %d ()\nStack: ", _current->id, 0xffff & i);
	for(i=0;i<5;i++)
		kprintf("%08lx ", get_seg_long(ss,(i+(unsigned long *)esp)));
	kprintf("\nCode: ");
	for(i=0;i<20;i++)
		kprintf("%02x ",0xff & get_seg_byte(regs->tf_cs,(i+(char *)regs->tf_eip)));
	kprintf("\n");
}

void trap( struct trapframe *frame ) {
  u_int trap_code;
  u_int cr2 = 0;

  struct thread *td = &_current->td;

  trap_code = frame->tf_trapno;

  if ( (frame->tf_eflags & PSL_I) == 0 ) {
    if (SEL_GET_PL(frame->tf_cs) == SEL_PL_USER || (frame->tf_eflags & PSL_VM)) {
  die_if_kernel("TEST", frame, 0x100);
 //     kpanic( "INT OFF! USER" );
}
    else {
  die_if_kernel("TEST", frame, 0x100);
//      kpanic( "INT OFF! KERN[0x%X]", trap_code );
}
  }

  kprintf("trap_code: %i(0x%X), EIP: 0x%X\n", frame->tf_trapno, frame->tf_trapno, frame->tf_eip);
/*
  switch (trap_code) {
    case 0xC:
       cr2 = rcr2();
       asm("sti"); // Turn Back On Ints!
       vmm_pageFault(frame, cr2);
       kprintf("Called page Fault\n");
    default:
      break;
  }

  kprintf("GOTTA RETURN!\n");
  while(1);
*/
}

/***
 END
 ***/

