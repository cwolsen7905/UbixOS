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

#include <ubixos/syscall.h>
#include <ubixos/syscall_posix.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <i386/pcpu_asm.h>
#include <sys/io.h>
#include <ubixos/sched.h>
#include <isa/8259.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <vmm/vmm.h>
#include <ubixos/kpanic.h>
#include <ubixos/endtask.h>
#include <string.h>
#include <sys/trap.h>

#define FP_TO_LINEAR(seg, off) ((void*) ((((u_int16_t) (seg)) << 4) + ((u_int16_t) (off))))


void intNull();

void _divideError();
void __divideError(struct trapframe *);

void _debug();
void __debug(struct trapframe *);

void _nmi();
void __nmi(struct trapframe *);

static void _int3();
static void _int4();
static void _int5();
static void _int6();
static void _int7();

void _doubleFault();
void __doubleFault(struct trapframe *);

static void _int9();
static void _int10();
static void _int11();
static void _int12();

void _gpf();
void __gpf(struct trapframe *);

void _floatingPoint();
void __floatingPoint(struct trapframe *);

void _alignmentCheck();
void __alignmentCheck(struct trapframe *);

void _machineCheck();
void __machineCheck(struct trapframe *);

void _simd();
void __simd(struct trapframe *);

void _virtualization();
void __virtualization(struct trapframe *);

void _security();
void __security(struct trapframe *);

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
  struct tssStruct *kernelTSS = (struct tssStruct *) 0x4200;

  /*
   * Initialize the single kernel TSS (GDT selector 0x20, TR loaded once at
   * boot).  Under software task switching this is the live TSS: the CPU reads
   * ss0/esp0 from it on every ring3->ring0 entry (esp0 is updated per switch by
   * switch_to).  Previously the ljmp scheduler repointed GDT[4] away from 0x4200
   * immediately, so it was never initialized.
   */
  kernelTSS->ss0 = 0x10;
  kernelTSS->cr3 = (unsigned int) kernelPageDirectory;
  kernelTSS->ldt = 0x0;
  kernelTSS->io_map = 0x8000;

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

   for (int i = 0;i < 256;i++)
   setVector(intNull, i, dPresent + dTrap + dDpl3);

  /* Set up the basic vectors for the reserved ints */
  setVector(_divideError, 0, dPresent + dInt + dDpl0);
  setVector(_debug, 1, dPresent + dInt + dDpl0);
  setVector(_nmi, 2, dPresent + dInt + dDpl0);
  setVector(_int3, 3, dPresent + dInt + dDpl0);
  setVector(_int4, 4, dPresent + dInt + dDpl0);
  setVector(_int5, 5, dPresent + dInt + dDpl0);
  setVector(_int6, 6, dPresent + dTrap + dDpl0);
  setVector(_int7, 7, dPresent + dInt + dDpl0);
  setVector(_doubleFault, 8, dPresent + dInt + dDpl0);
  //setTaskVector(8, dPresent + dTask + dDpl0, 0x40);
  setVector(_int9, 9, dPresent + dInt + dDpl0);
  setVector(_int10, 10, dPresent + dInt + dDpl0);
  setVector(_int11, 11, dPresent + dInt + dDpl0);
  setVector(_int12, 12, dPresent + dInt + dDpl0);
  /* GPF (#13) is an INTERRUPT GATE, not a hardware task gate: the v86 BIOS-INT
   * virtualizer __gpf reads/writes the faulting state from the trapframe and
   * irets back to VM86.  This is what makes v86 compatible with software task
   * switching (a task gate would do a hardware switch through the shared TSS). */
  setVector(_gpf, 13, dPresent + dInt + dDpl0);
  // setTaskVector(13, dPresent + dTask + dDpl0, 0x38);
  setVector(_vmm_page_fault, 14, dPresent + dInt + dDpl0);
  setVector(_floatingPoint, 16, dPresent + dInt + dDpl0);
  setVector(_alignmentCheck, 17, dPresent + dInt + dDpl0);
  setVector(_machineCheck, 18, dPresent + dInt + dDpl0);
  setVector(_simd, 19, dPresent + dInt + dDpl0);
  setVector(_virtualization, 20, dPresent + dInt + dDpl0);
  setVector(_security, 30, dPresent + dInt + dDpl0);
  setVector(_sys_call_posix, 0x80, dPresent + dTrap + dDpl3);
  setVector(_sys_call, 0x81, dPresent + dTrap + dDpl3);
  setVector(timerInt, 0x68, (dInt + dPresent + dDpl0));

  /*
   * Note: the GPF (#13), double-fault (#8) and v86 BIOS-INT paths used to be
   * hardware task gates with their own TSSes at 0x5200/0x6200.  Those are now
   * ordinary interrupt gates (software task switching), so no per-handler TSS
   * is built here.  The only TSS that matters is the kernel TSS above (0x4200),
   * used solely for ss0/esp0 on ring3->ring0 entry.
   */

  /* Print out information for the IDT */
  kprintf("idt0: addr=0x%X\n", &ubixIDT);

  /* Return so we know all went well */
  return (0x0);
}

/* Sets Up IDT Vector */
void setVector(void *handler, unsigned char interrupt, unsigned short controlMajor) {
  unsigned short codesegment = 0x08;
  asm volatile ("movw %%cs,%0":"=g" (codesegment));

  ubixIDT[interrupt].gate.offsetLow = (unsigned short) (((unsigned long) handler) & 0xffff);
  ubixIDT[interrupt].gate.selector = codesegment;
  ubixIDT[interrupt].gate.access = controlMajor;
  ubixIDT[interrupt].gate.offsetHigh = (unsigned short) (((unsigned long) handler) >> 16);
}

/************************************************************************

 Function: void setTaskVector(u_int8_t,u_int16_t,u_int8_t);
 Description: This Function Sets Up An IDT Task Vector
 Notes:

 ************************************************************************/
void setTaskVector(u_int8_t interrupt, u_int16_t controlMajor, u_int8_t selector) {
  u_int16_t codesegment = 0x08;
  asm volatile ("movw %%cs,%0":"=g" (codesegment));

  ubixIDT[interrupt].gate.offsetLow = 0x0;
  ubixIDT[interrupt].gate.selector = selector;
  ubixIDT[interrupt].gate.access = controlMajor;
  ubixIDT[interrupt].gate.offsetHigh = 0x0;
}

/* Null Intterupt Descriptor */
void _intNull(struct trapframe *frame) {
  die_if_kernel("invalid exception", frame, 0x0);
}

asm(
  ".globl intNull \n"
  "intNull:       \n"
  "  pushl $0x0 \n"
  "  pushl $0x0 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call _intNull \n"
  "  pop %gs      \n"
  "  pop %fs    \n"
  "  pop %es    \n"
  "  pop %ds    \n"
  "  popal      \n"
  "  iret       \n"
);

void __divideError(struct trapframe *frame) {
  die_if_kernel("Divid-by-Zero", frame, 0);
  endTask(_current->id);
    sched_yield();
}

asm(
  ".globl _divideError \n"
  "_divideError:       \n"
  "  pushl $0x0 \n"
  "  pushl $0x6 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call _divideError  \n"
  "  pop %gs      \n"
  "  pop %fs    \n"
  "  pop %es    \n"
  "  pop %ds    \n"
  "  popal      \n"
  "  iret       \n" 
);

void __debug(struct trapframe *frame) {
  die_if_kernel("debug", frame, 0x2);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _debug \n"
  "_debug:       \n"
  "  pushl $0x0 \n"
  "  pushl $0x6 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call _debug  \n"
  "  pop %gs      \n"
  "  pop %fs      \n"
  "  pop %es      \n"
  "  pop %ds      \n"
  "  popal        \n"
  "  iret         \n"
);

void __nmi(struct trapframe *frame) {
  die_if_kernel("nmi", frame, 0x2);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _nmi \n"
  "_nmi:       \n"
  "  pushl $0x0 \n"
  "  pushl $0x6 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call _nmi  \n"
  "  pop %gs      \n"
  "  pop %fs      \n"
  "  pop %es      \n"
  "  pop %ds      \n"
  "  popal        \n"
  "  iret         \n"
);


static void _int3() {
  kpanic("int3: Breakpoint [%i]\n", _current->id);
  endTask(_current->id);
  sched_yield();
}

static void _int4() {
  kpanic("int4: Overflow [%i]\n", _current->id);
  endTask(_current->id);
  sched_yield();
}

static void _int5() {
  kpanic("int5: Bounds check [%i]\n", _current->id);
  endTask(_current->id);
  sched_yield();
}

void __int6(struct trapframe *frame) {
  if (_current->oInfo.v86Task)
    kprintf("__int6: v86 pid=%d tss.cs=0x%X tss.eip=0x%X\n",
        _current->id, (u_int16_t)_current->md.md_tss.cs, _current->md.md_tss.eip);
  die_if_kernel("invalid_opcode", frame, 6);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _int6       \n"
  "_int6:             \n"
  "  pushl $0x0            \n"
  "  pushl $0x6            \n"
  "  pushal               \n" /* Save all registers           */
  "  push %ds             \n"
  "  push %es             \n"
  "  push %fs             \n"
  "  push %gs             \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp            \n"
  "  call __int6          \n"
  "  pop %gs              \n"
  "  pop %fs              \n"
  "  pop %es              \n"
  "  pop %ds              \n"
  "  popal                \n"
  "  iret                 \n" /* Exit interrupt                           */
);

void __doubleFault(struct trapframe *frame) {
  die_if_kernel("double fault", frame, 0x8);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _doubleFault       \n"
  "_doubleFault:                \n"
  "  pushl $0x8           \n"
  "  pushal               \n" /* Save all registers           */
  "  push %ds             \n"
  "  push %es             \n"
  "  push %fs             \n"
  "  push %gs             \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp            \n"
  "  call __doubleFault   \n"
  "  pop %gs              \n"
  "  pop %fs              \n"
  "  pop %es              \n"
  "  pop %ds              \n"
  "  popal                \n"
  "  iret                 \n" /* Exit interrupt                           */
);

static void _int9() {
  kpanic("int9: Coprocessor Segment Overrun! [%i]\n", _current->id);
  endTask(_current->id);
  sched_yield();
}

static __attribute__((optimize("O0"))) void _int10(void) {
  u_int32_t err_code = 0;
  /* After CPU pushes error_code, the C prologue does push ebp; mov ebp,esp.
   * So error_code sits at EBP+4. */
  asm volatile("movl 4(%%ebp), %0" : "=r"(err_code));
  kprintf("int10: Invalid TSS! [pid=%i, errcode=0x%X (sel=0x%X)]\n",
          _current->id, err_code, err_code & ~0x7);
  kprintf("  tss: cs=0x%X ss=0x%X ds=0x%X gs=0x%X ldt=0x%X\n",
          (u_int32_t)_current->md.md_tss.cs, (u_int32_t)_current->md.md_tss.ss,
          (u_int32_t)_current->md.md_tss.ds, (u_int32_t)_current->md.md_tss.gs,
          (u_int32_t)_current->md.md_tss.ldt);
  kpanic("int10: Invalid TSS! [%i]\n", _current->id);
  endTask(_current->id);
  sched_yield();
}

static void _int11() {
  kpanic("int11: Segment Not Present! [%i]\n", _current->id);
  endTask(_current->id);
  sched_yield();
}

static void _int12() {
  kpanic("int12: Stack-Segment Fault! [%i]\n", _current->id);
  endTask(_current->id);
  sched_yield();
}

void __gpf(struct trapframe *frame) {
  u_int8_t *ip = 0x0;
  u_int16_t *stack = 0x0, *ivt = 0x0;
  u_int32_t *stack32 = 0x0;
  bool isOperand32 = FALSE, isAddress32 = FALSE;

  static pidType gpfLastPid = -1;
  static u_int32_t gpfIterCount = 0;

  /*
   * Only VM86 (BIOS) faults are virtualized here.  A #GP from a normal task
   * (eflags.VM clear) is a real fault — a user bug or a kernel bug — not a BIOS
   * instruction to emulate; the VM86 segment slots above tf_ss are not present
   * for it either.  Route it to the generic fault path.
   */
  if (!(frame->tf_eflags & EFLAG_VM)) {
    die_if_kernel("general protection", frame, frame->tf_err);
    endTask(_current->id);
    sched_yield();
    return;
  }

  /*
   * Interrupt-gate v86 handler (was a hardware task gate).  The CPU delivered
   * this GPF from VM86 mode, so the faulting state is in the trapframe — and the
   * VM86 segment registers es/ds/fs/gs sit just above tf_ss (the CPU pushes them
   * on a VM86 fault).  Sync the trapframe into md_tss so the existing
   * virtualization logic (which operates on md_tss) works unchanged; sync back
   * before the iret in _gpf.  Results land in md_tss for bioscall to read.
   */
  u_int32_t *vsegs = (u_int32_t *)&frame->tf_ss; /* [0]=ss [1]=es [2]=ds [3]=fs [4]=gs */
  _current->md.md_tss.eip    = frame->tf_eip;
  _current->md.md_tss.cs     = frame->tf_cs;
  _current->md.md_tss.eflags = frame->tf_eflags;
  _current->md.md_tss.esp    = frame->tf_esp;
  _current->md.md_tss.ss     = frame->tf_ss;
  _current->md.md_tss.es     = vsegs[1];
  _current->md.md_tss.ds     = vsegs[2];
  _current->md.md_tss.fs     = vsegs[3];
  _current->md.md_tss.gs     = vsegs[4];
  _current->md.md_tss.eax    = frame->tf_eax;
  _current->md.md_tss.ecx    = frame->tf_ecx;
  _current->md.md_tss.edx    = frame->tf_edx;
  _current->md.md_tss.ebx    = frame->tf_ebx;
  _current->md.md_tss.esi    = frame->tf_esi;
  _current->md.md_tss.edi    = frame->tf_edi;
  _current->md.md_tss.ebp    = frame->tf_ebp;

  asm("cli");
  isOperand32 = FALSE;
  isAddress32 = FALSE;
  /* Kill VM86 tasks that spin for too long (e.g. BIOS busy-wait loops).
   * The Bochs stdvga GetModeInfo printf processes ~100+ characters × 3 GPFs
   * each (PUSHF, POPF, OUT), requiring ~400 iterations minimum.  4096 gives
   * plenty of headroom while still killing true infinite spin loops. */
  if (_current->id != gpfLastPid) {
    gpfLastPid = _current->id;
    gpfIterCount = 0;
  }
  if (++gpfIterCount > 4096) {
    kprintf("GPF: timeout pid=%d iter=%d at 0x%X:0x%X, killing\n",
        _current->id, gpfIterCount, (u_int16_t)_current->md.md_tss.cs, _current->md.md_tss.eip);
    sched_dead(_current);
    gpfLastPid = -1;
    gpfIterCount = 0;
    goto gpfDone;
  }

  ip = FP_TO_LINEAR(_current->md.md_tss.cs, _current->md.md_tss.eip);

  ivt = (u_int16_t *) 0x0;

  stack = (u_int16_t *) FP_TO_LINEAR(_current->md.md_tss.ss, _current->md.md_tss.esp);
  stack32 = (u_int32_t *) stack;

  gpfStart: switch (ip[0]) {
    case 0xCD: /* INT n */
      switch (ip[1]) {
        case 0x69:
          irqEnable(0);   /* restore timer IRQ masked by sched before v86 switch */
          sched_dead(_current);
        break;
        case 0x20:
        case 0x21:
          kpanic("GPF OP 0x20/0x21\n");
        break;
        default:
          stack -= 3;
          _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) - 6) & 0xffff;
          stack[0] = (u_int16_t) (_current->md.md_tss.eip + 2);
          stack[1] = _current->md.md_tss.cs;
          stack[2] = (u_int16_t) _current->md.md_tss.eflags;
          if (_current->oInfo.v86If)
            stack[2] |= EFLAG_IF;
          else
            stack[2] &= ~EFLAG_IF;

          _current->md.md_tss.cs = ivt[ip[1] * 2 + 1] & 0xFFFF;
          _current->md.md_tss.eip = ivt[ip[1] * 2] & 0xFFFF;
        break;
      }
    break;
    case 0x66:
      isOperand32 = TRUE;
      ip++;
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
      goto gpfStart;
    break;
    case 0x67:
      isAddress32 = TRUE;
      ip++;
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
      goto gpfStart;
    break;
    case 0xF0: /* LOCK prefix — harmless in VM86; skip and re-dispatch */
      ip++;
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 1);
      goto gpfStart;
    break;
    case 0x9C:
      if (isOperand32 == TRUE) {
        _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) - 4) & 0xffff;
        stack32--;
        stack32[0] = _current->md.md_tss.eflags & 0xDFF;
        if (_current->oInfo.v86If == TRUE)
          stack32[0] |= EFLAG_IF;
        else
          stack32[0] &= ~EFLAG_IF;
        _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 1);
      }
      else {
        _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) - 2) & 0xffff;
        stack--;

        stack[0] = (u_int16_t) _current->md.md_tss.eflags;
        if (_current->oInfo.v86If == TRUE)
          stack[0] |= EFLAG_IF;
        else
          stack[0] &= ~EFLAG_IF;
        _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);

      }
    break;
    case 0x9D:
      if (isOperand32 == TRUE) {
        _current->md.md_tss.eflags = EFLAG_IF | EFLAG_VM | (stack32[0] & 0xDFF);
        _current->oInfo.v86If = (stack32[0] & EFLAG_IF) != 0;
        _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) + 4) & 0xffff;
      }
      else {
        _current->md.md_tss.eflags = EFLAG_IF | EFLAG_VM | stack[0];
        _current->oInfo.v86If = (stack[0] & EFLAG_IF) != 0;
        _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) + 2) & 0xffff;
      }
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
    break;
    case 0xFA:
      _current->oInfo.v86If = FALSE;
      _current->md.md_tss.eflags &= ~EFLAG_IF;
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
      _current->oInfo.timer = 0x1;
    break;
    case 0xFB:
      _current->oInfo.v86If = TRUE;
      _current->md.md_tss.eflags |= EFLAG_IF;
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
      _current->oInfo.timer = 0x0;
      /* kprintf("sti [0x%X]\n",_current->id); */
    break;
    case 0xCF:
      if (isOperand32 == TRUE) { /* IRETD: pops 32-bit EIP, CS, EFLAGS */
        _current->md.md_tss.eip = stack32[0];
        _current->md.md_tss.cs  = stack32[1] & 0xFFFF;
        _current->md.md_tss.eflags = EFLAG_IF | EFLAG_VM | (stack32[2] & 0xDFFF);
        _current->oInfo.v86If = (stack32[2] & EFLAG_IF) != 0;
        _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) + 12) & 0xffff;
      } else {
        _current->md.md_tss.eip = stack[0];
        _current->md.md_tss.cs  = stack[1];
        _current->md.md_tss.eflags = EFLAG_IF | EFLAG_VM | stack[2];
        _current->oInfo.v86If = (stack[2] & EFLAG_IF) != 0;
        _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) + 6) & 0xffff;
      }
    break;
    case 0xC3: /* RET near */
      _current->md.md_tss.eip = stack[0];
      _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) + 2) & 0xffff;
    break;
    case 0xC2: /* RET near imm16 */
      _current->md.md_tss.eip = stack[0];
      _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) + 2 + ip[1] + (ip[2] << 8)) & 0xffff;
    break;
    case 0xCB: /* RETF */
      _current->md.md_tss.eip = stack[0];
      _current->md.md_tss.cs  = stack[1];
      _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) + 4) & 0xffff;
    break;
    case 0xCA: /* RETF imm16 */
      _current->md.md_tss.eip = stack[0];
      _current->md.md_tss.cs  = stack[1];
      _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) + 4 + ip[1] + (ip[2] << 8)) & 0xffff;
    break;
    case 0x9A: /* CALLF ptr16:16 — direct far call */
      _current->md.md_tss.esp = ((_current->md.md_tss.esp & 0xffff) - 4) & 0xffff;
      stack -= 2;
      stack[0] = (u_int16_t)(_current->md.md_tss.eip + 5);
      stack[1] = (u_int16_t)(_current->md.md_tss.cs);
      _current->md.md_tss.cs  = ip[3] | ((u_int16_t)ip[4] << 8);
      _current->md.md_tss.eip = ip[1] | ((u_int16_t)ip[2] << 8);
    break;
    case 0xE4: /* IN AL, imm8 */
      _current->md.md_tss.eax = (_current->md.md_tss.eax & ~0xFF) | inportByte(ip[1]);
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 2);
    break;
    case 0xE5: /* IN AX, imm8 */
      _current->md.md_tss.eax = (_current->md.md_tss.eax & ~0xFFFF) | inportWord(ip[1]);
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 2);
    break;
    case 0xE6: /* OUT imm8, AL */
      outportByte(ip[1], _current->md.md_tss.eax & 0xFF);
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 2);
    break;
    case 0xE7: /* OUT imm8, AX */
      outportWord(ip[1], _current->md.md_tss.eax & 0xFFFF);
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 2);
    break;
    case 0xEC: /* IN AL,DX */
      _current->md.md_tss.eax = (_current->md.md_tss.eax & ~0xFF) | inportByte(_current->md.md_tss.edx);
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
    break;
    case 0xED: /* IN AX,DX */
      _current->md.md_tss.eax = (_current->md.md_tss.eax & ~0xFFFF) | inportWord(_current->md.md_tss.edx);
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
    break;
    case 0xEE: /* OUT DX,AL */
      outportByte(_current->md.md_tss.edx, _current->md.md_tss.eax & 0xFF);
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
    break;
    case 0xEF:
      outportWord(_current->md.md_tss.edx, _current->md.md_tss.eax);
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
    break;
    case 0x6C: /* INSB: IN byte from DX into ES:[DI] */
    {
      u_int8_t *dst = (u_int8_t *)FP_TO_LINEAR(_current->md.md_tss.es, _current->md.md_tss.edi & 0xFFFF);
      *dst = inportByte(_current->md.md_tss.edx & 0xFFFF);
      _current->md.md_tss.edi = (_current->md.md_tss.edi + 1) & 0xFFFF;
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 1);
    }
    break;
    case 0x6D: /* INSW: IN word from DX into ES:[DI] */
    {
      u_int16_t *dst = (u_int16_t *)FP_TO_LINEAR(_current->md.md_tss.es, _current->md.md_tss.edi & 0xFFFF);
      *dst = inportWord(_current->md.md_tss.edx & 0xFFFF);
      _current->md.md_tss.edi = (_current->md.md_tss.edi + 2) & 0xFFFF;
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 1);
    }
    break;
    case 0x6E: /* OUTSB: OUT byte from DS:[SI] to DX */
    {
      u_int8_t *src = (u_int8_t *)FP_TO_LINEAR(_current->md.md_tss.ds, _current->md.md_tss.esi & 0xFFFF);
      outportByte(_current->md.md_tss.edx & 0xFFFF, *src);
      _current->md.md_tss.esi = (_current->md.md_tss.esi + 1) & 0xFFFF;
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 1);
    }
    break;
    case 0x6F: /* OUTSW: OUT word from DS:[SI] to DX */
    {
      u_int16_t *src = (u_int16_t *)FP_TO_LINEAR(_current->md.md_tss.ds, _current->md.md_tss.esi & 0xFFFF);
      outportWord(_current->md.md_tss.edx & 0xFFFF, *src);
      _current->md.md_tss.esi = (_current->md.md_tss.esi + 2) & 0xFFFF;
      _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 1);
    }
    break;
    case 0xF3: /* REP prefix — only privileged for REP INS/OUTS */
    {
      u_int8_t next = ip[1];
      u_int16_t cx = _current->md.md_tss.ecx & 0xFFFF;
      u_int16_t dx = _current->md.md_tss.edx & 0xFFFF;
      if (next == 0x6C) { /* REP INSB */
        while (cx--) {
          u_int8_t *dst = (u_int8_t *)FP_TO_LINEAR(_current->md.md_tss.es, _current->md.md_tss.edi & 0xFFFF);
          *dst = inportByte(dx);
          _current->md.md_tss.edi = (_current->md.md_tss.edi + 1) & 0xFFFF;
        }
        _current->md.md_tss.ecx = 0;
        _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 2);
      } else if (next == 0x6D) { /* REP INSW */
        while (cx--) {
          u_int16_t *dst = (u_int16_t *)FP_TO_LINEAR(_current->md.md_tss.es, _current->md.md_tss.edi & 0xFFFF);
          *dst = inportWord(dx);
          _current->md.md_tss.edi = (_current->md.md_tss.edi + 2) & 0xFFFF;
        }
        _current->md.md_tss.ecx = 0;
        _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 2);
      } else if (next == 0x6E) { /* REP OUTSB */
        while (cx--) {
          u_int8_t *src = (u_int8_t *)FP_TO_LINEAR(_current->md.md_tss.ds, _current->md.md_tss.esi & 0xFFFF);
          outportByte(dx, *src);
          _current->md.md_tss.esi = (_current->md.md_tss.esi + 1) & 0xFFFF;
        }
        _current->md.md_tss.ecx = 0;
        _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 2);
      } else if (next == 0x6F) { /* REP OUTSW */
        while (cx--) {
          u_int16_t *src = (u_int16_t *)FP_TO_LINEAR(_current->md.md_tss.ds, _current->md.md_tss.esi & 0xFFFF);
          outportWord(dx, *src);
          _current->md.md_tss.esi = (_current->md.md_tss.esi + 2) & 0xFFFF;
        }
        _current->md.md_tss.ecx = 0;
        _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 2);
      } else {
        /* REP + non-privileged string op: shouldn't GPF, skip REP and retry */
        ip++;
        _current->md.md_tss.eip = (u_int16_t)(_current->md.md_tss.eip + 1);
        goto gpfStart;
      }
    }
    break;
    case 0xF4:
      _current->md.md_tss.eip = (u_int16_t) (_current->md.md_tss.eip + 1);
    break;
    default: /* something wrong */
      kprintf("GPF unhandled op=0x%X [%02X %02X %02X %02X] at 0x%X:0x%X esp=0x%X:0x%X id=%d\n",
        ip[0], ip[1], ip[2], ip[3], ip[4],
        (u_int16_t)_current->md.md_tss.cs, _current->md.md_tss.eip,
        (u_int16_t)_current->md.md_tss.ss, (u_int16_t)_current->md.md_tss.esp,
        _current->id);
      sched_dead(_current);
    break;
  }

gpfDone:
  if (_current->state == DEAD) {
    /* v86 task finished (INT 0x69), timed out, or hit an unhandled op: restore
     * the timer (masked by sched on the v86 switch) and switch away.  The dead
     * task never resumes, so this does not return. */
    irqEnable(0);
    sched_yield();
    return;
  }

  /* Sync the virtualized (advanced) state back to the trapframe so the iret in
   * _gpf resumes VM86 at the new cs:eip with the updated registers. */
  frame->tf_eip    = _current->md.md_tss.eip;
  frame->tf_cs     = (u_int16_t)_current->md.md_tss.cs;
  frame->tf_eflags = _current->md.md_tss.eflags;
  frame->tf_esp    = _current->md.md_tss.esp;
  frame->tf_ss     = (u_int16_t)_current->md.md_tss.ss;
  vsegs[1]         = (u_int16_t)_current->md.md_tss.es;
  vsegs[2]         = (u_int16_t)_current->md.md_tss.ds;
  vsegs[3]         = (u_int16_t)_current->md.md_tss.fs;
  vsegs[4]         = (u_int16_t)_current->md.md_tss.gs;
  frame->tf_eax    = _current->md.md_tss.eax;
  frame->tf_ecx    = _current->md.md_tss.ecx;
  frame->tf_edx    = _current->md.md_tss.edx;
  frame->tf_ebx    = _current->md.md_tss.ebx;
  frame->tf_esi    = _current->md.md_tss.esi;
  frame->tf_edi    = _current->md.md_tss.edi;
  frame->tf_ebp    = _current->md.md_tss.ebp;
}

/*
 * _gpf — #GP (vector 13) interrupt-gate entry (no longer a hardware task gate).
 * The CPU pushed an error code; we add a trapno and build a struct trapframe,
 * load kernel data segments (the faulting VM86 context's segments are real-mode
 * and unusable for kernel memory access), call __gpf to virtualize the BIOS
 * instruction, then iret back to VM86.
 */
asm(
  ".globl _gpf       \n"
  "_gpf:             \n"
  "  cli             \n"
  "  pushl $0x13     \n" /* tf_trapno (CPU already pushed tf_err) */
  "  pushal          \n"
  "  push %ds        \n"
  "  push %es        \n"
  "  push %fs        \n"
  "  push %gs        \n"
  "  mov $0x10,%eax  \n" /* kernel data segs for the C handler */
  "  mov %eax,%ds    \n"
  "  mov %eax,%es    \n"
  "  mov %eax,%fs    \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp       \n"
  "  call __gpf      \n"
  "  add $0x4,%esp   \n"
  "  pop %gs         \n"
  "  pop %fs         \n"
  "  pop %es         \n"
  "  pop %ds         \n"
  "  popal           \n"
  "  add $8,%esp     \n" /* discard tf_trapno + tf_err */
  "  iret            \n"
);


void __floatingPoint(struct trapframe *frame) {
  die_if_kernel("floating point", frame, 0x10);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _floatingPoint \n"
  "_floatingPoint:       \n"
  "  pushl $0x0 \n"
  "  pushl $0x10 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call __floatingPoint  \n"
  "  pop %gs      \n"
  "  pop %fs      \n"
  "  pop %es      \n"
  "  pop %ds      \n"
  "  popal        \n"
  "  iret         \n"
);

void __alignmentCheck(struct trapframe *frame) {
  die_if_kernel("alignment check", frame, 0x11);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _alignmentCheck \n"
  "_alignmentCheck:       \n"
  "  pushl $0x11 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call __alignmentCheck  \n"
  "  pop %gs      \n"
  "  pop %fs      \n"
  "  pop %es      \n"
  "  pop %ds      \n"
  "  popal        \n"
  "  iret         \n"
);

void __machineCheck(struct trapframe *frame) {
  die_if_kernel("machine check", frame, 0x12);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _machineCheck \n"
  "_machineCheck:       \n"
   " pushl $0x0\n"
  "  pushl $0x12 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call __machineCheck  \n"
  "  pop %gs      \n"
  "  pop %fs      \n"
  "  pop %es      \n"
  "  pop %ds      \n"
  "  popal        \n"
  "  iret         \n"
);

void __simd(struct trapframe *frame) {
  die_if_kernel("simd", frame, 0x13);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _simd \n"
  "_simd:       \n"
  "  iret\n"
  "  pushl $0x0\n"
  "  pushl $0x13 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call __simd  \n"
  "  pop %gs      \n"
  "  pop %fs      \n"
  "  pop %es      \n"
  "  pop %ds      \n"
  "  popal        \n"
  "  iret         \n"
);

void __virtualization(struct trapframe *frame) {
  die_if_kernel("virtualization", frame, 0x14);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _virtualization \n"
  "_virtualization:       \n"
  "  pushl $0x0  \n"
  "  pushl $0x14 \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call __virtualization  \n"
  "  pop %gs      \n"
  "  pop %fs      \n"
  "  pop %es      \n"
  "  pop %ds      \n"
  "  popal        \n"
  "  iret         \n"
);

void __security(struct trapframe *frame) {
  u_int32_t rawESP;
  asm volatile("mov %%esp, %0" : "=r"(rawESP));
  kprintf("SECURITY: frame=%p rawESP=0x%X v86=%d cs=0x%X eip=0x%X esp0=0x%X\n",
      frame, rawESP,
      (_current && _current->oInfo.v86Task) ? 1 : 0,
      _current ? _current->md.md_tss.cs  : 0xDEAD,
      _current ? _current->md.md_tss.eip : 0xDEAD,
      _current ? _current->md.md_tss.esp0 : 0xDEAD);
  kprintf("SECURITY: tf_eip=0x%X tf_cs=0x%X tf_eflags=0x%X tf_esp=0x%X tf_ss=0x%X tf_err=0x%X\n",
      frame ? frame->tf_eip : 0, frame ? frame->tf_cs : 0,
      frame ? frame->tf_eflags : 0, frame ? frame->tf_esp : 0,
      frame ? frame->tf_ss : 0, frame ? frame->tf_err : 0);
  die_if_kernel("security exception", frame, 0x1E);
  endTask(_current->id);
  sched_yield();
}

asm(
  ".globl _security \n"
  "_security:       \n"
  "  pushl $0x1E \n"
  "  pushal     \n" /* Save all registers */
  "  push %ds   \n"
  "  push %es   \n"
  "  push %fs   \n"
  "  push %gs   \n"
  ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU for per-CPU data access */
  "  push %esp  \n"
  "  call __security\n"
  "  pop %gs      \n"
  "  pop %fs      \n"
  "  pop %es      \n"
  "  pop %ds      \n"
  "  popal        \n"
  "  iret         \n"
);

/* Removed static however this is the only place it's called from */
/* Removed static however this is the only place it's called from */
void mathStateRestore() {
  if (_usedMath != 0x0) {
    asm(
      "fnsave %0"
      :
      : "m" (_usedMath->md.md_i387)
    );
  }
  if (_current->usedMath != 0x0) {
    asm(
      "frstor %0"
      :
      : "m" (_current->md.md_i387)
    );
  }
  else {
    asm("fninit");
    _current->usedMath = 0x1;
  }

  _usedMath = _current;

  //Return
}


/* _int7 (device-not-available) — lazy FPU state restore. */
asm(
  ".globl _int7              \n"
  "_int7:                    \n"
  "  pushl %eax              \n"
  "  pushl %gs               \n" /* save interrupted %gs (may be user TLS)      */
  ASM_PCPU_LOAD_GS              /* %gs = SEL_PCPU: %gs:8 == _current for the    */
                               /* read below and for mathStateRestore          */
  "  clts                    \n"
  "  movl %gs:8, %eax        \n" /* _current */
  "  cmpl _usedMath,%eax     \n"
  "  je mathDone             \n"
  "  call mathStateRestore   \n"
  "mathDone:                 \n"
  "  popl %gs                \n" /* restore interrupted %gs */
  "  popl %eax               \n"
  "  iret                    \n"
);
