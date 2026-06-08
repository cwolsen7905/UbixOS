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
#include <cstring>
#include <unistd.h>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <ubistry/ubistry.h>
#include <api/display.h>
#include "window_manager.hh"
#include <sys/ubix_syscall.h>

/* ------------------------------------------------------------------ */
/* Syscall thunks (portable: i386 int $0x81, aarch64 svc — see the macro)  */
/* ------------------------------------------------------------------ */

UBIX_NATIVE_THUNK(_sys_mapfb, 43);       /* map the framebuffer into this process */
UBIX_NATIVE_THUNK(_sys_shareregion, 45); /* share a region into another process   */
UBIX_NATIVE_THUNK(_sys_getmouse, 44);    /* poll cooked mouse events              */
UBIX_NATIVE_THUNK(_sys_getkbd, 46);      /* poll cooked keyboard events           */
UBIX_NATIVE_THUNK(_sys_fbpresent, 54);   /* present a composited frame (no-op i386) */

extern "C"
{
	int _sys_getmouse(mouse_event_t *ev);
	int _sys_getkbd(kbd_event_t *ev);
	int _sys_fbpresent(void);
}

static int poll_mouse(mouse_event_t *ev)
{
	return _sys_getmouse(ev);
}
static int poll_kbd(kbd_event_t *ev)
{
	return _sys_getkbd(ev);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	ubix::Mailbox mbox;
	mbox.assign("views");
	if (!mbox.create())
	{
		std::printf("views: mpi_createMbox failed\n");
		return 1;
	}

	/* Request VESA init from the kernel display driver.  The desired VBE mode
	 * (the user's saved resolution preference from ubistry, 0 = kernel default)
	 * is carried at data[64]; the systemtask sets it where the V86 BIOS call is
	 * safe.  data[0..] is the reply mailbox name. */
	mpi_message_t req;
	std::memset(&req, 0, sizeof(req));
	req.header = 0x82;
	req.data[0] = 'v';
	req.data[1] = 'i';
	req.data[2] = 'e';
	req.data[3] = 'w';
	req.data[4] = 's';
	req.data[5] = '\0';
	{
		char mode_s[16];
		if (ubistry_get_str("/display/mode", mode_s, sizeof(mode_s)) == 0)
		{
			long m = std::strtol(mode_s, nullptr, 0);
			if (m > 0)
				*(uint16_t *)&req.data[64] = (uint16_t)m;
		}
	}
	/* Ask the systemtask to bring up VESA.  On i386 the V86 BIOS mode-set runs
	 * there and its reply gates sys_mapfb.  On a platform with no VESA systemtask
	 * (aarch64: the virtio-gpu framebuffer is already up from boot), the "system"
	 * mailbox doesn't exist, so the post fails — skip the handshake entirely and
	 * go straight to mapping the (already-initialised) framebuffer. */
	if (mpi_postMessage((char *)"system", 0x82, &req) == 0)
	{
		mpi_message_t reply;
		while (!mbox.try_fetch(reply))
			ubix::yield();
		if (reply.data[0] == 0)
		{
			std::printf("views: VESA init failed\n");
			return 1;
		}
	}

	WindowManager wm;
	{
		int tries = 500;
		while (wm.init() != 0)
		{
			if (--tries == 0)
			{
				std::printf("views: fb_open failed\n");
				return 1;
			}
			ubix::yield();
		}
	}
	wm.startup();
	_sys_fbpresent(); /* show the initial desktop (no-op on i386's live LFB) */

	/* Fork vlogin after the compositor is fully ready — launching it via
	 * init.d races with VESA VM86 and corrupts vlogin's startup context. */
	{
		int pid = ::fork();
		if (pid == 0)
		{
			char *argv_vl[] = {(char *)"vlogin", nullptr};
			char *envp_vl[] = {nullptr};
			::execve("/bin/vlogin", argv_vl, envp_vl);
			std::printf("views: failed to exec vlogin\n");
			std::exit(1);
		}
	}

	mpi_message_t msg;

	/* Throttle the dead-client sweep: the loop spins very tightly (one yield
	 * per pass), so probing every window's liveness each pass would be wasteful.
	 * Once every reap_interval passes is ample to reclaim a logged-out session's
	 * windows promptly without measurable overhead. */
	const unsigned reap_interval = 64;
	unsigned reap_tick = 0;

	for (;;)
	{
		kbd_event_t kev;
		while (poll_kbd(&kev) == 0)
			wm.handle_kbd(kev);

		mouse_event_t ev;
		while (poll_mouse(&ev) == 0)
			wm.handle_mouse(ev);

		while (mbox.try_fetch(msg))
			wm.dispatch(msg.header, msg.data);

		if (++reap_tick >= reap_interval)
		{
			reap_tick = 0;
			wm.reap_dead_clients();
		}

		wm.flush();
		_sys_fbpresent(); /* present the composited frame (no-op on i386) */
		ubix::yield();
	}

	return 0;
}
