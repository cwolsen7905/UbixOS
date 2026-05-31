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

#pragma once

#include <views/display.hh>
#include "window_registry.hh"
#include "compositor.hh"
#include "input_router.hh"

/* ------------------------------------------------------------------ */
/* WindowManager — facade: coordinates registry, compositor, router   */
/* ------------------------------------------------------------------ */

class WindowManager
{
	WindowRegistry reg_;
	Compositor comp_;
	InputRouter input_;
	bool mode_pending_ = false; /* awaiting the 0x82 reply for a live mode switch */

	void notify_taskbar(Window *w, uint8_t added);
	void close_window(Window *w);

      public:
	WindowManager();

	int init();
	void startup();
	void composite_all();

	/* Route an incoming MPI message to the appropriate handler.
	 * Adding a new message type only requires updating the handler
	 * table in window_manager.cc — views.cc is never touched. */
	void dispatch(uint32_t id, void *data);
	void flush();

	void handle_query(struct display_query *dq);
	void handle_claim(struct display_claim_req *creq);
	void handle_flip(struct display_flip *fl);
	void handle_release(struct display_release *rel);
	void handle_raise(struct display_raise *dr);
	void handle_settitle(struct display_settitle *st);
	void handle_refresh_desktop();
	void handle_set_user(struct display_set_user *su);
	void handle_setmode(struct display_setmode *sm);
	void handle_vesa_ready();
	void handle_mouse(mouse_event_t &ev);
	void handle_kbd(kbd_event_t &ev);
};
