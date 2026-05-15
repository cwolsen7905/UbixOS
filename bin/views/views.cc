/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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

#include <cstdio>
#include <cstdlib>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include "window_manager.hh"

/* ------------------------------------------------------------------ */
/* Syscall stubs                                                        */
/* ------------------------------------------------------------------ */

/* Syscall 43 — map the VESA framebuffer into the calling process */
asm(
    ".text                            \n"
    ".globl _sys_mapfb                \n"
    ".type  _sys_mapfb, @function     \n"
    "_sys_mapfb:                      \n"
    "  movl $43, %eax                 \n"
    "  int  $0x81                     \n"
    "  ret                            \n"
);

/* Syscall 45 — share a virtual region into another process's space */
asm(
    ".text                                \n"
    ".globl _sys_shareregion              \n"
    ".type  _sys_shareregion, @function   \n"
    "_sys_shareregion:                    \n"
    "  movl $45, %eax                     \n"
    "  int  $0x81                         \n"
    "  ret                                \n"
);

/* Syscall 44 — poll mouse event queue */
asm(
    ".text                              \n"
    ".globl _sys_getmouse               \n"
    ".type  _sys_getmouse, @function    \n"
    "_sys_getmouse:                     \n"
    "  movl $44, %eax                   \n"
    "  int  $0x81                       \n"
    "  ret                              \n"
);

/* Syscall 46 — poll keyboard event queue */
asm(
    ".text                              \n"
    ".globl _sys_getkbd                 \n"
    ".type  _sys_getkbd, @function      \n"
    "_sys_getkbd:                       \n"
    "  movl $46, %eax                   \n"
    "  int  $0x81                       \n"
    "  ret                              \n"
);

extern "C" {
	int _sys_getmouse(mouse_event_t *ev);
	int _sys_getkbd(kbd_event_t *ev);
}

static int poll_mouse(mouse_event_t *ev) { return _sys_getmouse(ev); }
static int poll_kbd(kbd_event_t *ev)     { return _sys_getkbd(ev); }

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	ubix::Mailbox mbox;
	mbox.assign("views");
	if (!mbox.create()) {
		std::printf("views: mpi_createMbox failed\n");
		return 1;
	}

	/* Request VESA init from the kernel display driver */
	mpi_message_t req;
	req.header  = 0x82;
	req.data[0] = 'v'; req.data[1] = 'i'; req.data[2] = 'e';
	req.data[3] = 'w'; req.data[4] = 's'; req.data[5] = '\0';
	ubix::post_message("system", 0x82, req);

	mpi_message_t reply;
	while (!mbox.try_fetch(reply))
		ubix::yield();

	if (reply.data[0] == 0) {
		std::printf("views: VESA init failed\n");
		return 1;
	}

	WindowManager wm;
	if (wm.init() != 0) {
		std::printf("views: fb_open failed\n");
		return 1;
	}
	wm.startup();

	/* Launch taskbar */
	{
		int pid = ::fork();
		if (pid == 0) {
			char *argv_tb[] = { (char *)"taskbar", nullptr };
			char *envp_tb[] = { nullptr };
			::execve("sys:/bin/taskbar", argv_tb, envp_tb);
			std::printf("views: failed to exec taskbar\n");
			std::exit(1);
		}
	}

	mpi_message_t msg;

	for (;;) {
		kbd_event_t kev;
		while (poll_kbd(&kev) == 0)
			wm.handle_kbd(kev);

		mouse_event_t ev;
		while (poll_mouse(&ev) == 0)
			wm.handle_mouse(ev);

		while (mbox.try_fetch(msg)) {
			switch (msg.header) {
			case DISPLAY_QUERY:
				wm.handle_query(
				    (struct display_query *)msg.data);
				break;
			case DISPLAY_CLAIM:
				wm.handle_claim(
				    (struct display_claim_req *)msg.data);
				break;
			case DISPLAY_FLIP:
				wm.handle_flip(
				    (struct display_flip *)msg.data);
				break;
			case DISPLAY_RELEASE:
				wm.handle_release(
				    (struct display_release *)msg.data);
				break;
			case DISPLAY_RAISE:
				wm.handle_raise(
				    (struct display_raise *)msg.data);
				break;
			default:
				break;
			}
		}

		ubix::yield();
	}

	return 0;
}
