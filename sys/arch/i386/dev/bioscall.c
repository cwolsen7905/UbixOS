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

#include <sys/_null.h>
#include <sys/tss.h>
#include <ubixos/sched.h>
#include <vmm/vmm.h>
#include <lib/bioscall.h>
#include <string.h>
#include <sys/video.h>
#include <assert.h>
#include <lib/kprintf.h>
#include <vmm/paging.h>

void biosCallEx(int biosInt, int eax, int ebx, int ecx, int edx, int esi, int edi, int es, int ds, struct biosRegs *out) {
  kTask_t *newProcess = 0x0;

  /*
   * QEMU's VGA BIOS leaves a 0x25 "mode-set-in-progress" marker at
   * physical 0xC01A4 after its own initialization.  The Bochs stdvga
   * GetModeInfo sub-routine spins waiting for this byte to become 0.
   * Clear it before every BIOS call so the BIOS doesn't loop forever.
   */
  *(volatile u_int8_t *)0xC01A4 = 0x0;

  /*
   * Build a per-call 7-byte 16-bit stub at physical 0x600 (free conventional
   * memory above the BDA).  bios16code.S has int $0x10 hardcoded; using a RAM
   * stub lets us issue any INT number without patching kernel text.
   *   CD <n>   — INT biosInt   (triggers GPF → __gpf emulates via IVT)
   *   CD 69    — INT 0x69      (completion signal → __gpf sets task DEAD)
   *   F4       — HLT           (idle while waiting to be reaped)
   *   EB FD    — JMP -3        (loop back to HLT)
   */
  {
    volatile u_int8_t *stub = (volatile u_int8_t *)0x600;
    stub[0] = 0xCD;
    stub[1] = (u_int8_t)(biosInt & 0xFF);
    stub[2] = 0xCD;
    stub[3] = 0x69;
    stub[4] = 0xF4;
    stub[5] = 0xEB;
    stub[6] = 0xFD;
  }

  newProcess = schedNewTask();
  assert(newProcess);

  newProcess->md.md_tss.back_link = 0x0;
  newProcess->md.md_tss.esp0 = (u_int32_t) vmm_get_free_kernel_page(newProcess->id, 2) + (0x2000 - 0x4);
  newProcess->md.md_tss.ss0 = 0x10;
  newProcess->md.md_tss.esp1 = 0x0;
  newProcess->md.md_tss.ss1 = 0x0;
  newProcess->md.md_tss.esp2 = 0x0;
  newProcess->md.md_tss.ss2 = 0x0;
  newProcess->md.md_tss.cr3 = (u_int32_t)kernelPageDirectory;
  newProcess->md.md_tss.eip = 0x0000;        /* offset within stub paragraph */
  newProcess->md.md_tss.eflags = 2 | EFLAG_IF | EFLAG_VM;
  newProcess->md.md_tss.eax = eax & 0xFFFF;
  newProcess->md.md_tss.ebx = ebx & 0xFFFF;
  newProcess->md.md_tss.ecx = ecx & 0xFFFF;
  newProcess->md.md_tss.edx = edx & 0xFFFF;
  newProcess->md.md_tss.esp = 0x1000 & 0xFFFF;
  newProcess->md.md_tss.ebp = 0x1000 & 0xFFFF;
  newProcess->md.md_tss.esi = esi & 0xFFFF;
  newProcess->md.md_tss.edi = edi & 0xFFFF;
  newProcess->md.md_tss.es = es & 0xFFFF;
  newProcess->md.md_tss.cs = 0x0060;         /* physical 0x600 = seg 0x60 * 16 */
  newProcess->md.md_tss.ss = 0x1000 & 0xFFFF;
  newProcess->md.md_tss.ds = ds & 0xFFFF;
  newProcess->md.md_tss.fs = 0x0;
  newProcess->md.md_tss.gs = 0x0;
  newProcess->md.md_tss.ldt = 0x0;
  newProcess->md.md_tss.trace_bitmap = 0x0;
  newProcess->md.md_tss.io_map = sizeof(struct tssStruct) - 8192;
  newProcess->oInfo.v86Task   = 0x1;
  newProcess->priority        = 23;  /* highest non-realtime: gets CPU over caller but yields properly */
  newProcess->base_priority   = 23;

  sched_ready(newProcess);

  while (newProcess->state > 0)
    sched_yield();

  if (out) {
    out->ax = newProcess->md.md_tss.eax & 0xFFFF;
    out->bx = newProcess->md.md_tss.ebx & 0xFFFF;
    out->cx = newProcess->md.md_tss.ecx & 0xFFFF;
    out->dx = newProcess->md.md_tss.edx & 0xFFFF;
    out->si = newProcess->md.md_tss.esi & 0xFFFF;
    out->di = newProcess->md.md_tss.edi & 0xFFFF;
    out->es = newProcess->md.md_tss.es & 0xFFFF;
    out->ds = newProcess->md.md_tss.ds & 0xFFFF;
  }
}

void biosCall(int biosInt, int eax, int ebx, int ecx, int edx, int esi, int edi, int es, int ds) {
  biosCallEx(biosInt, eax, ebx, ecx, edx, esi, edi, es, ds, 0x0);
}
