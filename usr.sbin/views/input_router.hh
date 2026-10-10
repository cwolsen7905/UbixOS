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

/* ------------------------------------------------------------------ */
/* InputRouter — mouse/keyboard routing, drag, and window lifecycle    */
/* ------------------------------------------------------------------ */

class InputRouter
{
	WindowRegistry &reg_;
	Compositor &comp_;

	bool dragging_;
	Window *drag_win_;
	int drag_off_x_;
	int drag_off_y_;
	uint8_t prev_buttons_;

	/* Resize-by-grip drag state. */
	bool resizing_ = false;
	Window *resize_win_ = nullptr;
	int resize_start_w_ = 0, resize_start_h_ = 0;
	int resize_start_cx_ = 0, resize_start_cy_ = 0;
	int resize_start_x_ = 0, resize_start_y_ = 0;
	int resize_edges_ = 0; /* Window::RZ_* being dragged */
	void resize_geometry(int &nx, int &ny, int &nw, int &nh) const;

	/* Double-click-on-title detection (toggles maximize). */
	long last_click_ms_ = 0;
	Window *last_click_win_ = nullptr;
	int last_click_x_ = 0, last_click_y_ = 0;

	/* Id of the window the cursor last hovered (for motion delivery); 0 if none.
	 * Tracked by id, not pointer, so a destroyed window can't dangle. */
	uint32_t last_hover_id_ = 0;

	/* Alt-Tab task switcher: modifier state + whether the overlay is up.  Alt+Tab
	 * opens/advances it; releasing Alt commits, Esc cancels. */
	bool alt_held_ = false;
	bool shift_held_ = false;
	bool switcher_active_ = false;
	void commit_switch(uint32_t id);
	/* Window whose title-bar control is hovered/pressed (0 if none), by id. */
	uint32_t ctl_hover_id_ = 0;

	/* Window that received the last left press (by id): its release goes there
	 * too, even when the press did not move focus (taskbar, pop-overs). */
	uint32_t press_id_ = 0;
	void update_ctl_hover(uint8_t buttons);

	/* on_close_ is called when the user clicks the close button.
	 * WindowManager sets this to its own close_window method via a
	 * captureless lambda + context pointer, avoiding a circular dep. */
	void *close_ctx_;
	void (*close_fn_)(void *, Window *);
	void (*min_fn_)(void *, Window *);              /* minimize callback (same ctx) */
	void (*resize_fn_)(void *, Window *, int, int); /* resize-commit callback (same ctx) */
	void (*place_fn_)(void *, Window *, int);       /* maximize/snap (mode: 0=max,1=left,2=right) */
	void (*focus_fn_)(void *, Window *);            /* click-to-focus callback (same ctx) */

	void send_mouse(Window *w, int cx, int cy, uint8_t buttons, int8_t wheel = 0);
	void close_window(Window *w);

      public:
	InputRouter(WindowRegistry &reg,
	            Compositor &comp,
	            void *close_ctx,
	            void (*close_fn)(void *, Window *),
	            void (*min_fn)(void *, Window *),
	            void (*resize_fn)(void *, Window *, int, int),
	            void (*place_fn)(void *, Window *, int),
	            void (*focus_fn)(void *, Window *));

	void handle_mouse(mouse_event_t &ev);
	void handle_kbd(kbd_event_t &ev);
};
