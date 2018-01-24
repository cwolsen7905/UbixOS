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

#include <sys/gen_calls.h>
#include <sys/thread.h>
#include <sys/gdt.h>
#include <ubixos/sched.h>
#include <ubixos/endtask.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <assert.h>
#include <sys/descrip.h>
#include <sys/video.h>
#include <sys/signal.h>
#include <ubixos/errno.h>
#include <vmm/vmm.h>

/* Exit Syscall */
int sys_exit(struct thread *td, struct sys_exit_args *args) {
  //kprintf("exit(%i)", args->status);
  endTask(_current->id);
  return (0x0);
}

/* return the process id */
int getpid(struct thread *td, struct getpid_args *uap) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  td->td_retval[0] = _current->id;
  return (0);
}

/* return the process user id */
int getuid(struct thread *td, struct getuid_args *uap) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  td->td_retval[0] = _current->uid;
  return (0);
}

/* return the process group id */
int getgid(struct thread *td, struct getgid_args *uap) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  td->td_retval[0] = _current->gid;
  return (0);
}

int sys_issetugid(register struct thread *td, struct sys_issetugid_args *uap) {
  td->td_retval[0] = 0;
  return (0);
}

int readlink(struct thread *td, struct readlink_args *uap) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  kprintf("readlink: [%s:%i]\n", uap->path, uap->count);
  td->td_retval[0] = -1;
  td->td_retval[1] = 0x0;
  return (0x0);
}

int gettimeofday_new(struct thread *td, struct gettimeofday_args *uap) {
#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif
  return (0x0);
}

int read(struct thread *td, struct read_args *uap) {
  int error = 0x0;
  size_t count = 0x0;
  struct file *fd = 0x0;

#ifdef DEBUG
  kprintf("[%s:%i]",__FILE__,__LINE__);
#endif

  error = getfd(td, &fd, uap->fd);

  if (error)
    return (error);

  count = fread(uap->buf, uap->nbyte, 0x1, fd->fd);
  kprintf("count: %i\n", count);
  td->td_retval[0] = count;

  return (error);
}

/*!
 * \brief place holder for now functionality to be added later
 */
int setitimer(struct thread *td, struct setitimer_args *uap) {
  int error = 0x0;

  return (error);
}

int access(struct thread *td, struct access_args *uap) {
  int error = 0x0;
  kprintf("name: [%s]\n", uap->path);
  return (error);
}

int mprotect(struct thread *td, struct mprotect_args *uap) {
  int error = 0x0;
  return (error);
}

int sys_invalid(struct thread *td, void *args) {
  kprintf("ISC[%i:%i]", td->frame->tf_eax, _current->id);
  td->td_retval[0] = -1;
  return (0);
}

int sys_wait4(struct thread *td, struct sys_wait4_args *args) {
  int error = 0;

  if (args->pid == -1) {
    if (_current->children <= 0) {
      td->td_retval[0] = ECHILD;
      return (-1);
    }

    int children = _current->children;

    sched_setStatus(_current->id, WAIT);
    while (_current->children == children) {
      sched_yield();
    }

    td->td_retval[0] = _current->last_exit;
    td->td_retval[1] = 0x8;
  }
  else {

    kTask_t *tmpTask = schedFindTask(args->pid);

    if (tmpTask != 0x0) {
      sched_setStatus(_current->id, WAIT);
      while (tmpTask != 0x0) {
        sched_yield();
        tmpTask = schedFindTask(args->pid);
      }
      td->td_retval[0] = args->pid;
    }
    else {
      td->td_retval[0] = -1;
      error = -1;
    }
  }
  return (error);
}

int sys_sysarch(struct thread *td, struct sys_sysarch_args *args) {
  void **segbase = 0x0;
  uint32_t base_addr = 0x0;
  if (args->op == 10) {
    kprintf("SETGSBASE: 0x%X:0x%X", args->parms, args->parms[0]);
    segbase = args->parms;
    kprintf("SGS: [0x%X:0x%X]", segbase[0], segbase[1]);
    base_addr = (uint32_t) segbase[0];
    struct gdtDescriptor *tmpDesc = 0x0;

    tmpDesc = VMM_USER_LDT + sizeof(struct gdtDescriptor); //taskLDT[1];

    tmpDesc->limitLow = (0xFFFFF & 0xFFFF);
    tmpDesc->baseLow = (base_addr & 0xFFFF);
    tmpDesc->baseMed = ((base_addr >> 16) & 0xFF);
    tmpDesc->access = ((dData + dWrite + dBig + dBiglim + dDpl3) + dPresent) >> 8;
    tmpDesc->limitHigh = (0xFFFFF >> 16);
    tmpDesc->granularity = ((dData + dWrite + dBig + dBiglim + dDpl3) & 0xFF) >> 4;
    tmpDesc->baseHigh = base_addr >> 24;
    /*
     asm(
     "push %eax\n"
     "lgdtl (loadGDT)\n"
     "mov $0xF,%eax\n"
     "mov %eax,%gs\n"
     "pop %eax\n"
     );
     */
    td->td_retval[0] = 0;
  }
  else {
    kprintf("sysarch(%i,NULL)", args->op);
    td->td_retval[0] = -1;
  }
  return (0);
}

int sys_getpid(struct thread *td, struct sys_getpid_args *args) {
  td->td_retval[0] = _current->id;
  return (0);
}
int sys_geteuid(struct thread *td, struct sys_geteuid_args *args) {
  td->td_retval[0] = _current->uid;
  return (0);
}

int sys_getegid(struct thread *td, struct sys_getegid_args *args) {
  td->td_retval[0] = _current->gid;
  return (0);
}

int sys_getppid(struct thread *td, struct sys_getppid_args *args) {
  td->td_retval[0] = _current->ppid;
  return (0);
}

int sys_sigprocmask(struct thread *td, struct sys_sigprocmask_args *args) {
  td->td_retval[0] = -1;

  if (args->oset != 0x0) {
    memcpy(args->oset, &td->sigmask, sizeof(sigset_t));
    td->td_retval[0] = 0x0;
  }

  if (args->set != 0x0) {
    if (args->how == SIG_SETMASK) {
      if (args->set != 0x0) {
        memcpy(&td->sigmask, args->set, sizeof(sigset_t));
        td->td_retval[0] = 0;
      }
      else {
        td->td_retval[0] = -1;
      }
    }
    else if (args->how == SIG_BLOCK) {
      if (args->set != 0x0) {
        td->sigmask.__bits[0] &= args->set->__bits[0];
        td->sigmask.__bits[1] &= args->set->__bits[1];
        td->sigmask.__bits[2] &= args->set->__bits[2];
        td->sigmask.__bits[3] &= args->set->__bits[3];
        td->td_retval[0] = 0;
      }
      else {
        td->td_retval[0] = -1;
      }
    }
    else if (args->how == SIG_UNBLOCK) {
      if (args->set != 0x0) {
        td->sigmask.__bits[0] |= args->set->__bits[0];
        td->sigmask.__bits[1] |= args->set->__bits[1];
        td->sigmask.__bits[2] |= args->set->__bits[2];
        td->sigmask.__bits[3] |= args->set->__bits[3];
        td->td_retval[0] = 0;
      }
      else {
        td->td_retval[0] = -1;
      }
    }
    else {
      kprintf("SPM: 0x%X", args->how);
      td->td_retval[0] = -1;
    }
  }

  return (0);
}

int sys_sigaction(struct thread *td, struct sys_sigaction_args *args) {
  td->td_retval[0] = -1;

  if (args->oact != 0x0) {
    memcpy(args->oact, &td->sigact[args->sig], sizeof(struct sigaction));
    td->td_retval[0] = 0;
  }

  if (args->act != 0x0) {
    //kprintf("SA: %i", args->sig);
    memcpy(&td->sigact[args->sig], args->act, sizeof(struct sigaction));
    td->td_retval[0] = 0;
  }
  return (0);
}

int sys_getpgrp(struct thread *td, struct sys_getpgrp_args *args) {
  td->td_retval[0] = _current->pgrp;
  return (0);
}

int sys_setpgid(struct thread *td, struct sys_setpgid_args *args) {
  pidType pid = 0x0;
  pidType pgrp = 0x0;

  if (args->pid == 0x0 || args->pid == _current->id) {
    if (args->pgid == 0x0 || args->pgid == _current->id) {
      td->td_retval[0] = 0x0;
      _current->pgrp = _current->id;
    }
    else {
      td->td_retval[0] = -1;
    }
  }
  else {
    kTask_t *tmpTask = schedFindTask(pid);

    if (tmpTask == 0x0) {
      td->td_retval[0] = -1;
    }
    else {

      /* Get The PRGP We Want To Set */
      pgrp = (args->pgid == 0) ? tmpTask->pgrp : args->pgid;

      if (pgrp != _current->pgrp || pgrp != tmpTask->id) {
        td->td_retval[0] = -1;
      }
      else {
        td->td_retval[0] = 0x0;
        tmpTask->pgrp = pgrp;
      }

    }

  }

  return (0);
}

int sys_gettimeofday(struct thread *td, struct sys_gettimeofday_args *args) {
  gettimeofday(args->tp, args->tzp);
  td->td_retval[0] = 0;
  return (0);
}
