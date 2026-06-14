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
#include <vmm/vm_map.h>
#include <ubixos/vitals.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <lib/bioscall.h>
#include <lib/vesa.h>
#include <isa/kbd.h>
#include <sys/shutdown.h>
#include <vmm/vmm.h>
#include <mpi/mpi.h>
#include <string.h>

/* display_proto.h lives in include/ (userland tree), not sys/include/.
 * Duplicate only the constant we need rather than pulling in the full header. */
#ifndef DISPLAY_FLIP
#define DISPLAY_FLIP 2
#endif

/* How long systemTask blocks on its mailbox before waking to poll the dead-task
 * reap queue.  An MPI message wakes it immediately regardless; this only bounds
 * how long a just-exited process waits before its pages are reclaimed (the
 * parent's wait4 already read the status from the ZOMBIE, so this is reclaim
 * latency only).  10 ticks = 100 ms: snappy reclaim, negligible idle CPU. */
#define SYSTASK_POLL_TICKS 10

static unsigned char *videoBuffer = (unsigned char *)0xB8000;

/*
 * Display ownership tracking.
 * When a process sends MPI 0x82 to claim the display (switch VESA mode),
 * we record its PID and the mode that was active before the switch.
 * The reaper loop restores the saved mode when the owning process exits
 * and notifies views to repaint.
 */
static pidType disp_owner_pid = 0;
static u_int16_t disp_saved_mode = 0;

void systemTask()
{

	mpi_message_t myMsg;
	u_int32_t counter = 0x0;
	int i = 0x0;
	int *x = 0x0;
	kTask_t *tmpTask = 0x0;

	char buf[16] = {"a\n"};

	if (mpi_createMbox("system") != 0x0)
	{
		kpanic("Error: Error creating mailbox: system\n");
	}

	while (1)
	{
		/* Block on the mailbox with a short timeout instead of busy-polling: a real
		 * message wakes us instantly; on timeout we fall through to drain any tasks
		 * that died meanwhile.  The old non-blocking fetch + sched_yield kept this
		 * thread permanently runnable, burning ~50% of a CPU at idle under TCG. */
		if (mpi_waitMessage("system", &myMsg, SYSTASK_POLL_TICKS) == 0x0)
		{
			switch (myMsg.header)
			{
				case 0x69:
					x = (int *)&myMsg.data;
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
					while (systemVitals->sysUptime < counter)
					{
						sched_yield();
					}
					sys_shutdown(REBOOT);
					break;
				case 31337:
					kprintf("system: backdoor opened\n");
					break;
				case 0x81:
					if (vesa_init(0x115) == 0)
					{
						vesa_map_fb();
						vesa_draw_test();
						vesa_draw_circle(400, 300, 150, 0xFFFFFF);
					}
					break;
				case 0x82:
				{
					/* Display mode claim / change: set a VBE mode and reply when ready.
					 * The desired mode number is carried at data[64] (0 = the 0x118
					 * default).  This runs in the systemtask context where the V86 BIOS
					 * call is safe, so views also re-sends 0x82 for live resolution
					 * changes.  Save the current mode so the reaper can restore it when
					 * the requesting process exits. */
					mpi_message_t reply;
					int vesa_ok = 0;
					u_int16_t prev_mode = vesa_current_mode;
					u_int16_t want_mode = *(u_int16_t *)&myMsg.data[64];

					/* Resolve to a 32bpp LFB mode wherever possible: a 32bpp framebuffer
					 * lets the compositor present via aligned row memcpy instead of the
					 * per-pixel 24bpp byte conversion — a real win under QEMU's software
					 * (TCG) emulation.  The 32bpp mode number varies by BIOS, so
					 * enumerate and map by resolution: take the requested mode's
					 * resolution (or 1024x768 for the default request) and pick the
					 * matching 32bpp mode.  Falls back to the known-good 24bpp 0x118. */
					if (g_vesa_mode_count < 0)
						g_vesa_mode_count = vesa_enum_modes(g_vesa_modes, VESA_MAX_MODES);
					{
						int tgt_w = 1024, tgt_h = 768, mi_i;
						for (mi_i = 0; want_mode != 0 && mi_i < g_vesa_mode_count; mi_i++)
						{
							if (g_vesa_modes[mi_i].mode == want_mode)
							{
								tgt_w = g_vesa_modes[mi_i].width;
								tgt_h = g_vesa_modes[mi_i].height;
								break;
							}
						}
						for (mi_i = 0; mi_i < g_vesa_mode_count; mi_i++)
						{
							if (g_vesa_modes[mi_i].bpp == 32 &&
							    g_vesa_modes[mi_i].width == tgt_w &&
							    g_vesa_modes[mi_i].height == tgt_h)
							{
								want_mode = g_vesa_modes[mi_i].mode;
								break;
							}
						}
						if (want_mode == 0)
							want_mode = 0x118; /* no 32bpp match and no explicit request */
					}

					vesa_ok = (vesa_init(want_mode) == 0);
					if (!vesa_ok && want_mode != 0x118)
						vesa_ok = (vesa_init(0x118) == 0); /* fall back to known-good default */

					if (vesa_ok)
					{
						vesa_map_fb();
						disp_saved_mode = prev_mode;
						disp_owner_pid = myMsg.pid;
						kprintf("system: display claimed by pid %d mode 0x%X (prev 0x%X)\n",
						        (int)myMsg.pid,
						        vesa_current_mode,
						        prev_mode);
					}

					reply.header = 0x82;
					reply.data[0] = vesa_ok ? 1 : 0;
					reply.data[1] = '\0';
					if (myMsg.data[0] != '\0')
						mpi_postMessage(myMsg.data, 0x82, &reply);

					/* Enumerate the available modes once (after replying so we don't
					 * delay the compositor's startup) for the sys_vesa_modes syscall. */
					if (vesa_ok && g_vesa_mode_count < 0)
						g_vesa_mode_count = vesa_enum_modes(g_vesa_modes, VESA_MAX_MODES);
					break;
				}
				case 0x80:
					if (!strcmp(myMsg.data, "free_page"))
					{
						kprintf("kkk Free Pages");
					}
					else if (!strcmp(myMsg.data, "sdeStop"))
					{
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
		while ((tmpTask = sched_getDelTask()) != 0x0)
		{
			/* If this task owned the display, restore the previous VESA mode
			 * and tell views to repaint.  Do this before freeing pages so
			 * vesa_init()'s BIOS call can still execute cleanly. */
			if (disp_owner_pid != 0 && tmpTask->id == (int)disp_owner_pid)
			{
				kprintf("system: display owner pid %d exited — restoring mode 0x%X\n",
				        (int)disp_owner_pid,
				        disp_saved_mode);
				if (disp_saved_mode == 0)
				{
					kbd_gui_mode = 0; /* unblock keyboard before the slow BIOS call */
					vesa_text_mode();
				}
				else if (disp_saved_mode != vesa_current_mode)
				{
					if (vesa_init(disp_saved_mode) == 0)
						vesa_map_fb();
				}
				disp_owner_pid = 0;
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
			vm_map_free(&tmpTask->vm_map);
			/* Reclaim the page tables/dir only for the task that owns the address
			 * space (a normal process, or the last thread of a tgid).  A non-last
			 * thread shared its siblings' cr3 and must not free it (reap_free_as=0,
			 * set in endTask). */
			if (tmpTask->reap_free_as)
				vmm_free_process_pages(tmpTask->id);
			if (tmpTask->kernelStack != 0x0)
				kfree(tmpTask->kernelStack);
			else
				kprintf("systemTask: WARNING: task %i has NULL kernelStack\n", tmpTask->id);
			kfree(tmpTask);
		}

		if (ogprintOff == 1)
		{
			videoBuffer[0] = systemVitals->sysTicks;
			videoBuffer[1] = 'c';
		}
		/* No sched_yield here: mpi_waitMessage() above already deschedules us until a
		 * message arrives or the poll timeout expires. */
	}

	return;
}
