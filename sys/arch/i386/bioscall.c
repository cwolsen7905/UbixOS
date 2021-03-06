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

void biosCall(int biosInt, int eax, int ebx, int ecx, int edx, int esi, int edi, int es, int ds) {
  short segment = 0x0, offset = 0x0;
  uint32_t tmpAddr = (uint32_t) &bios16Code;
  kTask_t *newProcess = 0x0;

  offset = tmpAddr & 0xF;  // lower 4 bits
  segment = tmpAddr >> 4;

  newProcess = schedNewTask();
  assert(newProcess);

  newProcess->tss.back_link = 0x0;
  newProcess->tss.esp0 = (uint32_t) vmm_getFreeKernelPage(newProcess->id, 2) + (0x2000 - 0x4); // XXX I had 0xDEADBEEF I'm not sure why
  newProcess->tss.ss0 = 0x10;
  newProcess->tss.esp1 = 0x0;
  newProcess->tss.ss1 = 0x0;
  newProcess->tss.esp2 = 0x0;
  newProcess->tss.ss2 = 0x0;
  newProcess->tss.cr3 = kernelPageDirectory; //vmm_createVirtualSpace(newProcess->id); //(uint32_t)_current->tss.cr3;
  newProcess->tss.eip = offset & 0xFFFF;
  newProcess->tss.eflags = 2 | EFLAG_IF | EFLAG_VM;
  newProcess->tss.eax = eax & 0xFFFF;
  newProcess->tss.ebx = ebx & 0xFFFF;
  newProcess->tss.ecx = ecx & 0xFFFF;
  newProcess->tss.edx = edx & 0xFFFF;
  newProcess->tss.esp = 0x1000 & 0xFFFF;
  newProcess->tss.ebp = 0x1000 & 0xFFFF;
  newProcess->tss.esi = esi & 0xFFFF;
  newProcess->tss.edi = edi & 0xFFFF;
  newProcess->tss.es = es & 0xFFFF;
  newProcess->tss.cs = segment & 0xFFFF;
  newProcess->tss.ss = 0x1000 & 0xFFFF;
  newProcess->tss.ds = ds & 0xFFFF;
  newProcess->tss.fs = 0x0 & 0xFFFF;
  newProcess->tss.gs = 0x0 & 0xFFFF;
  newProcess->tss.ldt = 0x0 & 0xFFFF;
  newProcess->tss.trace_bitmap = 0x0 & 0xFFFF;
  newProcess->tss.io_map = 0x0 & 0xFFFF;
  newProcess->tss.io_map = sizeof(struct tssStruct) - 8192;
  newProcess->oInfo.v86Task = 0x1;

  kprintf("EIP: [0x%X] 0x%X:0x%X", tmpAddr, newProcess->tss.eip, newProcess->tss.cs);

  newProcess->state = READY;
  while (newProcess->state > 0)
    sched_yield();

  kprintf("EIP: [0x%X] 0x%X:0x%X!", tmpAddr, newProcess->tss.eip, newProcess->tss.cs);
  kprintf("CALL DONE: %i 0x%X 0x%X!", newProcess->state, newProcess->tss.esp, newProcess->tss.ss);

  return;
}
