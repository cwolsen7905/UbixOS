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

#include <ubixos/fork.h>
#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>
#include <ubixos/vitals.h>
#include <vmm/vmm.h>
#include <string.h>
#include <assert.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/descrip.h>
#include <sys/pipe.h>
#include <sys/descrip.h>

/**
 * rfork(RFMEM)-style thread creation — the kernel side of pthread_create().
 *
 * Unlike fork(), the new task SHARES the caller's address space (same page
 * directory / cr3) instead of copying it, runs on a caller-supplied user
 * stack, and resumes at the same instruction (so musl's clone.s child path
 * runs with eax==0 and calls the thread function).  It joins the caller's
 * thread group (tgid) so endTask() tears the shared address space down only
 * when the last task of the group exits.
 *
 * Args (uBixOS stack ABI, pushed by musl clone.s):
 *   flags  — rfork flags (RFMEM etc.); informational for v1 (we always share).
 *   stack  — top of the new thread's user stack.
 *   tls    — thread-local-storage base (wired into %gs in the TLS step; stored
 *            for now).
 *
 * @return the new thread's tid to the caller; the new thread sees 0.
 */
int sys_rfork(struct thread *td, struct sys_rfork_args *args) {
  struct taskStruct *newThread;
  int i;

  newThread = schedNewTask();

  /* Inherit QoS floor, cwd, identity, session/tty from the caller. */
  newThread->base_priority = _current->base_priority;
  newThread->priority      = _current->base_priority;
  memcpy(newThread->oInfo.cwd, _current->oInfo.cwd, 1024);
  newThread->ppid   = _current->id;
  newThread->pgrp   = _current->pgrp;
  newThread->sid    = _current->sid;
  newThread->ct_tty = _current->ct_tty;
  newThread->term   = _current->term;
  newThread->uid    = _current->uid;
  newThread->gid    = _current->gid;

  /* Same thread group → shared address space; endTask uses this to decide
   * whether it is the last task and must free the address space. */
  newThread->tgid = _current->tgid;

  /*
   * Shallow-share the fd table: free the placeholders schedNewTask() created
   * for 0–2, then point at the caller's fd objects so threads share open
   * files (v1; a proper shared fdtable is the deferred cleanup).  endTask
   * must NOT free these for a non-last thread (guarded by tgid there).
   */
  for (i = 0; i < O_FILES; i++) {
    if (newThread->td.o_files[i] != NULL) {
      kfree(newThread->td.o_files[i]);
      newThread->td.o_files[i] = NULL;
    }
    newThread->td.o_files[i] = td->o_files[i];
  }

  /*
   * Register state: copy the caller's frame, then override for a new thread —
   * its own user stack, the clone() return convention (eax==0 in the child),
   * and the same code address (resume right after the int $0x80 in clone.s).
   */
  newThread->md.md_tss.eip      = td->frame->tf_eip;
  newThread->md.md_tss.eflags   = td->frame->tf_eflags;
  newThread->md.md_tss.eax      = 0; /* the new thread sees rfork()/clone() == 0 */
  newThread->md.md_tss.ebx      = td->frame->tf_ebx;
  newThread->md.md_tss.ecx      = td->frame->tf_ecx;
  newThread->md.md_tss.edx      = td->frame->tf_edx;
  newThread->md.md_tss.esi      = td->frame->tf_esi;
  newThread->md.md_tss.edi      = td->frame->tf_edi;
  newThread->md.md_tss.ebp      = 0; /* fresh frame base on the new stack */
  newThread->md.md_tss.esp      = (u_int32_t)args->stack;
  newThread->md.md_tss.cs       = td->frame->tf_cs;
  newThread->md.md_tss.ss       = td->frame->tf_ss;
  newThread->md.md_tss.ds       = td->frame->tf_ds;
  newThread->md.md_tss.es       = td->frame->tf_es;
  newThread->md.md_tss.fs       = td->frame->tf_fs;
  newThread->md.md_tss.gs       = _current->md.md_tss.gs & 0xFF; /* TLS %gs set in the TLS step */
  newThread->md.md_tss.ldt      = 0x18;
  newThread->md.md_tss.back_link = 0x0;
  newThread->md.md_tss.trace_bitmap = 0x0000;
  newThread->md.md_tss.io_map  = 0x8000;

  newThread->td.vm_tsize = _current->td.vm_tsize;
  newThread->td.vm_taddr = _current->td.vm_taddr;
  newThread->td.vm_dsize = _current->td.vm_dsize;
  newThread->td.vm_daddr = _current->td.vm_daddr;

  /* SHARE the address space — same page directory as the caller (do NOT copy). */
  newThread->md.md_tss.cr3 = _current->md.md_tss.cr3;
  /* v1: copy the VMA map; a shared vm_map (cross-thread mmap/demand-zero) is a
   * follow-up that touches the page-fault/mmap path. */
  memset(&newThread->vm_map, 0, sizeof(newThread->vm_map));
  vm_map_copy(&newThread->vm_map, &_current->vm_map);

  /* Signals: inherit dispositions + mask, no pending signals at start. */
  newThread->td.sig_pending = 0;
  memset(newThread->td.sig_code,  0, sizeof(newThread->td.sig_code));
  memset(newThread->td.sig_extra, 0, sizeof(newThread->td.sig_extra));
  memcpy(newThread->td.sigact, td->sigact, sizeof(newThread->td.sigact));
  memcpy(&newThread->td.sigmask, &td->sigmask, sizeof(newThread->td.sigmask));

  newThread->parent = _current;
  _current->children++;

  sched_ready(newThread);

  td->td_retval[0] = newThread->id; /* caller gets the new thread's tid */
  return (0);
}

int sys_fork(struct thread *td, struct sys_fork_args *args) {
  struct taskStruct *newProcess;

  newProcess = schedNewTask();

  /*
   * Inherit QoS class from parent: fork propagates the base_priority floor
   * (process-level QoS) but starts at that floor, not the parent's current
   * (possibly temporarily boosted) priority.
   */
  newProcess->base_priority = _current->base_priority;
  newProcess->priority      = _current->base_priority;

  /*
   * Initalize New Task Information From Parrent
   */

  /* Set CWD */
  memcpy(newProcess->oInfo.cwd, _current->oInfo.cwd, 1024);

  /* Set PPID */
  newProcess->ppid = _current->id;

  /* Set PGRP, SID, and controlling terminal (inherited from parent) */
  newProcess->pgrp   = _current->pgrp;
  newProcess->sid    = _current->sid;
  newProcess->ct_tty = _current->ct_tty;

  /* Copy File Descriptor Table */
  //memcpy(newProcess->files, _current->files, sizeof(fileDescriptor_t *) * MAX_OFILES);

  /* Free the placeholder fds schedNewTask allocated for slots 0-2 */
  for (int i = 0; i < 3; i++) {
    if (newProcess->td.o_files[i]) {
      kfree(newProcess->td.o_files[i]);
      newProcess->td.o_files[i] = NULL;
    }
  }

  /* Inherit all fds from parent (including stdin/stdout/stderr) */
  for (int i = 0; i < O_FILES; i++)
    if (td->o_files[i]) {
      struct file *parent_f = (struct file *)td->o_files[i];
      newProcess->td.o_files[i] = (struct file *)kmalloc(sizeof(struct file));
      memcpy(newProcess->td.o_files[i], parent_f, sizeof(struct file));
      if (parent_f->fd) {
        ((struct file *)newProcess->td.o_files[i])->fd = kmalloc(sizeof(fileDescriptor_t));
        memcpy(((struct file *)newProcess->td.o_files[i])->fd, parent_f->fd, sizeof(fileDescriptor_t));
        if (parent_f->fd->buffer) {
          ((struct file *)newProcess->td.o_files[i])->fd->buffer = kmalloc(4096);
          memcpy(((struct file *)newProcess->td.o_files[i])->fd->buffer, parent_f->fd->buffer, 4096);
        }
      }
      /* Pipes share their pipeInfo across processes; bump the matching
       * refcount so neither end is freed while the child still references it. */
      if (parent_f->fd_type == FD_TYPE_PIPE && parent_f->data != NULL) {
        struct pipeInfo *pi = (struct pipeInfo *)parent_f->data;
        if (parent_f->pipe_end == PIPE_END_READ)
          pi->rfdCNT++;
        else if (parent_f->pipe_end == PIPE_END_WRITE)
          pi->wfdCNT++;
      }
    }

  /* Set Up Task State */
  newProcess->md.md_tss.eip = td->frame->tf_eip;
  newProcess->oInfo.vmStart = _current->oInfo.vmStart;
  newProcess->term = _current->term;
  if (_current->term != NULL && _current->term->owner == _current->id)
    _current->term->owner = newProcess->id;
  newProcess->uid = _current->uid;
  newProcess->gid = _current->gid;
  newProcess->md.md_tss.back_link = 0x0;
  newProcess->md.md_tss.esp1 = 0x0;
  newProcess->md.md_tss.ss1 = 0x0;
  newProcess->md.md_tss.esp2 = 0x0;
  newProcess->md.md_tss.ss2 = 0x0;
  newProcess->md.md_tss.eflags = td->frame->tf_eflags;
  newProcess->md.md_tss.eax = 0x0;
  newProcess->md.md_tss.ebx = td->frame->tf_ebx;
  newProcess->md.md_tss.ecx = td->frame->tf_ecx;
  newProcess->md.md_tss.edx = td->frame->tf_edx;
  newProcess->md.md_tss.esi = td->frame->tf_esi;
  newProcess->md.md_tss.edi = td->frame->tf_edi;
  newProcess->md.md_tss.ebp = td->frame->tf_ebp;
  newProcess->md.md_tss.esp = td->frame->tf_esp;
  newProcess->md.md_tss.cs = td->frame->tf_cs; // & 0xFF;
  newProcess->md.md_tss.ss = td->frame->tf_ss; // & 0xFF;
  newProcess->md.md_tss.ds = td->frame->tf_ds; //_current->md.md_tss.ds & 0xFF;
  newProcess->md.md_tss.fs = td->frame->tf_fs; //_current->md.md_tss.fs & 0xFF;
  newProcess->md.md_tss.gs = _current->md.md_tss.gs & 0xFF;
  newProcess->md.md_tss.es = td->frame->tf_es; //_current->md.md_tss.es & 0xFF;
  newProcess->md.md_tss.ldt = 0x18;
  newProcess->md.md_tss.trace_bitmap = 0x0000;
  newProcess->md.md_tss.io_map = 0x8000;

  newProcess->td.vm_tsize = _current->td.vm_tsize;
  newProcess->td.vm_taddr = _current->td.vm_taddr;
  newProcess->td.vm_dsize = _current->td.vm_dsize;
  newProcess->td.vm_daddr = _current->td.vm_daddr;

  newProcess->md.md_tss.cr3 = (u_int32_t) vmm_copy_virtual_space(newProcess->id);
  memset(&newProcess->vm_map, 0, sizeof(newProcess->vm_map));
  vm_map_copy(&newProcess->vm_map, &_current->vm_map);

  /*
   * Inherit the userland TLS base: the child got a private copy of the LDT page
   * (vmm_copy_virtual_space) with the parent's TLS descriptor, and the same %gs.
   * Copying tls_base too keeps cpu_switch re-installing the right base if this
   * child later becomes multithreaded (a sibling thread would otherwise clobber
   * the shared LDT[1] base with no record to restore it).
   */
  newProcess->tls_base = _current->tls_base;

  /*
   * Signal state must be cleared AFTER vmm_copy_virtual_space.
   * vmm_get_free_kernel_page and the parent's kernel heap share the same VA
   * range (VMM_KERN_START..VMM_KERN_END).  A temporary double-mapping of
   * the physical page backing sig_pending can occur during the COW walk,
   * causing sig_pending to be overwritten with garbage.  Zeroing here
   * guarantees a clean slate regardless of what vmm_copy_virtual_space did.
   */
  /*
   * Clear delivery state (pending signals, queued info) — POSIX requires
   * the child start with no pending signals.  Signal dispositions (sigact)
   * and the signal mask are inherited, not cleared.
   */
  newProcess->td.sig_pending = 0;
  memset(newProcess->td.sig_code,  0, sizeof(newProcess->td.sig_code));
  memset(newProcess->td.sig_extra, 0, sizeof(newProcess->td.sig_extra));
  memcpy(newProcess->td.sigact, td->sigact, sizeof(newProcess->td.sigact));
  memcpy(&newProcess->td.sigmask, &td->sigmask, sizeof(newProcess->td.sigmask));

  newProcess->parent = _current;
  _current->children++;

  sched_ready(newProcess);

  /* Return Id of Proccess */
  td->td_retval[0] = newProcess->id;
  return (0);

}

/*****************************************************************************************
 Functoin: static int fork_copyProcess(struct taskStruct *newProcess,long ebp,long edi,
 long esi, long none,long ebx,long ecx,long edx,long eip,long cs,long eflags,
 long esp,long ss)

 Desc: This function will copy a process

 Notes:

 *****************************************************************************************/

/* Had to remove static though tihs function is only used in this file */
int fork_copyProcess(struct taskStruct *newProcess, long ebp, long edi, long esi, long none, long ebx, long ecx, long edx, long eip, long cs, long eflags, long esp, long ss) {
  assert(newProcess);
  assert(_current);

  /* Set Up New Tasks Information */
  memcpy(newProcess->oInfo.cwd, _current->oInfo.cwd, 1024);
  //kprintf( "Initializing New CWD!\n" );

  newProcess->md.md_tss.eip = eip;
  newProcess->oInfo.vmStart = _current->oInfo.vmStart;
  newProcess->term   = _current->term;
  newProcess->sid    = _current->sid;
  newProcess->ct_tty = _current->ct_tty;
  newProcess->uid = _current->uid;
  newProcess->gid = _current->gid;
  newProcess->md.md_tss.back_link = 0x0;
  newProcess->md.md_tss.esp0 = _current->md.md_tss.esp0;
  newProcess->md.md_tss.ss0 = 0x10;
  newProcess->md.md_tss.esp1 = 0x0;
  newProcess->md.md_tss.ss1 = 0x0;
  newProcess->md.md_tss.esp2 = 0x0;
  newProcess->md.md_tss.ss2 = 0x0;
  newProcess->md.md_tss.eflags = eflags;
  newProcess->md.md_tss.eax = 0x0;
  newProcess->md.md_tss.ebx = ebx;
  newProcess->md.md_tss.ecx = ecx;
  newProcess->md.md_tss.edx = edx;
  newProcess->md.md_tss.esi = esi;
  newProcess->md.md_tss.edi = edi;
  newProcess->md.md_tss.ebp = ebp;
  newProcess->md.md_tss.esp = esp;
  newProcess->md.md_tss.cs = cs & 0xFF;
  newProcess->md.md_tss.ss = ss & 0xFF;
  newProcess->md.md_tss.ds = _current->md.md_tss.ds & 0xFF;
  newProcess->md.md_tss.fs = _current->md.md_tss.fs & 0xFF;
  newProcess->md.md_tss.gs = _current->md.md_tss.gs & 0xFF;
  newProcess->md.md_tss.es = _current->md.md_tss.es & 0xFF;
  newProcess->md.md_tss.ldt = 0x18;
  newProcess->md.md_tss.trace_bitmap = 0x0000;
  newProcess->md.md_tss.io_map = 0x8000;

  newProcess->td.vm_tsize = _current->td.vm_tsize;
  newProcess->td.vm_taddr = _current->td.vm_taddr;
  newProcess->td.vm_dsize = _current->td.vm_dsize;
  newProcess->td.vm_daddr = _current->td.vm_daddr;

  /* Create A Copy Of The VM Space For New Task */
  //MrOlsen 2018kprintf("Copying Mem Space! [0x%X:0x%X:0x%X:0x%X:0x%X:%i:%i:0x%X]\n", newProcess->md.md_tss.esp0, newProcess->md.md_tss.esp, newProcess->md.md_tss.ebp, esi, eip, newProcess->id, _current->id, newProcess->td.vm_daddr);
  newProcess->md.md_tss.cr3 = (u_int32_t) vmm_copy_virtual_space(newProcess->id);
  memset(&newProcess->vm_map, 0, sizeof(newProcess->vm_map));
  vm_map_copy(&newProcess->vm_map, &_current->vm_map);

  sched_ready(newProcess);

  /* Return Id of Proccess */
  kprintf("Returning! [%i]", _current->id);

  return (newProcess->id);
}

void qT() {
  kprintf("qT\n");
}

/*****************************************************************************************
 Functoin: void sysFork();

 Desc: This function will fork a new task

 Notes:

 08/01/02 - This Seems To Be Working Fine However I'm Not Sure If I
 Chose The Best Path To Impliment It I Guess We Will See
 What The Future May Bring

 *****************************************************************************************/
//asm volatile(
__asm(
  ".globl sysFork_old       \n"
  "sysFork_old:                 \n"
  "  xor   %eax,%eax        \n"
  "  call  schedNewTask     \n"
  "  testl %eax,%eax        \n"
  "  je fork_ret            \n"
  "  pushl %esi             \n"
  "  pushl %edi             \n"
  "  pushl %ebp             \n"
  "  pushl %eax             \n"
  "  call  fork_copyProcess \n"
  "  movl  %eax,(%ebx)      \n"
  "  addl  $16,%esp         \n"
  "fork_ret:                \n"
  "  ret                    \n"
);

/***
 END
 ***/

