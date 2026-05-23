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

#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>

#define FONT_PATH "/var/fonts/ROM8X8.DPF"

/* Taskbar geometry */
#define TB_H 32
#define BTN_W 80
#define CLOCK_W 80
#define WIN_BTN_W 96

/* Flyout geometry */
#define FLY_W 120
#define FLY_H 80
#define FLY_ITEM_H (FLY_H / 2)

/* Colours: (r<<16)|(g<<8)|b */
static const uint32_t TB_BG = 0x003C8Cu;
static const uint32_t TB_BTN_N = 0x0050B0u;
static const uint32_t TB_BTN_P = 0x0070D0u;
static const uint32_t TB_SEP = 0x002868u;
static const uint32_t FLY_BG_C = 0x002860u;
static const uint32_t FLY_ITEM_C = 0x004080u;
static const uint32_t COL_WHITE = 0x00FFFFFFu;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static void font_fg(ogBitFont &f, uint32_t c)
{
	f.SetFGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

static void font_bg(ogBitFont &f, uint32_t c)
{
	f.SetBGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

/* ------------------------------------------------------------------ */
/* Time formatting                                                      */
/* ------------------------------------------------------------------ */

#define MINUTE 60
#define HOUR (60 * MINUTE)
#define DAY (24 * HOUR)
#define YEAR (365 * DAY)

static const int month_secs[12] = {
    0,
    DAY * 31,
    DAY * (31 + 29),
    DAY * (31 + 29 + 31),
    DAY * (31 + 29 + 31 + 30),
    DAY * (31 + 29 + 31 + 30 + 31),
    DAY * (31 + 29 + 31 + 30 + 31 + 30),
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31),
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30),
};

static void get_time_str(char *buf)
{
	int t = gettime();
	int year = (t / YEAR) + 1970;
	t -= YEAR * (year - 1970);
	t -= DAY * (((year - 1970) + 1) / 4);

	int month = 0;
	for (int i = 11; i >= 0; i--)
	{
		if ((t - month_secs[i]) > 0)
		{
			month = i;
			break;
		}
	}
	t -= month_secs[month];
	if (month > 1 && (((year - 1970) + 2) % 4) == 0)
		t += DAY;

	t -= (t / DAY) * DAY;
	int hour = t / HOUR;
	t -= hour * HOUR;
	int min = t / MINUTE;
	t -= min * MINUTE;
	int sec = t;

	buf[0] = '0' + (hour / 10);
	buf[1] = '0' + (hour % 10);
	buf[2] = ':';
	buf[3] = '0' + (min / 10);
	buf[4] = '0' + (min % 10);
	buf[5] = ':';
	buf[6] = '0' + (sec / 10);
	buf[7] = '0' + (sec % 10);
	buf[8] = '\0';
}

/* ------------------------------------------------------------------ */
/* MPI flip helper (stateless, takes win_id)                           */
/* ------------------------------------------------------------------ */

static void send_flip_msg(uint32_t win_id)
{
	mpi_message_t msg = {};
	struct display_flip *fl = (struct display_flip *)msg.data;
	msg.header = DISPLAY_FLIP;
	fl->window_id = win_id;
	fl->dirty_x = 0;
	fl->dirty_y = 0;
	fl->dirty_w = 0;
	fl->dirty_h = 0;
	ubix::post_message("views", DISPLAY_FLIP, msg);
}

/* ------------------------------------------------------------------ */
/* TrackedWin                                                           */
/* ------------------------------------------------------------------ */

struct TrackedWin
{
	uint32_t id;
	std::string title;
};

/* ------------------------------------------------------------------ */
/* Launcher — owns the pipe-based process spawning helper              */
/* ------------------------------------------------------------------ */

class Launcher
{
	int fd_ = -1;

      public:
	void init()
	{
		int pfd[2];
		if (::pipe(pfd) != 0)
			return;

		if (::fork() == 0)
		{
			::close(pfd[1]);
			char path[256];
			int len = 0;
			char ch;
			for (;;)
			{
				int r;
				do
				{
					r = ::read(pfd[0], &ch, 1);
				} while (r < 0);
				if (r == 0)
					::_exit(0);
				if (ch != '\0')
				{
					if (len < (int)sizeof(path) - 1)
						path[len++] = ch;
					continue;
				}
				if (len == 0)
					continue;
				path[len] = '\0';
				len = 0;
				if (::fork() == 0)
				{
					char *argv[] = {path, nullptr};
					char *envp[] = {nullptr};
					::execve(path, argv, envp);
					::_exit(1);
				}
			}
		}

		::close(pfd[0]);
		fd_ = pfd[1];
	}

	void launch(const char *path)
	{
		if (fd_ < 0)
			return;
		::write(fd_, path, std::strlen(path) + 1);
	}
};

/* ------------------------------------------------------------------ */
/* Flyout — owns the pop-up menu surface and its window lifecycle      */
/* ------------------------------------------------------------------ */

class Flyout
{
	ogSurface surf_;
	uint32_t win_id_ = 0;
	bool open_ = false;

	void draw(ogBitFont &font)
	{

		/* Flyout Menu */
		surf_.ogFillRect(0, 0, FLY_W - 1, FLY_H - 1, FLY_BG_C);

		/* Terminal Items */
		surf_.ogFillRect(2, 2, FLY_W - 3, FLY_ITEM_H - 3, FLY_ITEM_C);
		font_fg(font, COL_WHITE);
		font_bg(font, FLY_ITEM_C);
		font.PutString(surf_, 8, 10, "Terminal");

		/* About Item */
		surf_.ogFillRect(2, FLY_ITEM_H + 2, FLY_W - 3, FLY_H - 3, FLY_ITEM_C);
		font_fg(font, COL_WHITE);
		font_bg(font, FLY_ITEM_C);
		font.PutString(surf_, 8, FLY_ITEM_H + 10, "About");

		/* Log Out Item */
		surf_.ogFillRect(2, 2 * FLY_ITEM_H + 2, FLY_W - 3, 2 * FLY_ITEM_H - 3, FLY_ITEM_C);
		font_fg(font, COL_WHITE);
		font_bg(font, FLY_ITEM_C);
		font.PutString(surf_, 8, 2 * FLY_ITEM_H + 10, "Log Out");
	}

      public:
	bool is_open() const
	{
		return open_;
	}
	uint32_t win_id() const
	{
		return win_id_;
	}
	int hit_item(int y) const
	{
		return open_ ? (y / FLY_ITEM_H) : -1;
	}

	void show(uint32_t sh, ubix::Mailbox &mbox, ogBitFont &font)
	{
		if (open_)
			return;

		mpi_message_t claim = {};
		struct display_claim_req *creq = (struct display_claim_req *)claim.data;
		claim.header = DISPLAY_CLAIM;
		creq->x = 2;
		creq->y = (int32_t)(sh - TB_H - FLY_H);
		creq->w = FLY_W;
		creq->h = FLY_H;
		creq->sender_pid = ubix::pid();
		creq->no_decor = 1;
		std::strncpy(creq->title, "flyout", sizeof(creq->title) - 1);
		creq->title[sizeof(creq->title) - 1] = '\0';
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		creq->reply[sizeof(creq->reply) - 1] = '\0';
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		mpi_message_t reply;
		while (!mbox.try_fetch(reply))
			ubix::yield();
		if (reply.header != DISPLAY_ACK)
			return;

		struct display_ack *da = (struct display_ack *)reply.data;
		win_id_ = da->window_id;
		surf_.ogAttach(da->shm_base, FLY_W, FLY_H, OG_PIXFMT_32BPP);
		open_ = true;
		draw(font);
		send_flip_msg(win_id_);
	}

	void hide()
	{
		if (!open_)
			return;

		mpi_message_t msg = {};
		struct display_release *rel = (struct display_release *)msg.data;
		msg.header = DISPLAY_RELEASE;
		rel->window_id = win_id_;
		ubix::post_message("views", DISPLAY_RELEASE, msg);

		open_ = false;
		win_id_ = 0;
	}
};

/* ------------------------------------------------------------------ */
/* Taskbar — owns the strip surface, font, window list, and event      */
/*           routing; delegates to Flyout and Launcher                 */
/* ------------------------------------------------------------------ */

class Taskbar
{
	ogSurface surf_;
	ogBitFont font_;
	uint32_t win_id_ = 0;
	uint32_t sw_ = 0;
	uint32_t sh_ = 0;
	std::vector<TrackedWin> tracked_;
	Flyout fly_;
	Launcher launcher_;
	bool btn_pressed_ = false;

	void draw_strip()
	{
		uint32_t btn_color = btn_pressed_ ? TB_BTN_P : TB_BTN_N;
		int sw = (int)sw_;

		surf_.ogFillRect(0, 0, sw - 1, TB_H - 1, TB_BG);
		surf_.ogFillRect(0, 0, sw - 1, 0, TB_SEP);

		/* Launcher button */
		surf_.ogFillRect(2, 2, 2 + BTN_W - 1, TB_H - 3, btn_color);
		surf_.ogRect(2, 2, 2 + BTN_W - 1, TB_H - 3, TB_SEP);
		font_fg(font_, COL_WHITE);
		font_bg(font_, btn_color);
		font_.PutString(surf_, 10, 12, "UbixOS");

		/* Window list */
		int wx = 2 + BTN_W + 4;
		int clock_x = sw - CLOCK_W - 2;
		for (const auto &tw : tracked_)
		{
			if (wx + WIN_BTN_W > clock_x - 4)
				break;
			surf_.ogFillRect(wx, 2, wx + WIN_BTN_W - 1, TB_H - 3, TB_BTN_N);
			surf_.ogRect(wx, 2, wx + WIN_BTN_W - 1, TB_H - 3, TB_SEP);
			font_fg(font_, COL_WHITE);
			font_bg(font_, TB_BTN_N);
			font_.PutString(surf_, wx + 4, 12, tw.title.c_str());
			wx += WIN_BTN_W + 2;
		}

		/* Clock */
		char tstr[12];
		get_time_str(tstr);
		surf_.ogFillRect(clock_x, 2, clock_x + CLOCK_W - 1, TB_H - 3, TB_BTN_N);
		surf_.ogRect(clock_x, 2, clock_x + CLOCK_W - 1, TB_H - 3, TB_SEP);
		font_fg(font_, COL_WHITE);
		font_bg(font_, TB_BTN_N);
		font_.PutString(surf_, clock_x + 8, 12, tstr);
	}

	int winbtn_hit(int mx) const
	{
		int wx = 2 + BTN_W + 4;
		int clock_x = (int)sw_ - CLOCK_W - 2;
		for (int i = 0; i < (int)tracked_.size(); i++)
		{
			if (wx + WIN_BTN_W > clock_x - 4)
				break;
			if (mx >= wx && mx < wx + WIN_BTN_W)
				return i;
			wx += WIN_BTN_W + 2;
		}
		return -1;
	}

	void raise_window(uint32_t id)
	{
		mpi_message_t msg = {};
		struct display_raise *dr = (struct display_raise *)msg.data;
		msg.header = DISPLAY_RAISE;
		dr->window_id = id;
		ubix::post_message("views", DISPLAY_RAISE, msg);
	}

      public:
	bool init(ubix::Mailbox &mbox, const char *font_path)
	{
		launcher_.init();

		/* Query screen geometry */
		mpi_message_t msg = {};
		struct display_query *dq = (struct display_query *)msg.data;
		msg.header = DISPLAY_QUERY;
		std::strncpy(dq->reply, "taskbar", sizeof(dq->reply) - 1);
		dq->reply[sizeof(dq->reply) - 1] = '\0';
		ubix::post_message("views", DISPLAY_QUERY, msg);

		mpi_message_t reply;
		while (!mbox.try_fetch(reply))
			ubix::yield();
		if (reply.header != DISPLAY_INFO)
		{
			std::printf("taskbar: unexpected reply to DISPLAY_QUERY\n");
			return false;
		}
		struct display_info *di = (struct display_info *)reply.data;
		sw_ = di->screen_w;
		sh_ = di->screen_h;

		/* Claim bottom strip */
		mpi_message_t claim = {};
		struct display_claim_req *creq = (struct display_claim_req *)claim.data;
		claim.header = DISPLAY_CLAIM;
		creq->x = 0;
		creq->y = (int32_t)(sh_ - TB_H);
		creq->w = (int32_t)sw_;
		creq->h = TB_H;
		creq->sender_pid = ubix::pid();
		creq->no_decor = 1;
		std::strncpy(creq->title, "taskbar", sizeof(creq->title) - 1);
		creq->title[sizeof(creq->title) - 1] = '\0';
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		creq->reply[sizeof(creq->reply) - 1] = '\0';
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		while (!mbox.try_fetch(reply))
			ubix::yield();
		if (reply.header != DISPLAY_ACK)
		{
			std::printf("taskbar: DISPLAY_CLAIM denied\n");
			return false;
		}

		struct display_ack *da = (struct display_ack *)reply.data;
		win_id_ = da->window_id;
		void *shm = da->shm_base;

		if (!shm)
		{
			std::printf("taskbar: shm_base is NULL\n");
			return false;
		}

		surf_.ogAttach(shm, sw_, TB_H, OG_PIXFMT_32BPP);

		if (!font_.Load(font_path, 0))
		{
			std::printf("taskbar: font load failed\n");
			return false;
		}

		std::printf("taskbar: window %u at 0x%X, %dx%d+%d+%d\n", win_id_, (uint32_t)(uintptr_t)shm, da->w, da->h, da->x, da->y);

		return true;
	}

	void draw()
	{
		draw_strip();
	}

	void send_flip()
	{
		send_flip_msg(win_id_);
	}

	void win_add(uint32_t id, const char *title)
	{
		tracked_.push_back({id, std::string(title)});
	}

	void win_remove(uint32_t id)
	{
		auto it = std::find_if(tracked_.begin(), tracked_.end(), [id](const TrackedWin &w) { return w.id == id; });
		if (it != tracked_.end())
			tracked_.erase(it);
	}

	void on_mouse(const display_mouse_ev *me, ubix::Mailbox &mbox)
	{
		/* Flyout-targeted event */
		if (me->window_id == fly_.win_id())
		{
			if (!(me->buttons & 1) && fly_.is_open())
			{
				int item = fly_.hit_item(me->y);
				fly_.hide();
				if (item == 0)
					launcher_.launch("/bin/term");
			}
			return;
		}

		bool pressed = (me->buttons & 1) != 0;
		if (pressed == btn_pressed_)
			return;
		btn_pressed_ = pressed;
		draw_strip();
		send_flip();

		if (!pressed)
		{
			int wi = winbtn_hit(me->x);
			if (wi >= 0)
			{
				raise_window(tracked_[wi].id);
				return;
			}
			bool in_btn = (me->x >= 2 && me->x < 2 + BTN_W);
			if (in_btn)
			{
				if (fly_.is_open())
					fly_.hide();
				else
					fly_.show(sh_, mbox, font_);
			}
		}
	}
};

/* ------------------------------------------------------------------ */
/* main — thin event loop                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	ubix::Mailbox mbox;
	mbox.assign("taskbar");
	if (!mbox.create())
	{
		std::printf("taskbar: mpi_createMbox failed\n");
		return 1;
	}

	Taskbar tb;
	if (!tb.init(mbox, FONT_PATH))
		return 1;

	tb.draw();
	tb.send_flip();

	int last_sec = -1;
	for (;;)
	{
		int t = gettime() % 60;
		if (t != last_sec)
		{
			last_sec = t;
			tb.draw();
			tb.send_flip();
		}

		mpi_message_t reply;
		while (mbox.try_fetch(reply))
		{
			if (reply.header == DISPLAY_KEY)
				continue;

			if (reply.header == DISPLAY_NOTIFY)
			{
				struct display_notify *dn = (struct display_notify *)reply.data;
				if (dn->added)
					tb.win_add(dn->window_id, dn->title);
				else
					tb.win_remove(dn->window_id);
				tb.draw();
				tb.send_flip();
				continue;
			}

			if (reply.header != DISPLAY_MOUSE)
				continue;

			struct display_mouse_ev *me = (struct display_mouse_ev *)reply.data;
			tb.on_mouse(me, mbox);
		}

		ubix::yield();
	}

	return 0;
}
