/*****************************************************************************************
 Copyright (c) 2002-2005 The UbixOS Project
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

 $Id: idt.c 135 2016-01-15 22:22:55Z reddawg $

 *****************************************************************************************/

#include <sys/idt.h>
#include <sys/gdt.h>
#include <sys/io.h>
#include <ubixos/sched.h>
#include <isa/8259.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <vmm/vmm.h>
#include <ubixos/syscall.h>
#include <ubixos/kpanic.h>
#include <ubixos/endtask.h>
#include <string.h>
#include <sys/trap.h>

#define FP_TO_LINEAR(seg, off) ((void*) ((((uInt16) (seg)) << 4) + ((uInt16) (off))))

static ubixDescriptorTable(ubixIDT, 256) {};

static struct {
  unsigned short limit __attribute__((packed));
  union descriptorTableUnion *idt __attribute__((packed));
} loadidt = { (256 * sizeof(union descriptorTableUnion) - 1), ubixIDT };

/************************************************************************

 Function: int idtInit()
 Description: This function is used to enable our IDT subsystem
 Notes:

 02/20/2004 - Approved for quality

 ************************************************************************/
int idt_init() {
  int i = 0x0;

  struct tssStruct *sfTSS = (struct tssStruct *) 0x6200;
  struct tssStruct *gpfTSS = (struct tssStruct *) 0x4200;

  /* Set up default vector table for all possible 256 interrupts */

  for ( i = 0x0; i < 256; i++ ) {
    setVector( intNull, i, dPresent + dInt + dDpl3 );
  }

  /* Load the IDT into the system */
  asm volatile(
      "cli                      \n"
      "lidt (%0)                \n" /* Load the IDT                */
      "pushfl                   \n" /* Clear the NT flag           */
      "andl $0xffffbfff,(%%esp)  \n"
      "popfl                    \n"
      "sti                      \n"
      :
      : "r" ((char *)&loadidt)
  );

  /* Set up the basic vectors for the reserved ints */
  setVector( _int0, 0, dPresent + dInt + dDpl0 );
  setVector( _int1, 1, dPresent + dInt + dDpl0 );
  setVector( _int2, 2, dPresent + dInt + dDpl0 );
  setVector( _int3, 3, dPresent + dInt + dDpl0 );
  setVector( _int4, 4, dPresent + dInt + dDpl0 );
  setVector( _int5, 5, dPresent + dInt + dDpl0 );
  setVector( _int6, 6, dPresent + dInt + dDpl0 );
  setVector( _int7, 7, dPresent + dInt + dDpl0 );
  setTaskVector( 8, dPresent + dTask + dDpl0, 0x40 );
  setVector( _int9, 9, dPresent + dInt + dDpl0 );
  setVector( _int10, 10, dPresent + dInt + dDpl0 );
  setVector( _int11, 11, dPresent + dInt + dDpl0 );
  setVector( _int12, 12, dPresent + dInt + dDpl0 );
  setTaskVector( 13, dPresent + dTask + dDpl0, 0x38 );
  setVector( _vmm_pageFault, 14, dPresent + dInt + dDpl0 );
  setVector( _sysCall, 0x80, dPresent + dTrap + dDpl3 );
  setVector( _sysCall_new, 0x81, dPresent + dTrap + dDpl3 );
  //setVector(_sysCallNew, 0x90, dPresent + dTrap + dDpl3);
  setVector( timerInt, 0x68, (dInt + dPresent + dDpl0) );

  gpfTSS->back_link = 0x0;
  gpfTSS->esp0 = 0x0;
  gpfTSS->ss0 = 0x0;
  gpfTSS->esp1 = 0x0;
  gpfTSS->ss1 = 0x0;
  gpfTSS->esp2 = 0x0;
  gpfTSS->ss2 = 0x0;
  gpfTSS->cr3 = (unsigned int) kernelPageDirectory;
  gpfTSS->eip = (unsigned int) &_int13;
  gpfTSS->eflags = 0x206;
  gpfTSS->esp = 0x1D000;
  gpfTSS->ebp = 0x1D000;
  gpfTSS->esi = 0x0;
  gpfTSS->edi = 0x0;
  gpfTSS->es = 0x10;
  gpfTSS->cs = 0x08;
  gpfTSS->ss = 0x10;
  gpfTSS->ds = 0x10;
  gpfTSS->fs = 0x10;
  gpfTSS->gs = 0x10;
  gpfTSS->ldt = 0x0;
  gpfTSS->trace_bitmap = 0x0000;
  gpfTSS->io_map = 0x8000;

  memset( sfTSS, 0x0, sizeof(struct tssStruct) );
  sfTSS->cr3 = (unsigned int) kernelPageDirectory;
  sfTSS->eip = (unsigned int) &_int8;
  sfTSS->eflags = 0x206;
  sfTSS->esp = 0x1C000;
  sfTSS->ebp = 0x1C000;
  sfTSS->es = 0x10;
  sfTSS->cs = 0x08;
  sfTSS->ss = 0x10;
  sfTSS->ds = 0x10;
  sfTSS->fs = 0x10;
  sfTSS->gs = 0x10;
  sfTSS->io_map = 0x8000;

  /* Print out information for the IDT */
  kprintf( "idt0 - Address: [0x%X]\n", &ubixIDT );

  /* Return so we know all went well */
  return (0x0);
}

/* Sets Up IDT Vector */
void setVector( void *handler, unsigned char interrupt, unsigned short controlMajor ) {
  unsigned short codesegment = 0x08;
  asm volatile ("movw %%cs,%0":"=g" (codesegment));

  ubixIDT[interrupt].gate.offsetLow = (unsigned short) (((unsigned long) handler) & 0xffff);
  ubixIDT[interrupt].gate.selector = codesegment;
  ubixIDT[interrupt].gate.access = controlMajor;
  ubixIDT[interrupt].gate.offsetHigh = (unsigned short) (((unsigned long) handler) >> 16);
}

/************************************************************************

 Function: void setTaskVector(uInt8,uInt16,uInt8);
 Description: This Function Sets Up An IDT Task Vector
 Notes:

 ************************************************************************/
void setTaskVector( uInt8 interrupt, uInt16 controlMajor, uInt8 selector ) {
  uInt16 codesegment = 0x08;
  asm volatile ("movw %%cs,%0":"=g" (codesegment));

  ubixIDT[interrupt].gate.offsetLow = 0x0;
  ubixIDT[interrupt].gate.selector = selector;
  ubixIDT[interrupt].gate.access = controlMajor;
  ubixIDT[interrupt].gate.offsetHigh = 0x0;
}

/* Null Intterupt Descriptor */
void intNull() {
  kprintf( "Invalid Interrupt[%i]\n", _current->id );
  /*
   kpanic("Invalid Interrupt[%i]\n",_current->id);
   while (1);
   endTask(_current->id);
   sched_yield();
   */
}

void _int0() {
  kpanic( "int0: Divide-by-Zero [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int1() {
  kpanic( "int1: Debug exception [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int2() {
  kpanic( "int2: unknown error [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int3() {
  kpanic( "int3: Breakpoint [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int4() {
  kpanic( "int4: Overflow [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int5() {
  kpanic( "int5: Bounds check [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

asm volatile(
  ".globl _int6       \n"
  "_int6:             \n"
  "  pushal               \n" /* Save all registers           */
  "  push %ds             \n"
  "  push %es             \n"
  "  push %fs             \n"
  "  push %gs             \n"
  "  push %esp            \n"
  "  call __int6          \n"
  "  pop %gs              \n"
  "  pop %fs              \n"
  "  pop %es              \n"
  "  pop %ds              \n"
  "  popal                \n"
  "  iret                 \n" /* Exit interrupt                           */
);


void __int6(struct trapframe *frame) {
  die_if_kernel("invalid_op",frame,6);
  //kprintf("tf_gs: 0x%X, tf_fs: 0x%X, tf_es: 0x%X, tf_ds: 0x%X\n", frame->tf_gs, frame->tf_fs, frame->tf_es, frame->tf_ds);
  //kpanic( "int6: Invalid opcode! [%i:0x%X:0x%X]\n", _current->id, _current->tss.eip, frame->tf_eip );
  endTask( _current->id );
  sched_yield();
}

void _int8() {
  struct tssStruct *sfTSS = (struct tssStruct *) 0x6200;
  kpanic( "int8: Double Fault! [%i]\n", _current->id );
  sfTSS->cr3 = (unsigned int) kernelPageDirectory;
  sfTSS->eip = (unsigned int) &_int8;
  sfTSS->eflags = 0x206;
  sfTSS->esp = 0x1C000;
  sfTSS->ebp = 0x1C000;
  sfTSS->es = 0x10;
  sfTSS->cs = 0x08;
  sfTSS->ss = 0x10;
  sfTSS->ds = 0x10;
  sfTSS->fs = 0x10;
  sfTSS->gs = 0x10;
  sfTSS->io_map = 0x8000;
  while ( 1 )
    asm("nop");
}

void _int9() {
  kpanic( "int9: Coprocessor Segment Overrun! [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int10() {
  kpanic( "int10: Invalid TSS! [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int11() {
  kpanic( "int11: Segment Not Present! [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int12() {
  kpanic( "int12: Stack-Segment Fault! [%i]\n", _current->id );
  endTask( _current->id );
  sched_yield();
}

void _int13() {
  uInt8 *ip = 0x0;
  uInt16 *stack = 0x0, *ivt = 0x0;
  uInt32 *stack32 = 0x0;
  bool isOperand32 = FALSE, isAddress32 = FALSE;
  struct tssStruct *gpfTSS = (struct tssStruct *) 0x4200;

  irqDisable( 0x0 );

  gpfTSS->eip = (unsigned int) &_int13;
  gpfTSS->esp = 0x1D000;
  gpfTSS->ebp = 0x1D000;
  gpfTSS->eflags = 0x206;

  ip = FP_TO_LINEAR( _current->tss.cs, _current->tss.eip );
  ivt = (uInt16 *) 0x0;
  stack = (uInt16 *) FP_TO_LINEAR( _current->tss.ss, _current->tss.esp );
  stack32 = (uInt32 *) stack;

  gpfStart: switch ( ip[0] ) {
  case 0xCD: /* INT n */
    switch ( ip[1] ) {
    case 0x69:
      kprintf( "Exit Bios [0x%X]\n", _current->id );
      _current->state = DEAD;
      break;
    case 0x20:
    case 0x21:
      kpanic( "GPF OP 0x20/0x21\n" );
      break;
    default:
      stack -= 3;
      _current->tss.esp = ((_current->tss.esp & 0xffff) - 6) & 0xffff;
      stack[0] = (uInt16)( _current->tss.eip + 2 );
      stack[1] = _current->tss.cs;
      stack[2] = (uInt16) _current->tss.eflags;
      if ( _current->oInfo.v86If )
        stack[2] |= EFLAG_IF;
      else
        stack[2] &= ~EFLAG_IF;
      _current->tss.cs = ivt[ip[1] * 2 + 1] & 0xFFFF;
      _current->tss.eip = ivt[ip[1] * 2] & 0xFFFF;
      break;
    }
    break;
  case 0x66:
    isOperand32 = TRUE;
    ip++;
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    goto gpfStart;
    break;
  case 0x67:
    isAddress32 = TRUE;
    ip++;
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    goto gpfStart;
    break;
  case 0xF0:
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    kpanic( "GPF OP 0xF0\n" );
    break;
  case 0x9C:
    if ( isOperand32 == TRUE ) {
      _current->tss.esp = ((_current->tss.esp & 0xffff) - 4) & 0xffff;
      stack32--;
      stack32[0] = _current->tss.eflags & 0xDFF;
      if ( _current->oInfo.v86If == TRUE )
        stack32[0] |= EFLAG_IF;
      else
        stack32[0] &= ~EFLAG_IF;
    }
    else {
      _current->tss.esp = ((_current->tss.esp & 0xffff) - 2) & 0xffff;
      stack--;

      stack[0] = (uInt16) _current->tss.eflags;
      if ( _current->oInfo.v86If == TRUE )
        stack[0] |= EFLAG_IF;
      else
        stack[0] &= ~EFLAG_IF;
      _current->tss.eip = (uInt16)( _current->tss.eip + 1 );

    }
    break;
  case 0x9D:
    if ( isOperand32 == TRUE ) {
      _current->tss.eflags = EFLAG_IF | EFLAG_VM | (stack32[0] & 0xDFF);
      _current->oInfo.v86If = (stack32[0] & EFLAG_IF) != 0;
      _current->tss.esp = ((_current->tss.esp & 0xffff) + 4) & 0xffff;
    }
    else {
      _current->tss.eflags = EFLAG_IF | EFLAG_VM | stack[0];
      _current->oInfo.v86If = (stack[0] & EFLAG_IF) != 0;
      _current->tss.esp = ((_current->tss.esp & 0xffff) + 2) & 0xffff;
    }
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    /* kprintf("popf [0x%X]\n",_current->id); */
    break;
  case 0xFA:
    _current->oInfo.v86If = FALSE;
    _current->tss.eflags &= ~EFLAG_IF;
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    _current->oInfo.timer = 0x1;
    break;
  case 0xFB:
    _current->oInfo.v86If = TRUE;
    _current->tss.eflags |= EFLAG_IF;
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    _current->oInfo.timer = 0x0;
    /* kprintf("sti [0x%X]\n",_current->id); */
    break;
  case 0xCF:
    _current->tss.eip = stack[0];
    _current->tss.cs = stack[1];
    _current->tss.eflags = EFLAG_IF | EFLAG_VM | stack[2];
    _current->oInfo.v86If = (stack[2] & EFLAG_IF) != 0;
    _current->tss.esp = ((_current->tss.esp & 0xffff) + 6) & 0xffff;
    /* kprintf("iret [0x%X]\n",_current->id); */
    break;
  case 0xEC: /* IN AL,DX */
    _current->tss.eax = (_current->tss.eax & ~0xFF) | inportByte( _current->tss.edx );
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    break;
  case 0xED: /* IN AX,DX */
    _current->tss.eax = (_current->tss.eax & ~0xFFFF) | inportWord( _current->tss.edx );
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    break;
  case 0xEE: /* OUT DX,AL */
    outportByte( _current->tss.edx, _current->tss.eax & 0xFF );
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    break;
  case 0xEF:
    outportWord( _current->tss.edx, _current->tss.eax );
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    break;
  case 0xF4:
    _current->tss.eip = (uInt16)( _current->tss.eip + 1 );
    break;
  default: /* something wrong */
    kprintf( "NonHandled OpCode [0x%X:0x%X]\n", _current->id, ip[0] );
    _current->state = DEAD;
    break;
  }
  irqEnable( 0 );
  while ( 1 )
    ;
}

/* Removed static however this is the only place it's called from */
void mathStateRestore() {
  if ( _usedMath != 0x0 ) {
    asm(
        "fnsave %0"
        :
        : "m" (_usedMath->i387)
    );
  }
  if ( _current->usedMath != 0x0 ) {
    asm(
        "frstor %0"
        :
        : "m" (_current->i387)
    );
  }
  else {
    asm("fninit");
    _current->usedMath = 0x1;
  }

  _usedMath = _current;

  //Return
}

void _int7();

asm(
    ".globl _int7              \n"
    "_int7:                    \n"
    "  pushl %eax              \n"
    "  clts                    \n"
    "  movl _current,%eax      \n"
    "  cmpl _usedMath,%eax     \n"
    "  je mathDone             \n"
    "  call mathStateRestore   \n"
    "mathDone:                 \n"
    "  popl %eax               \n"
    "  iret                    \n"
);

/***
 END
 ***/
