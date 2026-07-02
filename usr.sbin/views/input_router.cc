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

#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include <sys/kbd.h>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include "input_router.hh"

/* Monotonic-ish millisecond clock for double-click timing. */
static long now_ms(void)
{
	struct timeval tv;
	if (::gettimeofday(&tv, nullptr) != 0)
		return 0;
	return (long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000;
}

#define DBLCLICK_MS 400 /* max gap between the two clicks */
#define DBLCLICK_PX 6   /* max cursor drift between the two clicks */

InputRouter::InputRouter(WindowRegistry &reg,
                         Compositor &comp,
                         void *close_ctx,
                         void (*close_fn)(void *, Window *),
                         void (*min_fn)(void *, Window *),
                         void (*resize_fn)(void *, Window *, int, int),
                         void (*place_fn)(void *, Window *, int),
                         void (*focus_fn)(void *, Window *))
    : reg_(reg), comp_(comp), dragging_(false), drag_win_(nullptr), drag_off_x_(0), drag_off_y_(0), prev_buttons_(0),
      close_ctx_(close_ctx), close_fn_(close_fn), min_fn_(min_fn), resize_fn_(resize_fn), place_fn_(place_fn),
      focus_fn_(focus_fn)
{
}

/* Clamp a prospective resize to the window's constraints. */
static void clamp_resize(const Window *w, int &nw, int &nh)
{
	if (nw < w->min_w)
		nw = w->min_w;
	if (w->max_w > 0 && nw > w->max_w)
		nw = w->max_w;
	if (nh < w->min_h)
		nh = w->min_h;
	if (w->max_h > 0 && nh > w->max_h)
		nh = w->max_h;
}

void InputRouter::send_mouse(Window *w, int cx, int cy, uint8_t buttons, int8_t wheel)
{
	if (w->mbox.empty())
		return;
	mpi_message_t mev;
	struct display_mouse_ev *me = (struct display_mouse_ev *)mev.data;
	mev.header = DISPLAY_MOUSE;
	me->window_id = w->id;
	me->x = (int16_t)(cx - w->x);
	me->y = (int16_t)(cy - (w->y + w->decor_h));
	me->dx = 0;
	me->dy = 0;
	me->buttons = buttons;
	me->wheel = wheel;
	ubix::post_message(w->mbox, DISPLAY_MOUSE, mev);
}

void InputRouter::close_window(Window *w)
{
	close_fn_(close_ctx_, w);
}

void InputRouter::handle_mouse(mouse_event_t &ev)
{
	comp_.cursor_move(ev.dx, ev.dy);

	if (dragging_ && drag_win_ && (ev.dx || ev.dy))
	{
		int ox = drag_win_->x, oy = drag_win_->y;
		int fullh = drag_win_->decor_h + drag_win_->h;
		drag_win_->x = comp_.cur_x() - drag_off_x_;
		drag_win_->y = comp_.cur_y() - drag_off_y_;
		if (drag_win_->x < 0)
			drag_win_->x = 0;
		if (drag_win_->y < 0)
			drag_win_->y = 0;
		/* Repaint only the union of the old and new window rectangles, not the
		 * whole screen — keeps dragging snappy. */
		int nx = drag_win_->x, ny = drag_win_->y;
		int ux = (ox < nx) ? ox : nx;
		int uy = (oy < ny) ? oy : ny;
		int ux2 = ((ox > nx) ? ox : nx) + drag_win_->w;
		int uy2 = ((oy > ny) ? oy : ny) + fullh;
		comp_.invalidate(ux, uy, ux2 - ux, uy2 - uy);
	}

	if (resizing_ && resize_win_ && (ev.dx || ev.dy))
	{
		int nw = resize_start_w_ + (comp_.cur_x() - resize_start_cx_);
		int nh = resize_start_h_ + (comp_.cur_y() - resize_start_cy_);
		clamp_resize(resize_win_, nw, nh);
		comp_.set_resize_preview(true, resize_win_->x, resize_win_->y, nw, nh + resize_win_->decor_h);
	}

	/*
	 * Deliver pointer motion only to a focused window that opted in via
	 * DISPLAY_CLAIM.wants_motion (cf. macOS acceptsMouseMovedEvents).  Apps that
	 * did not opt in (taskbar, start menu, simple dialogs) never receive motion,
	 * so a hover can never be mistaken for a click.  Opted-in clients (the
	 * NetSurf frontend) use motion to track the cursor — needed so clicks land
	 * on the right widget, the URL box can be focused, and hover/drag work.
	 */
	if ((ev.dx || ev.dy) && !dragging_ && !resizing_)
	{
		int cx = comp_.cur_x(), cy = comp_.cur_y();
		Window *f = reg_.focused();
		if (f != nullptr && f->wants_motion && !f->minimized && (f->hit_test(cx, cy) || ev.buttons != 0))
			send_mouse(f, cx, cy, ev.buttons);

		/* Hover: also deliver motion to the topmost opted-in window under the
		 * cursor (if it isn't the focused one already handled above) so panels
		 * and menus — taskbar, start menu — can track hover without being
		 * focused. */
		Window *hov = nullptr;
		for (auto it = reg_.z_stack().rbegin(); it != reg_.z_stack().rend(); ++it)
		{
			Window *h = *it;
			if (h->minimized || !h->wants_motion)
				continue;
			if (h->hit_test(cx, cy))
			{
				hov = h;
				break; /* topmost hit only */
			}
		}

		/* Tell the previously-hovered window the cursor left (negative coords)
		 * so it can clear its hover highlight — there is no enter/exit event. */
		uint32_t hov_id = hov ? hov->id : 0;
		if (last_hover_id_ != 0 && last_hover_id_ != hov_id)
		{
			Window *prev = reg_.find(last_hover_id_);
			if (prev != nullptr && prev->wants_motion)
				send_mouse(prev, -1, -1, 0);
		}
		last_hover_id_ = hov_id;

		if (hov != nullptr && hov != f)
			send_mouse(hov, cx, cy, ev.buttons);
	}

	/* Wheel: deliver a scroll notch to the topmost window under the cursor.  A
	 * discrete event (not hover), so it goes to any window — no wants_motion
	 * opt-in needed — and before the buttons-unchanged early return below (a
	 * wheel-only event leaves the button state untouched). */
	if (ev.wheel != 0)
	{
		int cx = comp_.cur_x(), cy = comp_.cur_y();
		for (auto it = reg_.z_stack().rbegin(); it != reg_.z_stack().rend(); ++it)
		{
			Window *h = *it;
			if (h->minimized)
				continue;
			if (h->hit_test(cx, cy))
			{
				send_mouse(h, cx, cy, ev.buttons, ev.wheel);
				break;
			}
		}
	}

	if (ev.buttons == prev_buttons_)
		return;

	bool pressed = (ev.buttons & 1) != 0;

	if (!pressed && dragging_)
	{
		/* Snap if the cursor was dragged to a screen edge (resizable only). */
		if (drag_win_->resizable())
		{
			const int M = 6;
			int rcx = comp_.cur_x(), rcy = comp_.cur_y();
			int sw = (int)comp_.screen_w();
			if (rcy <= M)
				place_fn_(close_ctx_, drag_win_, 0); /* top → maximize */
			else if (rcx <= M)
				place_fn_(close_ctx_, drag_win_, 1); /* left → left half */
			else if (rcx >= sw - M)
				place_fn_(close_ctx_, drag_win_, 2); /* right → right half */
		}
		dragging_ = false;
		drag_win_ = nullptr;
	}

	if (!pressed && resizing_)
	{
		int nw = resize_start_w_ + (comp_.cur_x() - resize_start_cx_);
		int nh = resize_start_h_ + (comp_.cur_y() - resize_start_cy_);
		clamp_resize(resize_win_, nw, nh);
		comp_.set_resize_preview(false, 0, 0, 0, 0);
		resize_fn_(close_ctx_, resize_win_, nw, nh);
		resizing_ = false;
		resize_win_ = nullptr;
	}

	if (pressed)
	{
		int cx = comp_.cur_x(), cy = comp_.cur_y();
		Window *hit = nullptr;
		for (auto it = reg_.z_stack().rbegin(); it != reg_.z_stack().rend(); ++it)
		{
			if ((*it)->minimized)
				continue; /* hidden windows are not clickable */
			if ((*it)->hit_test(cx, cy))
			{
				hit = *it;
				break;
			}
		}
		if (hit)
		{
			reg_.z_raise(hit);
			focus_fn_(close_ctx_, hit);
			comp_.invalidate_all();

			if (hit->in_resize_grip(cx, cy))
			{
				resizing_ = true;
				resize_win_ = hit;
				resize_start_w_ = hit->w;
				resize_start_h_ = hit->h;
				resize_start_cx_ = cx;
				resize_start_cy_ = cy;
			}
			else if (hit->in_close_btn(cx, cy))
			{
				close_window(hit);
			}
			else if (hit->in_min_btn(cx, cy))
			{
				min_fn_(close_ctx_, hit);
			}
			else if (hit->in_max_btn(cx, cy))
			{
				place_fn_(close_ctx_, hit, 0); /* toggle maximize */
			}
			else if (hit->in_decor(cx, cy))
			{
				/* A second title-bar click on the same window within the
				 * double-click window toggles maximize (macOS-style). */
				long t = now_ms();
				if (hit == last_click_win_ && (t - last_click_ms_) <= DBLCLICK_MS &&
				    std::abs(cx - last_click_x_) <= DBLCLICK_PX &&
				    std::abs(cy - last_click_y_) <= DBLCLICK_PX)
				{
					last_click_win_ = nullptr; /* consume; a third click is fresh */
					if (hit->resizable())
						place_fn_(close_ctx_, hit, 0);
				}
				else
				{
					last_click_win_ = hit;
					last_click_ms_ = t;
					last_click_x_ = cx;
					last_click_y_ = cy;
					dragging_ = true;
					drag_win_ = hit;
					drag_off_x_ = cx - hit->x;
					drag_off_y_ = cy - hit->y;
				}
			}
			else
			{
				send_mouse(hit, cx, cy, ev.buttons);
			}
		}
	}
	else
	{
		Window *f = reg_.focused();
		if (f)
			send_mouse(f, comp_.cur_x(), comp_.cur_y(), ev.buttons);
	}

	prev_buttons_ = ev.buttons;
}

/* Raise, un-minimize, and focus the switch target (same effect as a taskbar
 * click).  Mirrors the click-to-focus path in handle_mouse(). */
void InputRouter::commit_switch(uint32_t id)
{
	Window *w = reg_.find(id);
	if (w == nullptr)
		return;
	w->minimized = false;
	reg_.z_raise(w);
	focus_fn_(close_ctx_, w);
	comp_.invalidate_all();
}

void InputRouter::handle_kbd(kbd_event_t &ev)
{
	/* --- Alt-Tab task switcher (intercepted before window delivery) --- */
	if (ev.keycode == KEY_LSHIFT)
		shift_held_ = ev.pressed != 0;

	if (ev.keycode == KEY_LALT)
	{
		bool was = alt_held_;
		alt_held_ = ev.pressed != 0;
		/* Releasing Alt while the switcher is up commits to the highlighted
		 * window and swallows this key-up. */
		if (was && !alt_held_ && switcher_active_)
		{
			uint32_t id = comp_.switcher_selected();
			comp_.switcher_end();
			switcher_active_ = false;
			commit_switch(id);
			return;
		}
		/* Otherwise fall through and deliver Alt normally. */
	}

	if (switcher_active_)
	{
		/* While switching, Tab advances, Shift changes direction, Esc cancels;
		 * every other key is swallowed so nothing leaks to the windows. */
		if (ev.pressed && ev.keycode == '\t')
			comp_.switcher_move(shift_held_ ? -1 : 1);
		else if (ev.pressed && ev.keycode == KEY_ESC)
		{
			comp_.switcher_end();
			switcher_active_ = false;
		}
		return;
	}

	/* Alt+Tab with the switcher not yet up: open it and select the next window. */
	if (alt_held_ && ev.pressed && ev.keycode == '\t')
	{
		if (comp_.switcher_begin() > 0)
		{
			switcher_active_ = true;
			comp_.switcher_move(shift_held_ ? -1 : 1);
		}
		return;
	}

	/* --- Default: deliver the key to the focused window --- */
	Window *f = reg_.focused();
	if (!f || f->mbox.empty())
		return;
	mpi_message_t kmsg;
	struct display_key *dk = (struct display_key *)kmsg.data;
	kmsg.header = DISPLAY_KEY;
	dk->window_id = f->id;
	dk->keycode = ev.keycode;
	dk->pressed = ev.pressed;
	ubix::post_message(f->mbox, DISPLAY_KEY, kmsg);
}
