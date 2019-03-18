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

#include <sys/signal.h>
#include <sys/sysproto_posix.h>
#include <sys/thread.h>
#include <lib/kprintf.h>
#include <assert.h>

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
      } else {
        td->td_retval[0] = -1;
      }
    } else if (args->how == SIG_BLOCK) {
      if (args->set != 0x0) {
        td->sigmask.__bits[0] &= args->set->__bits[0];
        td->sigmask.__bits[1] &= args->set->__bits[1];
        td->sigmask.__bits[2] &= args->set->__bits[2];
        td->sigmask.__bits[3] &= args->set->__bits[3];
        td->td_retval[0] = 0;
      } else {
        td->td_retval[0] = -1;
      }
    } else if (args->how == SIG_UNBLOCK) {
      if (args->set != 0x0) {
        td->sigmask.__bits[0] |= args->set->__bits[0];
        td->sigmask.__bits[1] |= args->set->__bits[1];
        td->sigmask.__bits[2] |= args->set->__bits[2];
        td->sigmask.__bits[3] |= args->set->__bits[3];
        td->td_retval[0] = 0;
      } else {
        td->td_retval[0] = -1;
      }
    } else {
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
