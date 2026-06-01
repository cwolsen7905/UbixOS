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

#include <cstring>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include "input_router.hh"

InputRouter::InputRouter(WindowRegistry &reg,
                         Compositor &comp,
                         void *close_ctx,
                         void (*close_fn)(void *, Window *),
                         void (*min_fn)(void *, Window *),
                         void (*resize_fn)(void *, Window *, int, int))
    : reg_(reg), comp_(comp), dragging_(false), drag_win_(nullptr), drag_off_x_(0), drag_off_y_(0), prev_buttons_(0),
      close_ctx_(close_ctx), close_fn_(close_fn), min_fn_(min_fn), resize_fn_(resize_fn)
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

void InputRouter::send_mouse(Window *w, int cx, int cy, uint8_t buttons)
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

	if (ev.buttons == prev_buttons_)
		return;

	bool pressed = (ev.buttons & 1) != 0;

	if (!pressed && dragging_)
	{
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
			reg_.set_focused(hit);
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
			else if (hit->in_decor(cx, cy))
			{
				dragging_ = true;
				drag_win_ = hit;
				drag_off_x_ = cx - hit->x;
				drag_off_y_ = cy - hit->y;
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

void InputRouter::handle_kbd(kbd_event_t &ev)
{
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
