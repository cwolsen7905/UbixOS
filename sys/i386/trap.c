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

#include <i386/signal.h>
#include <sys/trap.h>
#include <sys/gdt.h>
#include <ubixos/sched.h>
#include <lib/kprintf.h>
#include <vmm/vmm.h>

#define KERNEL_STACK 0x2000

void die_if_kernel(char *str, struct trapframe *regs, long err);

#define TRAP_CODE(trap_nr, signr, str, trap_name, tsk) void do_##trap_name(struct trapframe *regs, long error_code) { \
  die_if_kernel(str, regs, error_code); \
}

TRAP_CODE(0, SIGFPE, "divide error", divide_error, _current)
TRAP_CODE(3, SIGTRAP, "int3", int3, _current)
TRAP_CODE(4, SIGSEGV, "overflow", overflow, _current)
TRAP_CODE(5, SIGSEGV, "bounds", bounds, _current)
TRAP_CODE(6, SIGILL, "invalid operand", invalid_op, _current)
TRAP_CODE(7, SIGSEGV, "device not available", device_not_available, _current)
TRAP_CODE(8, SIGSEGV, "double fault", double_fault, _current)
//TRAP_CODE( 9, SIGFPE,  "coprocessor segment overrun", coprocessor_segment_overrun, last_task_used_math)
TRAP_CODE(10, SIGSEGV, "invalid TSS", invalid_TSS, _current)
TRAP_CODE(11, SIGBUS, "segment not present", segment_not_present, _current)
TRAP_CODE(12, SIGBUS, "stack segment", stack_segment, _current)
TRAP_CODE(15, SIGSEGV, "reserved", reserved, _current)
TRAP_CODE(17, SIGSEGV, "alignment check", alignment_check, _current)

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
  unsigned long *stack, addr, module_start, module_end;
  char *_etext = 0x300a0;

  esp = (unsigned long) &regs->tf_esp;

  ss = 0x10; //KERNEL_DS

  //if ((regs->tf_eflags & VM_MASK) || (3 & regs->tf_cs) == 3)
  //  return;

  if ((regs->tf_cs & 3) == 3) {
    esp = regs->tf_esp;
    ss = regs->tf_ss;
    kprintf("USER TASK!");
  }
  else {
    ss = 0x10;
  }

  kprintf("%s: %04lx(%i:%i)[0x%X]\n", str, err & 0xffff, regs->tf_trapno, regs->tf_err, regs->tf_ss);
  kprintf("CPU: %d\n", 0);
  kprintf("EIP:    %04x:[<%08lx>]\nEFLAGS: %08lx\n", 0xffff & regs->tf_cs, regs->tf_eip, regs->tf_eflags);
  kprintf("eax: %08lx   ebx: %08lx   ecx: %08lx   edx: %08lx\n", regs->tf_eax, regs->tf_ebx, regs->tf_ecx, regs->tf_edx);
  kprintf("esi: %08lx   edi: %08lx   ebp: %08lx   esp: %08lx\n", regs->tf_esi, regs->tf_edi, regs->tf_ebp, esp);
  kprintf("cs: 0x%X ds: 0x%X   es: 0x%X   fs: 0x%X   gs: 0x%X   ss: 0x%X\n", regs->tf_cs, regs->tf_ds, regs->tf_es, regs->tf_fs, regs->tf_gs, ss);
  store_TR(i);
  kprintf("Process %s (pid: %i, process nr: %d, stackpage=%08lx)\nStack:", _current->name, _current->id, 0xffff & i, KERNEL_STACK);

  stack = (unsigned long *) esp;

  for (i = 0; i < 16; i++) {
    if (i && ((i % 8) == 0))
      kprintf("\n       ");
    kprintf("%08lx ", get_seg_long(ss, stack++));
  }
  while(1) asm("nop");

}

void trap(struct trapframe *frame) {
  u_int trap_code;
  u_int cr2 = 0;

  struct thread *td = &_current->td;

  trap_code = frame->tf_trapno;

  if ((frame->tf_eflags & PSL_I) == 0) {
    if (SEL_GET_PL(frame->tf_cs) == SEL_PL_USER || (frame->tf_eflags & PSL_VM)) {
      die_if_kernel("TEST", frame, 0x100);
      //     kpanic( "INT OFF! USER" );
    }
    else {
      die_if_kernel("TEST", frame, 0x100);
//      kpanic( "INT OFF! KERN[0x%X]", trap_code );
    }
  }

  cr2 = rcr2();
  kprintf("trap_code: %i(0x%X), EIP: 0x%X, CR2: 0x%X\n", frame->tf_trapno, frame->tf_trapno, frame->tf_eip, cr2);

  die_if_kernel("trapCode", frame, frame->tf_trapno);
  endTask(_current->id);
  sched_yield();
}
