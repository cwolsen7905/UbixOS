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
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogPixelFmt.h>
#include <ubistry/ubistry.h>

extern char **environ; /* inherited session env, forwarded to launched apps */

#define FONT_PATH "/var/fonts/DejaVuSans.ttf"
#define FONT_SIZE 14

/* Taskbar geometry */
#define TB_H 32
#define BTN_W 104   /* start button: hamburger icon + "uBixOS" */
#define CLOCK_W 160 /* fits "MM/DD  HH:MM:SS" */
#define WIN_BTN_W 96
#define TRAY_W 34 /* system-tray area (volume) left of the clock */

/* Start-menu geometry (a Menu sizes its height to its item count). */
#define MENU_W 180
#define MENU_ITEM_H 28
#define MENU_MAX_ITEMS 16
#define MENU_FOOTER_H 32 /* user + power row at the bottom of the start menu */

/* Colours: (r<<16)|(g<<8)|b.  All but white are derived from the per-user accent
 * colour (views/theme/accent) by apply_theme(); defaults are slate-derived to
 * match the compositor's calm default title-bar accent (0x333C4C). */
static uint32_t TB_BG = 0x0028303Cu;      /* taskbar fill (slightly darker than title bars) */
static uint32_t TB_BTN_N = 0x00333C4Cu;   /* window-button fill (= title-bar slate) */
static uint32_t TB_BTN_P = 0x00424E62u;   /* pressed / active accent (lighter slate) */
static uint32_t TB_SEP = 0x00191E26u;     /* top hairline / separators */
static uint32_t FLY_BG_C = 0x002D3644u;   /* start-menu panel fill */
static uint32_t FLY_ITEM_C = 0x00424E62u; /* start-menu item highlight */
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

	/* Derived so the window-button fill equals the title-bar accent (a) and the
	 * bar sits a touch darker — matching the compositor's chrome. */
	uint32_t a = (uint32_t)accent & 0x00FFFFFFu;
	TB_BG = scale_color(a, 8, 10);
	TB_SEP = scale_color(a, 5, 10);
	TB_BTN_N = scale_color(a, 10, 10);
	TB_BTN_P = scale_color(a, 13, 10);
	FLY_BG_C = scale_color(a, 9, 10);
	FLY_ITEM_C = scale_color(a, 13, 10);
}

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static void font_fg(ogScalableFont &f, uint32_t c)
{
	f.SetFGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

static void font_bg(ogScalableFont &f, uint32_t c)
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

/* Format "MM/DD  HH:MM:SS" into buf (needs >= 16 bytes). */
static void get_datetime_str(char *buf)
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

	int day = (t / DAY) + 1;
	t -= (t / DAY) * DAY;
	int hour = t / HOUR;
	t -= hour * HOUR;
	int min = t / MINUTE;
	t -= min * MINUTE;
	int sec = t;
	int mon = month + 1;

	buf[0] = '0' + (mon / 10);
	buf[1] = '0' + (mon % 10);
	buf[2] = '/';
	buf[3] = '0' + (day / 10);
	buf[4] = '0' + (day % 10);
	buf[5] = ' ';
	buf[6] = ' ';
	buf[7] = '0' + (hour / 10);
	buf[8] = '0' + (hour % 10);
	buf[9] = ':';
	buf[10] = '0' + (min / 10);
	buf[11] = '0' + (min % 10);
	buf[12] = ':';
	buf[13] = '0' + (sec / 10);
	buf[14] = '0' + (sec % 10);
	buf[15] = '\0';
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
					 * e.g. term can launch the user's shell.  Split on
					 * spaces so callers can pass args ("/bin/settings about"). */
					char *argv[8];
					int ac = 0;
					for (char *t = std::strtok(path, " "); t != nullptr && ac < 7;
					     t = std::strtok(nullptr, " "))
						argv[ac++] = t;
					argv[ac] = nullptr;
					if (ac > 0)
						::execve(argv[0], argv, environ);
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
	bool footer_ = false; /* show the user + power row (start menu only) */
	int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
	int hover_item_ = -1; /* item under the cursor (highlight) */
	int active_row_ = -1; /* row whose submenu is open (stays highlighted) */
	std::vector<MenuItem> items_;

	void draw(ogScalableFont &font)
	{
		/* Flat panel: items render straight on the panel fill (no per-item raised
		 * boxes) for a modern pop-over look.  A 1px accent strip down the left
		 * edge gives the menu a bit of identity.  The hovered item — and the row
		 * whose submenu is open — get a highlight fill. */
		surf_.ogFillRect(0, 0, w_ - 1, h_ - 1, FLY_BG_C);
		surf_.ogFillRect(0, 0, 2, h_ - 1, FLY_ITEM_C);
		for (int i = 0; i < (int)items_.size(); i++)
		{
			int top = i * MENU_ITEM_H;
			int ty = top + (MENU_ITEM_H - FONT_SIZE) / 2;
			bool hl = (i == hover_item_ || i == active_row_);
			uint32_t bg = hl ? FLY_ITEM_C : FLY_BG_C;
			if (hl)
				surf_.ogFillRect(2, top, w_ - 1, top + MENU_ITEM_H - 1, FLY_ITEM_C);
			font_fg(font, COL_WHITE);
			font_bg(font, bg);
			font.PutString(surf_, 14, ty, items_[i].label.c_str());
			if (items_[i].submenu)
				font.PutString(surf_, w_ - 16, ty, ">");
		}
		if (footer_)
			draw_footer(font);
	}

	/* Bottom row: the logged-in user on the left, a power button on the right. */
	void draw_footer(ogScalableFont &font)
	{
		int ftop = (int)items_.size() * MENU_ITEM_H;
		int fcy = ftop + MENU_FOOTER_H / 2;

		surf_.ogFillRect(8, ftop, w_ - 9, ftop, FLY_ITEM_C); /* separator */

		const char *user = getenv("USER");
		font_fg(font, 0x00C8C8D2u);
		font_bg(font, FLY_BG_C);
		font.PutString(surf_, 14, fcy - FONT_SIZE / 2, (user && user[0]) ? user : "user");

		/* Power glyph: a ring with a vertical line breaking its top. */
		int pcx = w_ - 18, pcy = fcy;
		surf_.ogCircle(pcx, pcy, 7, 0x00FF6E6Eu);
		surf_.ogFillRect(pcx - 1, pcy - 10, pcx + 1, pcy - 4, FLY_BG_C);
		surf_.ogVLine(pcx, pcy - 9, pcy - 1, 0x00FF6E6Eu);
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

	/* Show the user + power footer row (start menu only). */
	void set_footer(bool on)
	{
		footer_ = on;
	}

	/* Update the hovered item (negative y / out-of-range clears it).  Returns
	 * true if it changed (caller should redraw). */
	bool set_hover(int item)
	{
		if (item == hover_item_)
			return false;
		hover_item_ = item;
		return true;
	}

	/* Mark a row as "active" (its submenu is open) so it stays highlighted. */
	void set_active_row(int row)
	{
		active_row_ = row;
	}

	/* Repaint and flip (used on hover changes). */
	void redraw(ogScalableFont &font)
	{
		if (!open_)
			return;
		draw(font);
		send_flip_msg(win_id_);
	}

	/* Total height including the footer when enabled. */
	int height() const
	{
		return (int)items_.size() * MENU_ITEM_H + (footer_ ? MENU_FOOTER_H : 0);
	}

	/* True if (x,y) hits the footer's power button. */
	bool footer_power_hit(int x, int y) const
	{
		if (!open_ || !footer_)
			return false;
		int ftop = (int)items_.size() * MENU_ITEM_H;
		return (y >= ftop && y < ftop + MENU_FOOTER_H && x >= w_ - 32);
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

	void show(int x, int y, ubix::Mailbox &mbox, ogScalableFont &font)
	{
		if (open_ || items_.empty())
			return;

		w_ = MENU_W;
		h_ = height();
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
		creq->wants_motion = 1; /* receive hover motion for highlighting */
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
		hover_item_ = -1;
		active_row_ = -1;
	}
};

/* ------------------------------------------------------------------ */
/* Taskbar — owns the strip surface, font, window list, and event      */
/*           routing; delegates to Menu and Launcher                   */
/* ------------------------------------------------------------------ */

class Taskbar
{
	ogSurface surf_;
	ogScalableFont font_;
	uint32_t win_id_ = 0;
	uint32_t sw_ = 0;
	uint32_t sh_ = 0;
	std::vector<TrackedWin> tracked_;
	Menu start_menu_;
	Menu submenu_;
	Launcher launcher_;
	bool btn_pressed_ = false;
	bool prev_down_ = false;  /* previous button-1 state, for release-edge clicks */
	uint32_t focused_id_ = 0; /* active window (DISPLAY_FOCUS); highlights its tab */

	/* Hover state on the strip (updated from pointer-motion events). */
	bool hover_start_ = false; /* cursor over the start button */
	int hover_tab_ = -1;       /* window tab under the cursor, or -1 */
	bool hover_vol_ = false;   /* cursor over the volume tray glyph */

	/* System tray: master volume mirrored from ubistry (/aural/*). */
	int volume_ = 100;
	bool muted_ = false;

	/* Draw the hamburger "menu" glyph for the start button. */
	void draw_start_icon(int cx, int cy)
	{
		for (int i = -1; i <= 1; i++)
			surf_.ogFillRect(cx - 7, cy + i * 5 - 1, cx + 7, cy + i * 5, COL_WHITE);
	}

	/* Draw a compact speaker glyph for the volume tray.  Red + slashed when
	 * muted; a couple of "wave" ticks to the right scale with the level. */
	void draw_volume_glyph(int cx, int cy)
	{
		uint32_t col = muted_ ? 0x00FF6E6Eu : (hover_vol_ ? COL_WHITE : 0x00C8C8D2u);
		surf_.ogFillRect(cx - 7, cy - 2, cx - 5, cy + 2, col); /* driver block */
		for (int c = 0; c <= 5; c++)                           /* cone widens right */
			surf_.ogVLine(cx - 4 + c, cy - c, cy + c, col);
		if (muted_)
		{
			surf_.ogLine(cx - 7, cy + 6, cx + 7, cy - 6, 0x00FF6E6Eu); /* mute slash */
		}
		else
		{
			if (volume_ > 5)
				surf_.ogVLine(cx + 4, cy - 3, cy + 3, col);
			if (volume_ >= 55)
				surf_.ogVLine(cx + 7, cy - 5, cy + 5, col);
		}
	}

	void draw_strip()
	{
		int sw = (int)sw_;

		surf_.ogFillRect(0, 0, sw - 1, TB_H - 1, TB_BG);
		surf_.ogFillRect(0, 0, sw - 1, 0, TB_SEP); /* 1px top hairline */

		/* Launcher button — flat; hamburger icon + brand.  Fills on press, and a
		 * subtle highlight on hover. */
		uint32_t start_bg = btn_pressed_ ? TB_BTN_P : (hover_start_ ? TB_BTN_N : TB_BG);
		if (btn_pressed_ || hover_start_)
			surf_.ogFillRect(2, 1, 2 + BTN_W - 1, TB_H - 1, start_bg);
		draw_start_icon(16, TB_H / 2);
		font_fg(font_, COL_WHITE);
		font_bg(font_, start_bg);
		font_.PutString(surf_, 30, 12, "uBixOS");

		/* Window list — flat tabs; focused window gets a brighter fill + accent
		 * underline, hovered tab a lighter fill, the rest a flat fill. */
		int wx = 2 + BTN_W + 4;
		int clock_x = sw - CLOCK_W - 2;
		int tray_x = clock_x - TRAY_W;
		int i = 0;
		for (const auto &tw : tracked_)
		{
			if (wx + WIN_BTN_W > tray_x - 4)
				break;
			bool active = (tw.id == focused_id_);
			bool hover = (i == hover_tab_);
			uint32_t fill = active ? TB_BTN_P : TB_BTN_N;
			surf_.ogFillRect(wx, 4, wx + WIN_BTN_W - 1, TB_H - 1, fill);
			if (active || hover)
				surf_.ogFillRect(
				    wx, TB_H - 3, wx + WIN_BTN_W - 1, TB_H - 1, active ? COL_WHITE : TB_BTN_P);
			font_fg(font_, (active || hover) ? COL_WHITE : 0x00C8C8D2u);
			font_bg(font_, fill);
			font_.PutString(surf_, wx + 8, 12, tw.title.c_str());
			wx += WIN_BTN_W + 2;
			i++;
		}

		/* System tray: volume glyph. */
		draw_volume_glyph(tray_x + TRAY_W / 2, TB_H / 2);

		/* Clock — boxless date + time, right-aligned text on the bar. */
		char tstr[20];
		get_datetime_str(tstr);
		font_fg(font_, COL_WHITE);
		font_bg(font_, TB_BG);
		font_.PutString(surf_, clock_x + 8, 12, tstr);
	}

	/* Tray hit-test: true if mx is over the volume glyph. */
	bool vol_hit(int mx) const
	{
		int tray_x = (int)sw_ - CLOCK_W - 2 - TRAY_W;
		return mx >= tray_x && mx < tray_x + TRAY_W;
	}

	int winbtn_hit(int mx) const
	{
		int wx = 2 + BTN_W + 4;
		int tray_x = (int)sw_ - CLOCK_W - 2 - TRAY_W;
		for (int i = 0; i < (int)tracked_.size(); i++)
		{
			if (wx + WIN_BTN_W > tray_x - 4)
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
		creq->wants_motion = 1; /* receive hover motion for highlighting */
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

		if (!font_.Load(font_path, FONT_SIZE))
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

	/* Re-read master volume/mute from ubistry into the cached members. */
	void refresh_volume()
	{
		int v = 0, m = 0;
		if (ubistry_get_int("/aural/volume", &v) == 0)
			volume_ = v < 0 ? 0 : (v > 100 ? 100 : v);
		muted_ = (ubistry_get_int("/aural/mute", &m) == 0) && (m != 0);
	}

	/*
	 * Re-claim the bottom strip at a new screen size after a live resolution
	 * change (DISPLAY_RESIZE): release the old window, claim a full-width strip
	 * at the new geometry, and re-attach the surface.  Returns true on success.
	 */
	bool reclaim(ubix::Mailbox &mbox, uint32_t new_w, uint32_t new_h)
	{
		mpi_message_t rel = {};
		struct display_release *dr = (struct display_release *)rel.data;
		rel.header = DISPLAY_RELEASE;
		dr->window_id = win_id_;
		ubix::post_message("views", DISPLAY_RELEASE, rel);

		sw_ = new_w;
		sh_ = new_h;

		mpi_message_t claim = {};
		struct display_claim_req *creq = (struct display_claim_req *)claim.data;
		claim.header = DISPLAY_CLAIM;
		creq->x = 0;
		creq->y = (int32_t)(sh_ - TB_H);
		creq->w = (int32_t)sw_;
		creq->h = TB_H;
		creq->sender_pid = ubix::pid();
		creq->no_decor = 1;
		creq->wants_motion = 1; /* receive hover motion for highlighting */
		std::strncpy(creq->title, "taskbar", sizeof(creq->title) - 1);
		creq->title[sizeof(creq->title) - 1] = '\0';
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		creq->reply[sizeof(creq->reply) - 1] = '\0';
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		/* Wait for the ACK, ignoring any unrelated messages that interleave. */
		mpi_message_t reply;
		int tries = 200000;
		for (;;)
		{
			if (mbox.try_fetch(reply))
			{
				if (reply.header == DISPLAY_ACK)
					break;
			}
			else
			{
				ubix::yield();
				if (--tries == 0)
					return false;
			}
		}

		struct display_ack *da = (struct display_ack *)reply.data;
		win_id_ = da->window_id;
		if (da->shm_base == nullptr)
			return false;
		surf_.ogAttach(da->shm_base, sw_, TB_H, OG_PIXFMT_32BPP);
		return true;
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

	/* Record the focused window (DISPLAY_FOCUS) so its tab is highlighted. */
	void set_focused_id(uint32_t id)
	{
		focused_id_ = id;
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
			else if (it.exec == "@about")
				launcher_.launch("/bin/settings about"); /* open Settings on About */
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

	/* Update hover highlight from a pointer event (motion or click).  Negative
	 * coords are the "cursor left" signal; they clear the relevant hover. */
	void update_hover(const display_mouse_ev *me)
	{
		bool exited = (me->x < 0);
		if (me->window_id == win_id_)
		{
			bool hs = !exited && (me->x >= 2 && me->x < 2 + BTN_W);
			int ht = exited ? -1 : winbtn_hit(me->x);
			bool hv = !exited && vol_hit(me->x);
			if (hs != hover_start_ || ht != hover_tab_ || hv != hover_vol_)
			{
				hover_start_ = hs;
				hover_tab_ = ht;
				hover_vol_ = hv;
				draw_strip();
				send_flip();
			}
		}
		else if (me->window_id == start_menu_.win_id() && start_menu_.is_open())
		{
			if (start_menu_.set_hover(exited ? -1 : start_menu_.hit_item(me->y)))
				start_menu_.redraw(font_);
		}
		else if (me->window_id == submenu_.win_id() && submenu_.is_open())
		{
			if (submenu_.set_hover(exited ? -1 : submenu_.hit_item(me->y)))
				submenu_.redraw(font_);
		}
	}

	void on_mouse(const display_mouse_ev *me, ubix::Mailbox &mbox)
	{
		bool down = (me->buttons & 1) != 0;
		bool click = !down && prev_down_; /* release edge = a completed click */
		prev_down_ = down;

		update_hover(me);

		/* Submenu click: dispatch the chosen leaf. */
		if (me->window_id == submenu_.win_id() && submenu_.is_open())
		{
			if (click)
			{
				const MenuItem *it = submenu_.item(submenu_.hit_item(me->y));
				MenuItem sel = it ? *it : MenuItem();
				close_menus();
				if (it)
					dispatch(sel);
			}
			return;
		}

		/* Top-level menu click: power button, cascade a submenu, or dispatch. */
		if (me->window_id == start_menu_.win_id() && start_menu_.is_open())
		{
			if (click)
			{
				if (start_menu_.footer_power_hit(me->x, me->y))
				{
					close_menus();
					::exit(0); /* log out — vlogin resumes the session */
				}
				int row = start_menu_.hit_item(me->y);
				const MenuItem *it = start_menu_.item(row);
				if (it && it->submenu)
				{
					start_menu_.set_active_row(row); /* keep parent lit */
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

		/* Strip: reflect the pressed state on the start button. */
		if (down != btn_pressed_)
		{
			btn_pressed_ = down;
			draw_strip();
			send_flip();
		}
		if (!click)
			return;

		int wi = winbtn_hit(me->x);
		if (wi >= 0)
		{
			close_menus();
			raise_window(tracked_[wi].id);
			return;
		}
		if (vol_hit(me->x))
		{
			launcher_.launch("/bin/settings"); /* open Settings for volume */
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
				start_menu_.set_footer(true); /* user + power row */
				start_menu_.show(2, (int)sh_ - TB_H - start_menu_.height(), mbox, font_);
			}
		}
		else
		{
			close_menus(); /* click elsewhere dismisses the menu */
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

	tb.refresh_volume();
	tb.draw();
	tb.send_flip();

	int last_sec = -1;
	for (;;)
	{
		int t = gettime() % 60;
		if (t != last_sec)
		{
			last_sec = t;
			tb.refresh_volume(); /* pick up volume/mute changes once a second */
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

			if (reply.header == DISPLAY_RESIZE)
			{
				struct display_resize *dr = (struct display_resize *)reply.data;
				if (tb.reclaim(mbox, dr->screen_w, dr->screen_h))
				{
					tb.draw();
					tb.send_flip();
				}
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

			if (reply.header == DISPLAY_FOCUS)
			{
				struct display_focus *df = (struct display_focus *)reply.data;
				tb.set_focused_id(df->window_id);
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
