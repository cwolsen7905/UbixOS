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
#include <cstdlib>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>
#include <ubistry/ubistry.h>

extern char **environ; /* inherited session env, forwarded to launched apps */

#define FONT_PATH "/var/fonts/ROM8X8.DPF"

/* Taskbar geometry */
#define TB_H 32
#define BTN_W 80
#define CLOCK_W 80
#define WIN_BTN_W 96

/* Start-menu geometry (a Menu sizes its height to its item count). */
#define MENU_W 140
#define MENU_ITEM_H 20
#define MENU_MAX_ITEMS 16

/* Colours: (r<<16)|(g<<8)|b.  All but white are derived from the per-user accent
 * colour (views/theme/accent) by apply_theme(); defaults match the old blue. */
static uint32_t TB_BG = 0x003C8Cu;
static uint32_t TB_BTN_N = 0x0050B0u;
static uint32_t TB_BTN_P = 0x0070D0u;
static uint32_t TB_SEP = 0x002868u;
static uint32_t FLY_BG_C = 0x002860u;
static uint32_t FLY_ITEM_C = 0x004080u;
static const uint32_t COL_WHITE = 0x00FFFFFFu;

/**
 * Scale a packed 0xRRGGBB colour's brightness by num/den (clamped to 0xFF).
 */
static uint32_t scale_color(uint32_t c, int num, int den)
{
	int r = (int)((c >> 16) & 0xFF) * num / den;
	int g = (int)((c >> 8) & 0xFF) * num / den;
	int b = (int)(c & 0xFF) * num / den;
	if (r > 255)
		r = 255;
	if (g > 255)
		g = 255;
	if (b > 255)
		b = 255;
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/**
 * Re-derive the taskbar palette from the session user's accent colour.  Called
 * at startup and whenever a DISPLAY_THEME message arrives.
 */
static void apply_theme(void)
{
	const char *user = getenv("USER");
	int accent;

	if (ubistry_get_for_int((user && user[0]) ? user : nullptr, "views/theme/accent", &accent) != 0)
		return; /* keep current palette if the key is missing */

	uint32_t a = (uint32_t)accent & 0x00FFFFFFu;
	TB_BG = scale_color(a, 5, 10);
	TB_SEP = scale_color(a, 3, 10);
	TB_BTN_N = scale_color(a, 8, 10);
	TB_BTN_P = scale_color(a, 12, 10);
	FLY_BG_C = scale_color(a, 4, 10);
	FLY_ITEM_C = scale_color(a, 7, 10);
}

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
					/* Pass our inherited session environment (SHELL,
					 * HOME, USER … set by vlogin) through to the app so
					 * e.g. term can launch the user's shell. */
					char *argv[] = {path, nullptr};
					::execve(path, argv, environ);
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
/* Menu — a pop-up loaded from the registry.  An entry is a leaf (an    */
/*        exec action) or a submenu (has an items/ container).          */
/* ------------------------------------------------------------------ */

struct MenuItem
{
	std::string label;
	std::string exec; /* leaf action: a program path, or "@builtin" */
	std::string path; /* registry path of this entry (for submenus) */
	bool submenu = false;
};

class Menu
{
	ogSurface surf_;
	uint32_t win_id_ = 0;
	bool open_ = false;
	int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
	std::vector<MenuItem> items_;

	void draw(ogBitFont &font)
	{
		surf_.ogFillRect(0, 0, w_ - 1, h_ - 1, FLY_BG_C);
		for (int i = 0; i < (int)items_.size(); i++)
		{
			int top = i * MENU_ITEM_H;
			surf_.ogFillRect(2, top + 2, w_ - 3, top + MENU_ITEM_H - 3, FLY_ITEM_C);
			font_fg(font, COL_WHITE);
			font_bg(font, FLY_ITEM_C);
			font.PutString(surf_, 8, top + 6, items_[i].label.c_str());
			if (items_[i].submenu)
				font.PutString(surf_, w_ - 12, top + 6, ">");
		}
	}

	void load_fallback()
	{
		items_.clear();
		items_.push_back({"Terminal", "/bin/term", "", false});
		items_.push_back({"About", "@about", "", false});
		items_.push_back({"Log Out", "@logout", "", false});
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
	int count() const
	{
		return (int)items_.size();
	}
	int x() const
	{
		return x_;
	}
	int y() const
	{
		return y_;
	}
	int w() const
	{
		return w_;
	}
	const MenuItem *item(int i) const
	{
		return (i >= 0 && i < (int)items_.size()) ? &items_[i] : nullptr;
	}

	int hit_item(int y) const
	{
		if (!open_)
			return -1;
		int i = y / MENU_ITEM_H;
		return (i >= 0 && i < (int)items_.size()) ? i : -1;
	}

	/* Populate from a registry container; fall back to a built-in menu if the
	 * registry is unavailable or empty so the desktop is never broken. */
	void load(const char *regpath)
	{
		char names[UB_NAMES_MAX];
		int n = ubistry_enum(regpath, names, sizeof(names));

		items_.clear();
		if (n <= 0)
		{
			load_fallback();
			return;
		}

		std::string ns(names);
		size_t start = 0;
		while (start < ns.size() && (int)items_.size() < MENU_MAX_ITEMS)
		{
			size_t nl = ns.find('\n', start);
			std::string child = ns.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
			start = (nl == std::string::npos) ? ns.size() : nl + 1;
			if (child.empty())
				continue;

			MenuItem it;
			it.path = std::string(regpath) + "/" + child;

			char buf[128];
			if (ubistry_get_str((it.path + "/label").c_str(), buf, sizeof(buf)) == 0)
				it.label = buf;
			else
				it.label = child;

			char scratch[UB_NAMES_MAX];
			it.submenu = (ubistry_enum((it.path + "/items").c_str(), scratch, sizeof(scratch)) >= 0);
			if (!it.submenu && ubistry_get_str((it.path + "/exec").c_str(), buf, sizeof(buf)) == 0)
				it.exec = buf;

			items_.push_back(it);
		}
		if (items_.empty())
			load_fallback();
	}

	void show(int x, int y, ubix::Mailbox &mbox, ogBitFont &font)
	{
		if (open_ || items_.empty())
			return;

		w_ = MENU_W;
		h_ = (int)items_.size() * MENU_ITEM_H;
		x_ = x < 0 ? 0 : x;
		y_ = y < 0 ? 0 : y;

		mpi_message_t claim = {};
		struct display_claim_req *creq = (struct display_claim_req *)claim.data;
		claim.header = DISPLAY_CLAIM;
		creq->x = x_;
		creq->y = y_;
		creq->w = w_;
		creq->h = h_;
		creq->sender_pid = ubix::pid();
		creq->no_decor = 1;
		std::strncpy(creq->title, "menu", sizeof(creq->title) - 1);
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
		surf_.ogAttach(da->shm_base, (uint32_t)w_, (uint32_t)h_, OG_PIXFMT_32BPP);
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
/*           routing; delegates to Menu and Launcher                   */
/* ------------------------------------------------------------------ */

class Taskbar
{
	ogSurface surf_;
	ogBitFont font_;
	uint32_t win_id_ = 0;
	uint32_t sw_ = 0;
	uint32_t sh_ = 0;
	std::vector<TrackedWin> tracked_;
	Menu start_menu_;
	Menu submenu_;
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

		std::printf("taskbar: window %u at 0x%X, %dx%d+%d+%d\n",
		            win_id_,
		            (uint32_t)(uintptr_t)shm,
		            da->w,
		            da->h,
		            da->x,
		            da->y);

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
		auto it =
		    std::find_if(tracked_.begin(), tracked_.end(), [id](const TrackedWin &w) { return w.id == id; });
		if (it != tracked_.end())
			tracked_.erase(it);
	}

	void close_menus()
	{
		submenu_.hide();
		start_menu_.hide();
	}

	/* Run a leaf entry: a built-in "@action" or a program to launch. */
	void dispatch(const MenuItem &it)
	{
		if (it.exec.empty())
			return;
		if (it.exec[0] == '@')
		{
			if (it.exec == "@logout")
				::exit(0); /* taskbar exit ends the session; vlogin resumes */
			/* @about and other built-ins: not yet implemented */
			return;
		}
		launcher_.launch(it.exec.c_str());
	}

	/* Open the submenu for a top-level entry beside its row, on-screen. */
	void open_submenu(const MenuItem &parent, int row, ubix::Mailbox &mbox)
	{
		submenu_.hide();
		submenu_.load((parent.path + "/items").c_str());

		int sx = start_menu_.x() + start_menu_.w();
		int sy = start_menu_.y() + row * MENU_ITEM_H;
		int sh_px = submenu_.count() * MENU_ITEM_H;
		if (sx + MENU_W > (int)sw_)
			sx = start_menu_.x() - MENU_W; /* flip to the left edge */
		if (sy + sh_px > (int)sh_ - TB_H)
			sy = (int)sh_ - TB_H - sh_px;
		submenu_.show(sx, sy, mbox, font_);
	}

	void on_mouse(const display_mouse_ev *me, ubix::Mailbox &mbox)
	{
		/* Submenu click: dispatch the chosen leaf. */
		if (me->window_id == submenu_.win_id() && submenu_.is_open())
		{
			if (!(me->buttons & 1))
			{
				const MenuItem *it = submenu_.item(submenu_.hit_item(me->y));
				MenuItem sel = it ? *it : MenuItem();
				close_menus();
				if (it)
					dispatch(sel);
			}
			return;
		}

		/* Top-level menu click: cascade a submenu or dispatch a leaf. */
		if (me->window_id == start_menu_.win_id() && start_menu_.is_open())
		{
			if (!(me->buttons & 1))
			{
				int row = start_menu_.hit_item(me->y);
				const MenuItem *it = start_menu_.item(row);
				if (it && it->submenu)
				{
					open_submenu(*it, row, mbox);
				}
				else
				{
					MenuItem sel = it ? *it : MenuItem();
					close_menus();
					if (it)
						dispatch(sel);
				}
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
				close_menus();
				raise_window(tracked_[wi].id);
				return;
			}
			bool in_btn = (me->x >= 2 && me->x < 2 + BTN_W);
			if (in_btn)
			{
				if (start_menu_.is_open())
				{
					close_menus();
				}
				else
				{
					start_menu_.load("/views/startmenu");
					int mh = start_menu_.count() * MENU_ITEM_H;
					start_menu_.show(2, (int)sh_ - TB_H - mh, mbox, font_);
				}
			}
			else
			{
				close_menus(); /* click elsewhere dismisses the menu */
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

	apply_theme(); /* derive the palette from the user's accent before first paint */

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

			if (reply.header == DISPLAY_THEME)
			{
				apply_theme();
				tb.draw();
				tb.send_flip();
				continue;
			}

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
