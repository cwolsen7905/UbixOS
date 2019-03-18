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

#include <ubixos/smp.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <lib/kprintf.h>
#include <lib/string.h>
#include <sys/io.h>

static struct spinLock initSpinLock = SPIN_LOCK_INITIALIZER;
static struct spinLock cpuInfoLock = SPIN_LOCK_INITIALIZER;
static uInt32 cpus = 0;
struct cpuinfo_t cpuinfo[8];

uInt8 kernel_function(void);
uInt8 *vram = (uInt8 *)0xB8000;

static inline unsigned int apicRead(address) {
  return *(volatile unsigned int *) (0xFEE00000 + address);
}

static inline void apicWrite(unsigned int address,unsigned int data) {
  *(volatile unsigned int *) (0xFEE00000 + address) = data;
}

static __inline__ void setDr3 (void *dr3) {
  register uInt32 value = (uInt32)dr3;
  __asm__ __volatile__ ("mov %0, %%dr3" :: "r" (value));
}

static __inline__ uInt32 getDr3 (void) {
  register uInt32 value;
  __asm__ __volatile__ ("mov %%dr3, %0" : "=r" (value));
  return value;
}

struct gdt_descr {
  uInt16 limit;
  uInt32 *base __attribute__ ((packed));
};

static void GDT_fixer() {
  struct gdt_descr gdt_descr;
  uInt32 *gdt = (uInt32 *)0x20000; // 128KB

  gdt[0] = 0;
  gdt[1] = 0;
  gdt[2] = 0x0000ffff;// seg 0x8  -- DPL 0 4GB code
  gdt[3] = 0x00cf9a00;
  gdt[4] = 0x0000ffff;// seg 0x10 -- DPL 0 4GB data
  gdt[5] = 0x00cf9200;
  gdt[6] = 0x0000ffff;// seg 0x1b -- DPL 3 4GB code
  gdt[7] = 0x00cffa00;
  gdt[8] = 0x0000ffff;// seg 0x23 -- DPL 3 4GB data
  gdt[9] = 0x00cff200;

  gdt_descr.limit = 32 * 4;
  gdt_descr.base = gdt;

  /*
   asm("lgdt %0;" : : "m" (gdt_descr));
   __asm__ __volatile__ ("ljmp %0,$1f; 1:" :: "i" (0x08));
   __asm__ __volatile__ ("movw %w0,%%ds" :: "r" (0x10));
   __asm__ __volatile__ ("movw %w0,%%es" :: "r" (0x10));
   __asm__ __volatile__ ("movw %w0,%%ss" :: "r" (0x10));
   */
}

void cpu0_thread(void) {
  for(;;) {
    vram[40+640] = kernel_function();
    vram[42+640]++;
  }
}
void cpu1_thread(void) {
  for(;;) {
    vram[60+640] = kernel_function();
    vram[62+640]++;
  }
}
void cpu2_thread(void) {
  for(;;) {
    vram[80+640] = kernel_function();
    vram[82+640]++;
  }
}
void cpu3_thread(void) {
  for(;;) {
    vram[100+640] = kernel_function();
    vram[102+640]++;
  }
}

static struct spinLock bkl = SPIN_LOCK_INITIALIZER;
uInt8 kernel_function(void) {
  struct cpuinfo_t *cpu;

  spinLock(&bkl);

  cpu = (struct cpuinfo_t *)getDr3();

  spinUnlock(&bkl);

  return('0' + cpu->id);
}

void c_ap_boot(void) {

  while(spinLockLocked(&initSpinLock));

  switch(cpuInfo()) {
    case 1:
    cpu1_thread();
    break;
    case 2:
    cpu2_thread();
    break;
    case 3:
    cpu3_thread();
    break;
  }

  outportByte(0xe9,'5');

  for(;;) {
    asm("nop");
  }
}

void smpInit() {
  spinLock(&initSpinLock);
  GDT_fixer();
  cpuidDetect();
  cpuInfo();
  apicMagic();
  spinUnlock(&initSpinLock);

  //cpu0_thread(); 

}

void cpuidDetect() {
  if (!(getEflags() & (1<<21)) ) {
    setEflags(getEflags() | (1<<21));
    if( !(getEflags() & (1<<21)) ) {
      kpanic("CPU doesn't support CPUID, get a newer machine\n");
    }
  }
}

uInt8 cpuInfo() {
  uInt32 data[4],i;

  if( !(getEflags() & (1<<21)) ) {       // If the cpuid bit in eflags not set..
    setEflags(getEflags() | (1<<21));// ..try and set it to see if it comes on..
    if( !(getEflags() & (1<<21)) ) {     // It didn't.. This CPU suck
      kpanic("CPU doesn't support CPUID, get a newer machine\n");
    }
  }

  spinLock(&cpuInfoLock);
  cpuinfo[cpus].ok = 1;
  cpuinfo[cpus].apic_id = apicRead(0x20) >> 24;
  cpuinfo[cpus].apic_ver = apicRead(0x30) & 0xFF;

  cpuid(0,data);
  *(uInt32 *)&cpuinfo[cpus].ident[0] = data[1];
  *(uInt32 *)&cpuinfo[cpus].ident[4] = data[3];
  *(uInt32 *)&cpuinfo[cpus].ident[8] = data[2];
  cpuinfo[cpus].ident[17] = 0;
  cpuinfo[cpus].max = data[0];

  cpuid(1,data);
  cpuinfo[cpus].signature = data[0];
  cpuinfo[cpus].feature = data[3];

  cpuid(0x80000000,data);
  if(data[0]>=0x80000004) {
    for(i=0;i<3;i++) {
      cpuid(0x80000002 + i,data);

      *(unsigned int *)&cpuinfo[cpus].brand[16*i+0] = data[0];
      *(unsigned int *)&cpuinfo[cpus].brand[16*i+4] = data[1];
      *(unsigned int *)&cpuinfo[cpus].brand[16*i+8] = data[2];
      *(unsigned int *)&cpuinfo[cpus].brand[16*i+12] = data[3];
    }
    cpuinfo[cpus].brand[48] = 0;
  }
  else {
    cpuinfo[cpus].brand[0] = 0;
  }

  setDr3(&cpuinfo[cpus]); // DR3 always points to the cpu-struct for that CPU (should be thread-struct of current thread)
  cpuinfo[cpus].id = cpus;

  cpus++;

  spinUnlock(&cpuInfoLock);

  return(cpus - 1);
}

extern void ap_trampoline_start(),
ap_trampoline_end();
void apicMagic(void) {
  uInt32 tmp;

  kprintf("Copying %u bytes from 0x%x to 0x00\n", ap_trampoline_end - ap_trampoline_start, ap_trampoline_start);
  memcpy(0x0, (char *) ap_trampoline_start, ap_trampoline_end - ap_trampoline_start);
  apicWrite(0x280, 0);
  apicRead(0x280);

  apicWrite(0x300, 0x000C4500);  // INIT IPI to all CPUs
  for (tmp = 0; tmp < 800000; tmp++)
    asm("nop");
  // Sleep a little (should be 10ms)
  apicWrite(0x300, 0x000C4600);  // INIT SIPI to all CPUs
  for (tmp = 0; tmp < 800000; tmp++)
    asm("nop");
  // Sleep a little (should be 200ms)
  apicWrite(0x300, 0x000C4600);  // Second INIT SIPI
  for (tmp = 0; tmp < 800000; tmp++)
    asm("nop");
  // Sleep a little (should be 200ms)
}

uInt32 getEflags() {
  uInt32 eflags = 0x0;
  asm(
    "pushfl     \n"
    "popl %%eax \n"
    : "=a" (eflags)
  );
  return (eflags);
}

void setEflags(uInt32 eflags) {
  asm(
    "pushl %%eax \n"
    "popfl       \n"
    :
    : "a" (eflags)
  );
}

asm(
  ".globl cpuid            \n"
  "cpuid:                  \n"
  "  pushl   %ebx          \n"
  "  pushl   %edi          \n"
  "  movl    12(%esp),%eax \n"
  "  movl    16(%esp),%edi \n"
  "  cpuid                 \n"
  "  movl    %eax,0(%edi)  \n"
  "  movl    %ebx,4(%edi)  \n"
  "  movl    %ecx,8(%edi)  \n"
  "  movl    %edx,12(%edi) \n"
  "  popl    %edi          \n"
  "  popl    %ebx          \n"
  "  ret                   \n"
);

/***
 END
 ***/

