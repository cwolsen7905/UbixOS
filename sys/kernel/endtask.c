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

#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/vitals.h>
#include <vmm/vmm.h>
#include <lib/kprintf.h>
#include <isa/8259.h>

/************************************************************************

 Function: endTask(pidType pid)

 Description: This will do cleanup for an ending task

 Notes:

 ************************************************************************/
void endTask(pidType pid) {
  //kTask_t *tmpTask = 0x0;

  /* Don't mess with scheduler structures from outside the scheduler! */
  /* Just set status to dead, and let the scheduler clean up itself */
  sched_setStatus(pid, DEAD);
  //tmpTask = schedFindTask(pid);
  //if (sched_deleteTask(pid) != 0x0)
  //  kpanic("sched_deleteTask: Failed\n");
  //kprintf("Ending Task: (%i:0x%X)\n",tmpTask->id,tmpTask);
  //sched_addDelTask(tmpTask);
  //tmpTask->state = DEAD;

  //tmpTask->term->owner = tmpTask->parentPid;

  if (pid == _current->id)
    while (1)
      sched_yield();
  sched_yield();

  return;
}

/***
 END
 ***/

