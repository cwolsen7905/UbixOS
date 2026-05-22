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
#include <lib/vesa.h>
#include <sys/shutdown.h>
#include <vmm/vmm.h>
#include <mpi/mpi.h>
#include <string.h>

/* display_proto.h lives in include/ (userland tree), not sys/include/.
 * Duplicate only the constant we need rather than pulling in the full header. */
#ifndef DISPLAY_FLIP
#define DISPLAY_FLIP 2
#endif

static unsigned char *videoBuffer = (unsigned char*) 0xB8000;

/*
 * Display ownership tracking.
 * When a process sends MPI 0x82 to claim the display (switch VESA mode),
 * we record its PID and the mode that was active before the switch.
 * The reaper loop restores the saved mode when the owning process exits
 * and notifies views to repaint.
 */
static pidType disp_owner_pid  = 0;
static uint16_t disp_saved_mode = 0;

void systemTask() {

  mpi_message_t myMsg;
  uint32_t counter = 0x0;
  int i = 0x0;
  int *x = 0x0;
  kTask_t *tmpTask = 0x0;

  char buf[16] = { "a\n" };

  if (mpi_createMbox("system") != 0x0) {
    kpanic("Error: Error creating mailbox: system\n");
  }


  while (1) {
    if (mpi_fetchMessage("system", &myMsg) == 0x0) {
      switch (myMsg.header) {
        case 0x69:
          x = (int*) &myMsg.data;
          kprintf("Switching to term: [%i][%i]\n", *x, myMsg.pid);
          {
            kTask_t *_st = schedFindTask(myMsg.pid);
            if (_st != NULL)
              _st->term = tty_find(*x);
          }
          break;
        case 1000:
          kprintf("Restarting the system in 5 seconds\n");
          counter = systemVitals->sysUptime + 5;
          while (systemVitals->sysUptime < counter) {
            sched_yield();
          }
                    sys_shutdown(REBOOT);
          break;
        case 31337:
          kprintf("system: backdoor opened\n");
          break;
        case 0x81:
          if (vesa_init(0x115) == 0) {
            vesa_map_fb();
            vesa_draw_test();
            vesa_draw_circle(400, 300, 150, 0xFFFFFF);
          }
          break;
        case 0x82: {
          /* Display mode claim: init VESA 1024x768x24, reply when ready.
           * Save the current mode so the reaper can restore it when the
           * requesting process exits.  views sends this at startup and runs
           * forever, so disp_owner_pid stays 0 for the compositor itself —
           * we only track transient fullscreen apps that will eventually die. */
          mpi_message_t reply;
          int vesa_ok = 0;
          uint16_t prev_mode = vesa_current_mode;

          if (vesa_init(0x118) == 0) {
            vesa_map_fb();
            vesa_ok = 1;
          }

          if (vesa_ok) {
            disp_saved_mode = prev_mode;
            disp_owner_pid  = myMsg.pid;
            kprintf("system: display claimed by pid %d (prev mode 0x%X)\n",
                (int)myMsg.pid, prev_mode);
          }

          reply.header = 0x82;
          reply.data[0] = vesa_ok ? 1 : 0;
          reply.data[1] = '\0';
          if (myMsg.data[0] != '\0')
            mpi_postMessage(myMsg.data, 0x82, &reply);
          break;
        }
        case 0x80:
          if (!strcmp(myMsg.data, "freePage")) {
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
      /* If this task owned the display, restore the previous VESA mode
       * and tell views to repaint.  Do this before freeing pages so
       * vesa_init()'s BIOS call can still execute cleanly. */
      if (disp_owner_pid != 0 && tmpTask->id == (int)disp_owner_pid) {
        kprintf("system: display owner pid %d exited — restoring mode 0x%X\n",
            (int)disp_owner_pid, disp_saved_mode);
        if (disp_saved_mode == 0) {
          vesa_text_mode();
        } else if (disp_saved_mode != vesa_current_mode) {
          if (vesa_init(disp_saved_mode) == 0)
            vesa_map_fb();
        }
        disp_owner_pid  = 0;
        disp_saved_mode = 0;

        /* Signal views to repaint the full compositor surface */
        {
          mpi_message_t note;
          memset(&note, 0, sizeof(note));
          note.header = DISPLAY_FLIP;
          mpi_postMessage("views", DISPLAY_FLIP, &note);
        }
      }

      if (tmpTask->files[0] != 0x0)
        fclose(tmpTask->files[0]);
      vmm_freeProcessPages(tmpTask->id);
      if (tmpTask->kernelStack != 0x0)
        kfree(tmpTask->kernelStack);
      else
        kprintf("systemTask: WARNING: task %i has NULL kernelStack\n", tmpTask->id);
      kfree(tmpTask);
    }

    if (ogprintOff == 1) {
      videoBuffer[0] = systemVitals->sysTicks;
      videoBuffer[1] = 'c';
    }
    /*
     else
     ogPrintf(buf);
     */

    sched_yield();
  }

  return;
}
