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
#include <cstdio>
#include <cstring>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include "window_manager.hh"

#define PAGE_SIZE 4096

extern "C" int _sys_shareregion(int dst_pid, void *vaddr, uint32_t size, uintptr_t *out_vaddr);

static int share_buffer(int dst_pid, void *buf, uint32_t size, uintptr_t *client_vaddr)
{
	return _sys_shareregion(dst_pid, buf, size, client_vaddr);
}

WindowManager::WindowManager()
    : comp_(reg_),
      input_(
          reg_,
          comp_,
          this,
          [](void *ctx, Window *w) { static_cast<WindowManager *>(ctx)->close_window(w); },
          [](void *ctx, Window *w) { static_cast<WindowManager *>(ctx)->minimize_window(w); },
          [](void *ctx, Window *w, int nw, int nh) { static_cast<WindowManager *>(ctx)->resize_window(w, nw, nh); },
          [](void *ctx, Window *w, int mode) { static_cast<WindowManager *>(ctx)->place_window(w, mode); },
          [](void *ctx, Window *w) { static_cast<WindowManager *>(ctx)->set_focus(w); })
{
}

int WindowManager::init()
{
	return comp_.init();
}

void WindowManager::startup()
{
	comp_.startup();
}

void WindowManager::composite_all()
{
	comp_.composite_all();
}

void WindowManager::notify_taskbar(Window *w, uint8_t added)
{
	static const char tb[] = "taskbar";
	mpi_message_t nm;
	struct display_notify *dn = (struct display_notify *)nm.data;
	nm.header = DISPLAY_NOTIFY;
	dn->window_id = w->id;
	dn->added = added;
	std::strncpy(dn->title, w->title.c_str(), sizeof(dn->title) - 1);
	dn->title[sizeof(dn->title) - 1] = '\0';
	ubix::post_message(tb, DISPLAY_NOTIFY, nm);
}

void WindowManager::set_focus(Window *w)
{
	reg_.set_focused(w);

	/* Tell the taskbar which decorated window is active so it can highlight the
	 * matching tab.  Only decorated windows have a tab; focusing a no-decor
	 * surface (taskbar, vlogin) or nothing clears the highlight (id 0).  Dedupe
	 * so redundant focus updates don't spam the taskbar mailbox — this also makes
	 * the taskbar's own claim a no-op (id 0 == initial last_focus_sent_), so we
	 * never post DISPLAY_FOCUS into the middle of its claim handshake. */
	uint32_t id = (w != nullptr && w->decor_h > 0) ? w->id : 0;
	if (id == last_focus_sent_)
		return;
	last_focus_sent_ = id;

	static const char tb[] = "taskbar";
	mpi_message_t fm;
	struct display_focus *df = (struct display_focus *)fm.data;
	fm.header = DISPLAY_FOCUS;
	df->window_id = id;
	ubix::post_message(tb, DISPLAY_FOCUS, fm);
}

void WindowManager::close_window(Window *w)
{
	/* Two-phase close: send DISPLAY_CLOSE to the client, remove the window
	 * from the visible stack and focus, but keep it in the registry so that
	 * handle_release() can free the buffer when the client acknowledges.
	 * This prevents freeing the shared buffer before the client has stopped
	 * writing to it, which would corrupt views' heap and freeze the GUI. */
	w->closing = true;
	if (w->decor_h > 0)
		notify_taskbar(w, 0);
	reg_.z_remove(w);
	set_focus(reg_.z_stack().empty() ? nullptr : reg_.z_stack().back());
	comp_.invalidate_all();

	if (!w->mbox.empty())
	{
		mpi_message_t cm;
		struct display_close *dc = (struct display_close *)cm.data;
		cm.header = DISPLAY_CLOSE;
		dc->window_id = w->id;
		ubix::post_message(w->mbox, DISPLAY_CLOSE, cm);
	}
	else
	{
		/* No client mailbox — free immediately */
		std::free(w->buf);
		reg_.destroy(w);
	}
}

void WindowManager::minimize_window(Window *w)
{
	w->minimized = true;
	/* If it had focus, move focus to the topmost still-visible window. */
	if (reg_.focused() == w)
	{
		Window *nf = nullptr;
		for (auto it = reg_.z_stack().rbegin(); it != reg_.z_stack().rend(); ++it)
			if (!(*it)->minimized)
			{
				nf = *it;
				break;
			}
		set_focus(nf);
	}
	comp_.invalidate_all();
	/* The taskbar already shows a button for this window (from DISPLAY_NOTIFY);
	 * clicking it sends DISPLAY_RAISE, which restores it in handle_raise(). */
}

void WindowManager::resize_window(Window *w, int new_w, int new_h)
{
	if (new_w < 1 || new_h < 1 || (new_w == w->w && new_h == w->h))
		return;

	/* Allocate and share a fresh buffer of the new size for the client. */
	uint32_t pitch = (uint32_t)new_w * WIN_BPP;
	uint32_t buf_size = pitch * (uint32_t)new_h;
	uint32_t alloc_size = (buf_size + PAGE_SIZE - 1) & ~(uint32_t)(PAGE_SIZE - 1);
	void *nbuf = std::aligned_alloc(PAGE_SIZE, alloc_size);
	if (!nbuf)
		return;
	std::memset(nbuf, 0, buf_size);

	uintptr_t client_vaddr = 0;
	if (share_buffer(w->sender_pid, nbuf, buf_size, &client_vaddr) != 0)
	{
		std::free(nbuf);
		return;
	}

	/* Keep the old buffer mapped: the client may still be mid-write, and freeing
	 * it here risks corrupting views' heap (same hazard as the close path).  A
	 * small per-resize leak, traded for safety. */
	w->buf = nbuf;
	w->w = new_w;
	w->h = new_h;
	w->pitch = pitch;

	mpi_message_t m = {};
	struct display_winresize *wr = (struct display_winresize *)m.data;
	m.header = DISPLAY_WINRESIZE;
	wr->window_id = w->id;
	wr->shm_base = (void *)client_vaddr;
	wr->pitch = (uint16_t)pitch;
	wr->w = new_w;
	wr->h = new_h;
	if (!w->mbox.empty())
		ubix::post_message(w->mbox, DISPLAY_WINRESIZE, m);

	comp_.invalidate_all();
}

void WindowManager::place_window(Window *w, int mode)
{
	if (!w->resizable())
		return;

	int sw = (int)comp_.screen_w();
	int sh = (int)comp_.screen_h();
	const int reserve = 32; /* leave room for the taskbar at the bottom */
	int avail_h = sh - reserve;

	/* Restore from a maximized/snapped state (the maximize button toggles). */
	if (mode == 0 && w->maximized)
	{
		w->x = w->saved_x;
		w->y = w->saved_y;
		resize_window(w, w->saved_w, w->saved_h);
		w->maximized = false;
		comp_.invalidate_all();
		return;
	}

	if (!w->maximized)
	{
		w->saved_x = w->x;
		w->saved_y = w->y;
		w->saved_w = w->w;
		w->saved_h = w->h;
	}

	int tx, tw;
	if (mode == 1)
	{
		tx = 0;
		tw = sw / 2; /* left half */
	}
	else if (mode == 2)
	{
		tx = sw / 2;
		tw = sw - sw / 2; /* right half */
	}
	else
	{
		tx = 0;
		tw = sw; /* maximize */
	}

	w->x = tx;
	w->y = 0;
	resize_window(w, tw, avail_h - w->decor_h); /* resize_window clamps to the window's max_* */
	/* Centre horizontally if a fixed/constrained width came out narrower. */
	if (w->w < tw)
		w->x = tx + (tw - w->w) / 2;
	w->maximized = true;
	comp_.invalidate_all();
}

void WindowManager::handle_query(struct display_query *dq)
{
	if (!dq->reply[0])
		return;
	mpi_message_t info;
	struct display_info *di = (struct display_info *)info.data;
	info.header = DISPLAY_INFO;
	di->screen_w = comp_.screen_w();
	di->screen_h = comp_.screen_h();
	di->bpp = 32;
	ubix::post_message(dq->reply, DISPLAY_INFO, info);
}

void WindowManager::handle_claim(struct display_claim_req *creq)
{
	mpi_message_t ack;
	struct display_ack *da = (struct display_ack *)ack.data;

	auto deny = [&]()
	{
		ack.header = DISPLAY_DENIED;
		if (creq->reply[0])
			ubix::post_message(creq->reply, DISPLAY_DENIED, ack);
	};

	if (creq->sender_pid <= 0 || creq->w <= 0 || creq->h <= 0)
	{
		deny();
		return;
	}

	int dh = creq->no_decor ? 0 : DECOR_H;

	int32_t wx = creq->x, wy = creq->y;
	int32_t ww = creq->w, wh = creq->h;

	if (dh > 0)
		reg_.next_cascade(&wx, &wy, ww, (int32_t)dh, wh, comp_.screen_w(), comp_.screen_h());

	if (wx < 0)
		wx = 0;
	if (wy < 0)
		wy = 0;
	if (wx + ww > (int32_t)comp_.screen_w())
		ww = (int32_t)comp_.screen_w() - wx;
	if (wy + (int32_t)dh + wh > (int32_t)comp_.screen_h())
		wh = (int32_t)comp_.screen_h() - wy - (int32_t)dh;

	uint32_t pitch = (uint32_t)ww * WIN_BPP;
	uint32_t buf_size = pitch * (uint32_t)wh;
	uint32_t alloc_size = (buf_size + PAGE_SIZE - 1) & ~(uint32_t)(PAGE_SIZE - 1);
	void *buf = std::aligned_alloc(PAGE_SIZE, alloc_size);
	if (!buf)
	{
		deny();
		return;
	}
	std::memset(buf, 0, buf_size);

	uintptr_t client_vaddr = 0;
	if (share_buffer(creq->sender_pid, buf, buf_size, &client_vaddr) != 0)
	{
		std::free(buf);
		deny();
		return;
	}

	Window *w = reg_.alloc();
	w->id = 0; /* filled by registry — assign separately */
	w->x = wx;
	w->y = wy;
	w->w = ww;
	w->h = wh;
	w->pitch = pitch;
	w->buf = buf;
	w->decor_h = dh;
	w->title = creq->title;
	w->mbox = creq->reply;
	w->sender_pid = creq->sender_pid;
	w->wants_motion = creq->wants_motion != 0;
	/* Resize constraints: default to fixed at the granted size. */
	w->min_w = creq->min_w > 0 ? creq->min_w : ww;
	w->min_h = creq->min_h > 0 ? creq->min_h : wh;
	w->max_w = creq->max_w > 0 ? creq->max_w : ww;
	w->max_h = creq->max_h > 0 ? creq->max_h : wh;

	/* Assign a stable ID after alloc so find() works from this point on */
	static uint32_t next_id = 1;
	w->id = next_id++;

	reg_.z_push(w);

	ack.header = DISPLAY_ACK;
	da->window_id = w->id;
	da->shm_base = (void *)client_vaddr;
	da->pitch = (uint16_t)pitch;
	da->x = wx;
	da->y = wy + dh;
	da->w = ww;
	da->h = wh;
	if (creq->reply[0])
		ubix::post_message(creq->reply, DISPLAY_ACK, ack);

	std::printf("views: window %u pid %d %dx%d+%d+%d shm=0x%lX\n",
	            w->id,
	            creq->sender_pid,
	            ww,
	            wh,
	            wx,
	            wy,
	            (unsigned long)client_vaddr);

	if (dh > 0)
		notify_taskbar(w, 1);
	/* Focus the new window AFTER its ACK (and the tab NOTIFY) are queued, so the
	 * DISPLAY_FOCUS never lands in the middle of the client's claim handshake. */
	set_focus(w);
	comp_.invalidate_all();
}

void WindowManager::handle_flip(struct display_flip *fl)
{
	Window *w = reg_.find(fl->window_id);
	if (!w)
		return;
	if (fl->dirty_w > 0 && fl->dirty_h > 0)
	{
		int sx = w->x + (int)fl->dirty_x;
		int sy = w->y + w->decor_h + (int)fl->dirty_y;
		comp_.invalidate(sx, sy, (int)fl->dirty_w, (int)fl->dirty_h);
	}
	else
	{
		comp_.invalidate_all();
	}
}

void WindowManager::handle_release(struct display_release *rel)
{
	Window *w = reg_.find(rel->window_id);
	if (!w)
		return;
	if (!w->closing)
	{
		/* Normal client-initiated close (e.g. vlogin releasing its window) */
		if (w->decor_h > 0)
			notify_taskbar(w, 0);
		reg_.z_remove(w);
		if (reg_.focused() == w)
			set_focus(reg_.z_stack().empty() ? nullptr : reg_.z_stack().back());
		comp_.invalidate_all();
	}
	/* closing==true: already removed from z-stack by close_window(); just free */
	std::free(w->buf);
	reg_.destroy(w); /* w is dangling after this point */
}

void WindowManager::reap_window(Window *w)
{
	/* The client is gone, so there is no DISPLAY_RELEASE handshake to wait for
	 * and no mailbox to message: drop the window from the visible stack, hand
	 * focus to whatever is left, free the shared buffer (its last reference, so
	 * the physical pages are reclaimed), and destroy the record. */
	if (w->decor_h > 0)
		notify_taskbar(w, 0);
	reg_.z_remove(w);
	if (reg_.focused() == w)
		set_focus(reg_.z_stack().empty() ? nullptr : reg_.z_stack().back());
	std::free(w->buf);
	reg_.destroy(w); /* w is dangling after this point */
}

void WindowManager::reap_dead_clients()
{
	/* Collect first, then free: reap_window() mutates the registry, so we must
	 * not free while iterating the snapshot. */
	bool reaped = false;
	for (Window *w : reg_.all())
	{
		if (w->sender_pid <= 0)
			continue;
		if (::kill(w->sender_pid, 0) == 0)
			continue; /* client still alive */
		std::printf("views: reaping window %u (dead client pid %d)\n", w->id, w->sender_pid);
		reap_window(w);
		reaped = true;
	}
	if (reaped)
		comp_.invalidate_all();
}

void WindowManager::handle_raise(struct display_raise *dr)
{
	Window *w = reg_.find(dr->window_id);
	if (!w)
		return;
	w->minimized = false; /* a taskbar click restores a minimized window */
	reg_.z_raise(w);
	set_focus(w);
	comp_.invalidate_all();
}

void WindowManager::handle_settitle(struct display_settitle *st)
{
	Window *w = reg_.find(st->window_id);
	if (!w)
		return;
	st->title[sizeof(st->title) - 1] = '\0';
	w->title = st->title;
	if (w->decor_h > 0)
		notify_taskbar(w, 1); /* refresh the taskbar's window label too */
	comp_.invalidate_all();
}

void WindowManager::handle_refresh_desktop()
{
	comp_.set_desktop_from_registry();
	comp_.invalidate_all();
}

void WindowManager::handle_set_user(struct display_set_user *su)
{
	su->user[sizeof(su->user) - 1] = '\0';
	comp_.set_active_user(su->user);
	comp_.invalidate_all();
}

void WindowManager::handle_setmode(struct display_setmode *sm)
{
	/* Ask the systemtask to switch the VBE mode (the V86 BIOS call is only safe
	 * there).  The 0x82 reply comes back to our mailbox and drives the actual
	 * re-map in handle_vesa_ready(). */
	mpi_message_t m = {};
	m.data[0] = 'v';
	m.data[1] = 'i';
	m.data[2] = 'e';
	m.data[3] = 'w';
	m.data[4] = 's';
	m.data[5] = '\0';
	*(uint16_t *)&m.data[64] = sm->mode;
	m.header = 0x82;
	mode_pending_ = true;
	ubix::post_message("system", 0x82, m);
}

void WindowManager::handle_vesa_ready()
{
	if (!mode_pending_)
		return;
	mode_pending_ = false;

	/* The new mode is live in the kernel; re-map our framebuffer, reposition any
	 * off-screen windows, tell the taskbar to re-claim its strip, and repaint. */
	comp_.reopen_fb();
	reg_.clamp_to((int)comp_.screen_w(), (int)comp_.screen_h());

	mpi_message_t m = {};
	struct display_resize *dr = (struct display_resize *)m.data;
	m.header = DISPLAY_RESIZE;
	dr->screen_w = comp_.screen_w();
	dr->screen_h = comp_.screen_h();
	ubix::post_message("taskbar", DISPLAY_RESIZE, m);

	comp_.invalidate_all();
}

void WindowManager::flush()
{
	comp_.flush();
}

void WindowManager::handle_mouse(mouse_event_t &ev)
{
	input_.handle_mouse(ev);
}

void WindowManager::handle_kbd(kbd_event_t &ev)
{
	input_.handle_kbd(ev);
}

void WindowManager::dispatch(uint32_t id, void *data)
{
	using Fn = void (*)(WindowManager &, void *);
	static const struct
	{
		uint32_t id;
		Fn fn;
	} table[] = {
	    {DISPLAY_QUERY, [](WindowManager &wm, void *d) { wm.handle_query((struct display_query *)d); }},
	    {DISPLAY_CLAIM, [](WindowManager &wm, void *d) { wm.handle_claim((struct display_claim_req *)d); }},
	    {DISPLAY_FLIP, [](WindowManager &wm, void *d) { wm.handle_flip((struct display_flip *)d); }},
	    {DISPLAY_RELEASE, [](WindowManager &wm, void *d) { wm.handle_release((struct display_release *)d); }},
	    {DISPLAY_RAISE, [](WindowManager &wm, void *d) { wm.handle_raise((struct display_raise *)d); }},
	    {DISPLAY_SETTITLE, [](WindowManager &wm, void *d) { wm.handle_settitle((struct display_settitle *)d); }},
	    {DISPLAY_REFRESH_DESKTOP,
	     [](WindowManager &wm, void *d)
	     {
		     (void)d;
		     wm.handle_refresh_desktop();
	     }},
	    {DISPLAY_SET_USER, [](WindowManager &wm, void *d) { wm.handle_set_user((struct display_set_user *)d); }},
	    {DISPLAY_SETMODE, [](WindowManager &wm, void *d) { wm.handle_setmode((struct display_setmode *)d); }},
	    {0x82,
	     [](WindowManager &wm, void *d)
	     {
		     (void)d;
		     wm.handle_vesa_ready();
	     }},
	};
	for (const auto &e : table)
		if (e.id == id)
		{
			e.fn(*this, data);
			return;
		}
}
