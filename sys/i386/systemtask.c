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

#include <ubixos/systemtask.h>
#include <ubixos/kpanic.h>
#include <ubixos/exec.h>
#include <ubixos/tty.h>
#include <ubixos/sched.h>
#include <ubixos/vitals.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <lib/bioscall.h>
#include <sde/sde.h>
#include <sys/io.h>
#include <vmm/vmm.h>
#include <mpi/mpi.h>
#include <string.h>

static unsigned char *videoBuffer = (unsigned char *) 0xB8000;

void systemTask() {
  mpi_message_t myMsg;
  uInt32 counter = 0x0;
  int i = 0x0;
  int *x = 0x0;
  kTask_t *tmpTask = 0x0;

  if (mpi_createMbox("system") != 0x0) {
    kpanic("Error: Error creating mailbox: system\n");
  }

  while (1) {
    if (mpi_fetchMessage("system", &myMsg) == 0x0) {
      kprintf("A");
      switch (myMsg.header) {
        case 0x69:
          x = (int *) &myMsg.data;
          kprintf("Switching to term: [%i][%i]\n", *x, myMsg.pid);
          schedFindTask(myMsg.pid)->term = tty_find(*x);
        break;
        case 1000:
          kprintf("Restarting the system in 5 seconds\n");
          counter = systemVitals->sysUptime + 5;
          while (systemVitals->sysUptime < counter) {
            sched_yield();
          }
          kprintf("Rebooting NOW!!!\n");
          while (inportByte(0x64) & 0x02)
            ;
          outportByte(0x64, 0xFE);
        break;
        case 31337:
          kprintf("system: backdoor opened\n");
        break;
        case 0x80:
          if (!strcmp(myMsg.data, "sdeStart")) {
            kprintf("Starting SDE\n");
            //execThread(sdeThread,(uInt32)(kmalloc(0x2000)+0x2000),0x0);
          }
          else if (!strcmp(myMsg.data, "freePage")) {
            kprintf("kkk Free Pages");
          }
          else if (!strcmp(myMsg.data, "sdeStop")) {
            printOff = 0x0;
            biosCall(0x10, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0);
            for (i = 0x0; i < 100; i++)
              asm("hlt");
          }
        break;
        default:
          kprintf("system: Received message %i:%s\n", myMsg.header, myMsg.data);
        break;
      }
    }

    /*
     Here we get the next task from the delete task queue
     we first check to see if it has an fd attached for the binary and after that
     we free the pages for the process and then free the task
     */
    tmpTask = sched_getDelTask();
    if (tmpTask != 0x0) {
      if (tmpTask->imageFd != 0x0)
        fclose(tmpTask->imageFd);
      vmm_freeProcessPages(tmpTask->id);
      kfree(tmpTask);
    }
    videoBuffer[0] = systemVitals->sysTicks;
    sched_yield();
  }

  return;
}

/***
 END
 ***/

