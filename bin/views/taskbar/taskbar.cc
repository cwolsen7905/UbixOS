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
#include <deque>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogPixelFmt.h>
#include <objgfx/ogImage.h>
#include <views/theme.h>
#include <views/uidraw.h>
#include <views/appicons.h>
#include <api/power.h>
#include <api/netcfg.h>
#include <ubistry/ubistry.h>
#include <audio/aural_proto.h>

extern char **environ; /* inherited session env, forwarded to launched apps */

#define FONT_PATH "/var/fonts/DejaVuSans.ttf"
#define FONT_BOLD_PATH "/var/fonts/DejaVuSans-Bold.ttf"
#define FONT_SIZE 14       /* line height for 12px UI text (views/theme.h UI_FS_UI) */
#define FONT_SMALL_SIZE 13 /* 11px secondary text: volume % */
#define FONT_CLOCK_SIZE 15 /* 12.5px tray clock (date + time, same size) */

/* Taskbar geometry (ubixos-v2): a floating rounded bar UI_TASKBAR_INSET px off
 * the bottom/side edges.  The claimed strip is TB_H rows: the bar plus its
 * inset below and an equal band above for the bar's soft shadow.  Menus and
 * flyouts open flush above the strip (= 8px above the bar). */
#define TB_H UI_TASKBAR_RESERVE
#define BAR_X UI_TASKBAR_INSET
#define BAR_Y (TB_H - UI_TASKBAR_INSET - UI_TASKBAR_H)
#define BAR_H UI_TASKBAR_H
#define ITEM_H 32 /* launcher pill, task items, tray items */
#define ITEM_Y (BAR_Y + (BAR_H - ITEM_H) / 2)
#define TASK_W 180 /* running-window item */

/* Volume pop-over geometry (design: 280px card above the tray). */
#define MX_W 280     /* panel width */
#define MX_PAD 16    /* panel padding */
#define MX_ROW 32    /* per-app row height */
#define MX_NAME_W 96 /* per-app name column */
#define MX_MX 12     /* window margin left/right of the panel (shadow room) */
#define MX_MT 10     /* ... above */
#define MX_MB 16     /* ... below */

/* System palette (views/theme.h) for the session user: light/dark mode and
 * accent.  Everything the taskbar draws (bar, launcher, volume pop-over,
 * power page) takes its colours from here. */
static struct ui_theme g_t = ui_theme_dark;
static bool g_clock24 = false; /* /views/clock/24h for the session user */

/**
 * Re-resolve the palette for the session user ($USER).  Called at startup and
 * whenever a DISPLAY_THEME message arrives (mode or accent changed).
 */
static void apply_theme(void)
{
	g_t = ui_theme_load(getenv("USER"));
	g_clock24 = ui_clock_24h(getenv("USER"));
}

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static void font_fg(ogScalableFont &f, uint32_t c)
{
	f.SetFGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

/* ------------------------------------------------------------------ */
/* Time formatting                                                      */
/* ------------------------------------------------------------------ */

/*
 * Timezone: the kernel clock is UTC.  The local zone is a POSIX TZ string in
 * ubistry (/system/timezone, e.g. "EST5EDT,M3.2.0,M11.1.0" = US Eastern with
 * auto-DST).  We hand it to musl via $TZ and let localtime() do the offset +
 * DST math — the standard, future-proof path (and $TZ then serves any POSIX
 * app's localtime() too).
 */
static bool g_tz_loaded = false;

/* musl gates these POSIX prototypes behind a feature macro the -nostdinc world
 * build does not set; the symbols are in libc, so declare them directly. */
extern "C" int setenv(const char *name, const char *value, int overwrite);
extern "C" void tzset(void);
extern "C" int nanosleep(const struct timespec *req, struct timespec *rem);

/* Load /system/timezone once (default US Eastern w/ DST) and apply it via $TZ. */
static void load_timezone(void)
{
	char tz[64];
	if (ubistry_get_str("/system/timezone", tz, sizeof(tz)) != 0)
	{
		strncpy(tz, "EST5EDT,M3.2.0,M11.1.0", sizeof(tz) - 1);
		tz[sizeof(tz) - 1] = '\0';
	}
	setenv("TZ", tz, 1);
	tzset();
	g_tz_loaded = true;
}

/* Format the tray clock: date "Fri, Oct 9" and time "8:57 PM" / "20:57". */
static void get_clock_strs(char *date, size_t dn, char *tm_s, size_t tn)
{
	static const char *const wd[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static const char *const mo[] = {
	    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	if (!g_tz_loaded)
		load_timezone();
	time_t t = (time_t)gettime();
	struct tm *lt = localtime(&t);
	if (lt == nullptr)
	{
		date[0] = tm_s[0] = '\0';
		return;
	}
	snprintf(date, dn, "%s, %s %d", wd[lt->tm_wday % 7], mo[lt->tm_mon % 12], lt->tm_mday);
	ui_format_time(tm_s, tn, lt, g_clock24);
}

/* ------------------------------------------------------------------ */
/* Reply matching                                                       */
/* ------------------------------------------------------------------ */

/* Messages that arrived while waiting for a specific reply (a window grant,
 * screen info); main() handles them, in order, before reading the mailbox. */
static std::deque<mpi_message_t> g_deferred;

/**
 * Wait for a reply whose header is want_a or want_b, setting aside anything
 * else (input, window notifications, theme changes) for the main loop instead
 * of mistaking it for the reply or dropping it.
 */
static void await_reply(ubix::Mailbox &mbox, mpi_message_t &reply, uint32_t want_a, uint32_t want_b)
{
	for (;;)
	{
		if (!mbox.try_fetch(reply))
		{
			ubix::yield();
			continue;
		}
		if (reply.header == want_a || reply.header == want_b)
			return;
		g_deferred.push_back(reply);
	}
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
/* MenuItem — one launchable entry (label + exec action).              */
/* ------------------------------------------------------------------ */

struct MenuItem
{
	std::string label;
	std::string exec; /* a program path (with args), or "@builtin" */
};

/* ------------------------------------------------------------------ */
/* MixerFlyout — the speaker-tray pop-over.  A vertical master-volume   */
/*   slider plus one slider per live aural stream (the per-app mixer).  */
/*   Drives the codec via AURAL_SET_MASTER and each stream via          */
/*   AURAL_SET_GAIN; the live stream set comes from an AURAL_LIST query. */
/* ------------------------------------------------------------------ */

/* Write @src into @dst (cap @n) trimmed to fit @maxpx pixels, appending "..."
 * when shortened.  Uses the font's real glyph advances, so it works for any name
 * and proportional face — no guessed character budget.  Shared by the mixer
 * flyout (stream names) and the taskbar (window-button titles). */
static void fit_label(ogScalableFont &font, char *dst, size_t n, const char *src, int maxpx)
{
	std::strncpy(dst, src, n - 1);
	dst[n - 1] = '\0';
	if ((int)font.TextWidth(dst) <= maxpx)
		return; /* fits whole */

	/* Largest prefix p such that "p..." still fits the budget. */
	for (size_t len = std::strlen(dst); len > 0; len--)
	{
		char trial[40];
		std::snprintf(trial, sizeof(trial), "%.*s...", (int)(len - 1), src);
		if ((int)font.TextWidth(trial) <= maxpx)
		{
			std::strncpy(dst, trial, n - 1);
			dst[n - 1] = '\0';
			return;
		}
	}
	std::snprintf(dst, n, "..."); /* nothing fits — just the ellipsis */
}

class MixerFlyout
{
	ogSurface surf_;
	ubix::Mailbox replies_; /* dedicated reply box for AURAL_LIST */
	bool replies_ready_ = false;
	uint32_t win_id_ = 0;
	bool open_ = false;
	int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
	int active_col_ = -1; /* row under a held button (0 = master, 1.. = app) */
	int hover_col_ = -1;  /* row under the cursor (knob highlight)            */
	ui_canvas cv_;        /* window pixels (translucent: shape + shadow in alpha) */

	int master_vol_ = 100;
	bool master_mute_ = false;
	struct aural_list_reply data_ = {}; /* live streams (from AURAL_LIST) */

	/* Master row plus one per live stream. */
	int columns() const
	{
		return 1 + (int)data_.count;
	}

	/* Display level 0..100 for row @p c (0 = master). */
	int col_level(int c) const
	{
		if (c == 0)
			return master_mute_ ? 0 : master_vol_;
		uint32_t g = data_.streams[c - 1].gain;
		int lvl = (int)(g * 100u / AURAL_GAIN_UNITY);
		return lvl > 100 ? 100 : lvl;
	}

	const char *col_name(int c) const
	{
		return (c == 0) ? "Volume" : data_.streams[c - 1].name;
	}

	/* Row geometry (window-local): the panel sits at (MX_MX, MX_MT). */
	int row_y(int c) const
	{
		return MX_MT + MX_PAD + (c == 0 ? 0 : 24 + 12 + 16 + (c - 1) * MX_ROW);
	}
	int row_h(int c) const
	{
		return c == 0 ? 24 : MX_ROW;
	}
	int track_x0(int c) const
	{
		return MX_MX + MX_PAD + (c == 0 ? 24 + 12 : MX_NAME_W);
	}
	int track_x1() const
	{
		return MX_MX + MX_W - MX_PAD - 40 - 14; /* room for "100%" + the knob */
	}

	/* Row under (x, y), or -1. */
	int col_at(int x, int y) const
	{
		if (x < MX_MX || x >= MX_MX + MX_W)
			return -1;
		for (int c = 0; c < columns(); c++)
			if (y >= row_y(c) - 4 && y < row_y(c) + row_h(c) + 4)
				return c;
		return -1;
	}

	/* Map a window-local x onto row c's 0..100 level (clamped to its track). */
	int level_at(int c, int x) const
	{
		int x0 = track_x0(c), x1 = track_x1();
		if (x <= x0)
			return 0;
		if (x >= x1)
			return 100;
		return (x - x0) * 100 / (x1 - x0);
	}

	/* Text centred on row cy, blended over the surface. */
	void text(ogScalableFont &f, int x, int cy, const char *str, uint32_t fg)
	{
		ui_text(surf_, f, x, cy, str, fg);
	}

	/* One horizontal slider row (design: 2px track, accent fill, 14px knob). */
	void draw_slider(ogScalableFont &font, int c)
	{
		const ui_theme &t = g_t;
		int y = row_y(c), cy = y + row_h(c) / 2;
		int x0 = track_x0(c), x1 = track_x1();
		int lvl = col_level(c);
		bool lit = (c == active_col_ || c == hover_col_);

		if (c == 0)
			ui_draw_icon_tint(cv_,
			                  ui_icon_get(24, master_mute_ ? "volume-mute" : "volume"),
			                  MX_MX + MX_PAD,
			                  y,
			                  t.text_muted);
		else
		{
			char nm[24];
			fit_label(font, nm, sizeof(nm), col_name(c), MX_NAME_W - 12);
			text(font, MX_MX + MX_PAD, cy, nm, t.text);
		}

		int kx = x0 + lvl * (x1 - x0) / 100;
		ui_fill_round(cv_, x0, cy - 1, x1 - x0, 2, 1, t.neutral_800);
		if (kx > x0)
			ui_fill_round(cv_, x0, cy - 1, kx - x0, 2, 1, t.accent);
		int kr = lit ? 8 : 7;
		ui_fill_round(cv_, kx - kr, cy - kr, 2 * kr, 2 * kr, kr, t.accent);
		ui_fill_round(cv_, kx - kr + 2, cy - kr + 2, 2 * kr - 4, 2 * kr - 4, kr - 2, 0xE4E7F5u);

		char pct[8];
		std::snprintf(pct, sizeof(pct), "%d%%", lvl);
		text(font, MX_MX + MX_W - MX_PAD - (int)font.TextWidth(pct), cy, pct, t.text_muted);
	}

	void draw(ogScalableFont &font)
	{
		const ui_theme &t = g_t;
		ui_fill(cv_, 0, 0, w_, h_, t.panel);
		if (columns() > 1)
		{
			int ly = row_y(0) + 24 + 12;
			ui_fill(
			    cv_, MX_MX + MX_PAD, ly - 6, MX_W - 2 * MX_PAD, 1, ui_mix(t.panel, t.divider, t.divider_a));
			ui_text(surf_, font, MX_MX + MX_PAD, ly + 8, "Apps", t.text_faint);
		}
		for (int c = 0; c < columns(); c++)
			draw_slider(font, c);
		ui_alpha_shape(cv_,
		               MX_MX,
		               MX_MT,
		               MX_W,
		               h_ - MX_MT - MX_MB,
		               UI_R_XL,
		               t.panel_a,
		               t.edge_strong,
		               t.shadow_panel_y,
		               t.shadow_panel_reach,
		               t.shadow_panel_a,
		               t.shadow);
	}

	/* Push a column's new level to aural (master -> codec, app -> stream gain). */
	void apply_level(int c, int level)
	{
		mpi_message_t msg = {};
		if (c == 0)
		{
			master_vol_ = level;
			master_mute_ = false;
			struct aural_master *m = (struct aural_master *)msg.data;
			m->volume = (uint32_t)level;
			m->mute = AURAL_MASTER_UNCHANGED;
			msg.header = AURAL_SET_MASTER;
			mpi_postMessage("aural", AURAL_SET_MASTER, &msg);
		}
		else
		{
			uint32_t gain = (uint32_t)level * AURAL_GAIN_UNITY / 100u;
			data_.streams[c - 1].gain = gain;
			struct aural_gain *g = (struct aural_gain *)msg.data;
			g->stream_id = data_.streams[c - 1].stream_id;
			g->gain = gain;
			msg.header = AURAL_SET_GAIN;
			mpi_postMessage("aural", AURAL_SET_GAIN, &msg);
		}
	}

	/* Ask aural for the current live-stream set (blocks briefly on the reply). */
	void query_aural()
	{
		if (!replies_ready_)
		{
			replies_.assign("taskbar.mix");
			replies_ready_ = replies_.create();
		}
		std::memset(&data_, 0, sizeof(data_));
		if (!replies_ready_)
			return;

		mpi_message_t drain;
		while (replies_.try_fetch(drain)) /* discard any stale reply */
			;

		mpi_message_t msg = {};
		struct aural_list_req *req = (struct aural_list_req *)msg.data;
		req->ver = AURAL_PROTO_VERSION;
		std::strncpy(req->reply, "taskbar.mix", sizeof(req->reply) - 1);
		msg.header = AURAL_LIST;
		if (mpi_postMessage("aural", AURAL_LIST, &msg) != 0)
			return; /* aural not running — master-only flyout */

		/*
		 * Wait for the reply paced in REAL time, not raw yields.  aural answers on
		 * its next ~10 ms wake; a bare yield loop burns thousands of iterations in
		 * microseconds (when nothing else is runnable on the cooperative scheduler)
		 * and gives up before aural ever runs — which left the flyout master-only.
		 * Sleeping ~2 ms between polls guarantees aural is scheduled.  The post
		 * above succeeded, so the "aural" mailbox exists and a reply is coming;
		 * ~300 ms is a generous ceiling only hit if the server is wedged.
		 */
		struct timespec nap = {0, 2 * 1000 * 1000}; /* 2 ms */
		for (int tries = 0; tries < 150; tries++)
		{
			mpi_message_t rep;
			if (replies_.try_fetch(rep))
			{
				if (rep.header == AURAL_LIST_REPLY)
				{
					struct aural_list_reply *lr = (struct aural_list_reply *)rep.data;
					data_ = *lr;
					if (data_.count > AURAL_LIST_MAX)
						data_.count = AURAL_LIST_MAX;
					return;
				}
				continue; /* unrelated message — keep waiting without sleeping */
			}
			nanosleep(&nap, 0);
		}
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

	/* Current master level / mute as shown in the pop-over. */
	int master() const
	{
		return master_vol_;
	}
	bool master_muted() const
	{
		return master_mute_;
	}

	/* Seed the master mirror from the taskbar's ubistry cache before showing. */
	void set_master(int vol, bool mute)
	{
		master_vol_ = vol;
		master_mute_ = mute;
	}

	/* Open the flyout, right-edge anchored to @p anchor_right (the tray glyph),
	 * sitting just above the taskbar.  Queries aural for the app list first. */
	void show(int anchor_right, int screen_h, ubix::Mailbox &mbox, ogScalableFont &font)
	{
		if (open_)
			return;
		query_aural();

		/* Panel: master row, then (if any app streams) an "Apps" list. */
		int ph = MX_PAD + 24 + (columns() > 1 ? 12 + 16 + (columns() - 1) * MX_ROW : 0) + MX_PAD;
		w_ = MX_MX + MX_W + MX_MX;
		h_ = MX_MT + ph + MX_MB;
		x_ = anchor_right - MX_W - MX_MX;
		if (x_ < 0)
			x_ = 0;
		y_ = screen_h - TB_H - ph - MX_MT;
		if (y_ < 0)
			y_ = 0;

		mpi_message_t claim = {};
		struct display_claim_req *creq = (struct display_claim_req *)claim.data;
		claim.header = DISPLAY_CLAIM;
		creq->x = x_;
		creq->y = y_;
		creq->w = w_;
		creq->h = h_;
		creq->sender_pid = ubix::pid();
		creq->no_decor = 1;
		creq->wants_motion = 1; /* receive drag motion while a button is held */
		creq->translucent = 1;
		std::strncpy(creq->title, "mixer", sizeof(creq->title) - 1);
		creq->title[sizeof(creq->title) - 1] = '\0';
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		creq->reply[sizeof(creq->reply) - 1] = '\0';
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		mpi_message_t reply;
		await_reply(mbox, reply, DISPLAY_ACK, DISPLAY_DENIED);
		if (reply.header != DISPLAY_ACK)
			return;

		struct display_ack *da = (struct display_ack *)reply.data;
		win_id_ = da->window_id;
		surf_.ogAttach(da->shm_base, (uint32_t)w_, (uint32_t)h_, OG_PIXFMT_32BPP);
		cv_.px = (uint32_t *)surf_.ogGetPtr(0, 0);
		cv_.w = w_;
		cv_.h = h_;
		cv_.stride = (int)((uint8_t *)surf_.ogGetPtr(0, 1) - (uint8_t *)surf_.ogGetPtr(0, 0)) / 4;
		open_ = true;
		active_col_ = -1;
		hover_col_ = -1;
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
		active_col_ = -1;
		hover_col_ = -1;
	}

	/*
	 * Handle a pointer event addressed to the flyout window.  A press locks onto
	 * the column under the cursor; subsequent motion with the button held drags
	 * that slider (so a slightly off-axis drag stays in its own column).
	 */
	void on_event(const display_mouse_ev *me, ogScalableFont &font)
	{
		bool down = (me->buttons & 1) != 0;

		/* Button up: just track which knob the cursor hovers (negative x = left). */
		if (!down)
		{
			active_col_ = -1;
			int hc = (me->x < 0) ? -1 : col_at(me->x, me->y);
			if (hc != hover_col_)
			{
				hover_col_ = hc;
				draw(font);
				send_flip_msg(win_id_);
			}
			return;
		}

		if (active_col_ < 0)
			active_col_ = col_at(me->x, me->y); /* lock the row on press */
		if (active_col_ < 0)
			return;

		hover_col_ = active_col_;
		apply_level(active_col_, level_at(active_col_, me->x));
		draw(font);
		send_flip_msg(win_id_);
	}
};

/* ------------------------------------------------------------------ */
/* CalendarPanel — the clock's pop-over: big time, long date and the    */
/*   current month (Monday-first), today ringed in the accent.          */
/* ------------------------------------------------------------------ */

#define CAL_W 340 /* panel width (design: 340, 32px padding) */
#define CAL_PAD 28
#define CAL_CELL 36

/**
 * Monday-first column (0..6) of the 1st of lt's month.
 */
static int cal_first_col(const struct tm &lt)
{
	return (lt.tm_wday - (lt.tm_mday - 1) % 7 + 7 + 6) % 7;
}

/**
 * Days in month mon (0..11) of year y (full year).
 */
static int cal_days(int mon, int y)
{
	static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
	return mdays[mon % 12] + (mon == 1 && leap);
}

class CalendarPanel
{
	ogSurface surf_;
	ui_canvas cv_;
	ogScalableFont big_; /* 32px time */
	ogScalableFont *font_ = nullptr;
	ogScalableFont *small_ = nullptr;
	uint32_t win_id_ = 0;
	bool open_ = false;
	int w_ = 0, h_ = 0, ph_ = 0;

	void draw()
	{
		static const char *const wd[] = {
		    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
		static const char *const mo[] = {"January",
		                                 "February",
		                                 "March",
		                                 "April",
		                                 "May",
		                                 "June",
		                                 "July",
		                                 "August",
		                                 "September",
		                                 "October",
		                                 "November",
		                                 "December"};
		static const char *const dow[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
		const ui_theme &t = g_t;
		ui_fill(cv_, 0, 0, w_, h_, t.panel);

		time_t now = (time_t)gettime();
		struct tm lt = *localtime(&now);
		int x = MX_MX + CAL_PAD, y = MX_MT + CAL_PAD;
		char buf[48];
		ui_format_time(buf, sizeof(buf), &lt, g_clock24);
		ui_text(surf_, big_, x, y + 18, buf, t.text);
		std::snprintf(buf,
		              sizeof(buf),
		              "%s, %s %d, %d",
		              wd[lt.tm_wday % 7],
		              mo[lt.tm_mon % 12],
		              lt.tm_mday,
		              lt.tm_year + 1900);
		ui_text(surf_, *font_, x, y + 46, buf, t.neutral_400);
		std::snprintf(buf, sizeof(buf), "%s %d", mo[lt.tm_mon % 12], lt.tm_year + 1900);
		ui_text(surf_, *font_, x, y + 84, buf, t.text);

		/* Weekday labels, then the month grid (other months' days dimmed). */
		int gy = y + 108, cw = (CAL_W - 2 * CAL_PAD) / 7;
		for (int i = 0; i < 7; i++)
			ui_text_c(surf_, *small_, x + i * cw + cw / 2, gy + 8, dow[i], t.text_faint);
		gy += 24;
		int off = cal_first_col(lt);
		int dim = cal_days(lt.tm_mon, lt.tm_year + 1900);
		int pdim = cal_days((lt.tm_mon + 11) % 12, lt.tm_year + 1900);
		int cells = ((off + dim + 6) / 7) * 7;
		for (int i = 0; i < cells; i++)
		{
			int d = i - off + 1;
			bool in = d >= 1 && d <= dim;
			int n = in ? d : (d < 1 ? pdim + d : d - dim);
			int cx = x + (i % 7) * cw + cw / 2, cy = gy + (i / 7) * CAL_CELL + CAL_CELL / 2;
			bool today = in && d == lt.tm_mday;
			if (today)
				ui_outlined_round(cv_, cx - 16, cy - 16, 32, 32, 16, t.accent, t.accent_800);
			std::snprintf(buf, sizeof(buf), "%d", n);
			ui_text_c(surf_, *font_, cx, cy, buf, today ? t.accent_200 : (in ? t.text : t.neutral_700));
		}

		ui_alpha_shape(cv_,
		               MX_MX,
		               MX_MT,
		               CAL_W,
		               ph_,
		               UI_R_XL,
		               t.panel_a,
		               t.edge_strong,
		               t.shadow_panel_y,
		               t.shadow_panel_reach,
		               t.shadow_panel_a,
		               t.shadow);
	}

      public:
	void init(ogScalableFont *font, ogScalableFont *small)
	{
		font_ = font;
		small_ = small;
		big_.Load(FONT_PATH, 37); /* 32px */
	}
	bool is_open() const
	{
		return open_;
	}
	uint32_t win_id() const
	{
		return win_id_;
	}

	/* Open right-aligned to anchor_right, just above the taskbar strip. */
	void show(int anchor_right, int screen_h, ubix::Mailbox &mbox)
	{
		if (open_)
			return;
		time_t now = (time_t)gettime();
		struct tm lt = *localtime(&now);
		int rows = (cal_first_col(lt) + cal_days(lt.tm_mon, lt.tm_year + 1900) + 6) / 7;
		ph_ = CAL_PAD + 108 + 24 + rows * CAL_CELL + CAL_PAD - 8;
		w_ = MX_MX + CAL_W + MX_MX;
		h_ = MX_MT + ph_ + MX_MB;
		int x = anchor_right - CAL_W - MX_MX, y = screen_h - TB_H - ph_ - MX_MT;

		mpi_message_t claim = {};
		struct display_claim_req *creq = (struct display_claim_req *)claim.data;
		claim.header = DISPLAY_CLAIM;
		creq->x = x < 0 ? 0 : x;
		creq->y = y < 0 ? 0 : y;
		creq->w = w_;
		creq->h = h_;
		creq->sender_pid = ubix::pid();
		creq->no_decor = 1;
		creq->translucent = 1;
		std::strncpy(creq->title, "calendar", sizeof(creq->title) - 1);
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		mpi_message_t reply;
		await_reply(mbox, reply, DISPLAY_ACK, DISPLAY_DENIED);
		if (reply.header != DISPLAY_ACK)
			return;
		struct display_ack *da = (struct display_ack *)reply.data;
		win_id_ = da->window_id;
		surf_.ogAttach(da->shm_base, (uint32_t)w_, (uint32_t)h_, OG_PIXFMT_32BPP);
		cv_.px = (uint32_t *)surf_.ogGetPtr(0, 0);
		cv_.w = w_;
		cv_.h = h_;
		cv_.stride = (int)((uint8_t *)surf_.ogGetPtr(0, 1) - (uint8_t *)surf_.ogGetPtr(0, 0)) / 4;
		open_ = true;
		draw();
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

	/* Repaint (the minute changed). */
	void refresh()
	{
		if (!open_)
			return;
		draw();
		send_flip_msg(win_id_);
	}
};

/* ------------------------------------------------------------------ */
/* Taskbar — owns the strip surface, font, window list, and event      */
/*           routing; delegates to Menu and Launcher                   */
/* ------------------------------------------------------------------ */

/**
 * Draw an app's icon (views/appicons.h) in the session palette.
 */
static void draw_app_icon(ui_canvas &cv, const std::string &label, const std::string &exec, int size, int x, int y)
{
	ui_draw_app_icon(cv, label, exec, size, x, y, g_t);
}

/* ------------------------------------------------------------------ */
/* LauncherPanel — the uBix launcher (ubixos-v2 launcher-menu): a       */
/*   category rail + search + app grid + "Continue" (recent apps).     */
/*   Apps come from the same /views/startmenu registry tree the old    */
/*   cascading menu used; recents are per user (views/launcher/recent). */
/* ------------------------------------------------------------------ */

#define LP_W 640 /* panel size (design 640x472) */
#define LP_H 472
#define LP_MARGIN_X 8  /* window margin left of the panel (shadow room) */
#define LP_MARGIN_T 10 /* ... above */
#define LP_MARGIN_B 16 /* ... below (shadow is offset down) */
#define LP_RAIL_W 176
#define LP_TILE_W 104
#define LP_TILE_H 80
#define LP_RECENT_MAX 3

struct LaunchApp
{
	std::string label, exec;
	int cat; /* index into the category list (0 = All, never stored) */
};

/* What a launcher click/keypress asks the taskbar to do. */
struct LaunchAction
{
	enum
	{
		NONE,
		REDRAW,
		RESTART,
		SHUTDOWN,
		CLOSE,
		LAUNCH,
		LOGOUT
	} kind = NONE;
	std::string label, exec;
};

class LauncherPanel
{
	ogSurface surf_;
	ui_canvas cv_;
	ogScalableFont *font_ = nullptr;  /* 12px */
	ogScalableFont *small_ = nullptr; /* 11px */
	ogScalableFont *bold_ = nullptr;  /* 12px bold */
	ogScalableFont tiny_;             /* 10px: counts, section labels */
	uint32_t win_id_ = 0;
	bool open_ = false;
	int wx_ = 0, wy_ = 0, ww_ = 0, wh_ = 0; /* window rect (screen) */
	int pw_ = LP_W, ph_ = LP_H;             /* panel size; panel sits at (LP_MARGIN_X, LP_MARGIN_T) */

	std::vector<std::string> cats_; /* [0] = "All" */
	std::vector<LaunchApp> apps_;
	int cat_ = 0;
	std::string query_;

	struct Recent
	{
		std::string label, exec;
		long when;
	};
	std::vector<Recent> recent_;

	/* hit rects from the last draw (window-local): x, y, w, h */
	struct Rect
	{
		int x, y, w, h;
		bool in(int px, int py) const
		{
			return px >= x && px < x + w && py >= y && py < y + h;
		}
	};
	std::vector<Rect> cat_r_, tile_r_, recent_r_;
	std::vector<int> visible_; /* apps_ indices in grid order */
	Rect settings_r_ = {0, 0, 0, 0}, power_r_ = {0, 0, 0, 0};
	int hov_cat_ = -1, hov_tile_ = -1, hov_recent_ = -1, hov_btn_ = -1; /* btn: 0 settings, 1 power */

	/* Power pop-over above the rail's power button: Log out / Restart / Shut down. */
	bool power_open_ = false;
	int hov_pow_ = -1;
	Rect pow_r_[3] = {};

	/* Draw the power pop-over (rows recorded in pow_r_). */
	void draw_power_menu()
	{
		static const char *const labels[3] = {"Log out", "Restart", "Shut down"};
		static const char *const icons[3] = {"arrow-left", "restart", "power"};
		const ui_theme &t = g_t;
		int w = 168, h = 4 + 3 * 34 + 2 + 4;
		int x = power_r_.x + power_r_.w - w, y = power_r_.y - 8 - h;
		ui_shadow(cv_, x, y, w, h, UI_R_MD, 8, 14, t.shadow_win_a, t.shadow);
		ui_outlined_round(cv_, x, y, w, h, UI_R_MD, t.edge, t.ground);
		for (int i = 0; i < 3; i++)
		{
			pow_r_[i] = {x + 4, y + 4 + i * 35, w - 8, 34};
			const Rect &r = pow_r_[i];
			if (i == hov_pow_)
				ui_fill_round(cv_, r.x, r.y, r.w, r.h, UI_R_MD, t.neutral_800);
			ui_draw_icon_tint(cv_, ui_icon_get(16, icons[i]), r.x + 16, r.y + 9, t.text_muted);
			text(*font_, r.x + 16 + 16 + 16, r.y + r.h / 2, labels[i], t.text);
		}
	}

	static std::string lower(const std::string &in)
	{
		std::string o;
		for (char c : in)
			o += (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
		return o;
	}

	/* Text vertically centred on row cy, blended over what is drawn. */
	void text(ogScalableFont &f, int x, int cy, const char *str, uint32_t fg)
	{
		font_fg(f, fg);
		f.SetBGColor(0, 0, 0, 0);
		f.PutString(surf_, x, cy - (int)f.GetHeight() / 2 + 1, str);
	}

	/* Section label: 10px, upper-case, +0.06em tracking. */
	void section(int x, int cy, const std::string &label)
	{
		std::string u;
		for (char c : label)
			u += (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
		char one[2] = {0, 0};
		for (char c : u)
		{
			one[0] = c;
			text(tiny_, x, cy, one, g_t.text_faint);
			x += (int)tiny_.TextWidth(one) + 1;
		}
	}

	/* Walk a registry menu container, collecting leaf entries into category cat. */
	void collect(const std::string &path, int cat, int depth)
	{
		char names[UB_NAMES_MAX];
		if (depth > 4 || ubistry_enum(path.c_str(), names, sizeof(names)) <= 0)
			return;
		std::string ns(names);
		size_t start = 0;
		while (start < ns.size())
		{
			size_t nl = ns.find('\n', start);
			std::string child = ns.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
			start = (nl == std::string::npos) ? ns.size() : nl + 1;
			if (child.empty())
				continue;
			std::string p = path + "/" + child;
			char buf[128], scratch[UB_NAMES_MAX];
			std::string label =
			    ubistry_get_str((p + "/label").c_str(), buf, sizeof(buf)) == 0 ? buf : child;
			if (ubistry_enum((p + "/items").c_str(), scratch, sizeof(scratch)) >= 0)
			{
				int c = cat;
				if (depth == 0) /* a top-level submenu is a category */
				{
					cats_.push_back(label);
					c = (int)cats_.size() - 1;
				}
				collect(p + "/items", c, depth + 1);
			}
			else if (ubistry_get_str((p + "/exec").c_str(), buf, sizeof(buf)) == 0)
				apps_.push_back({label, buf, cat});
		}
	}

	void load_recent()
	{
		const char *u = getenv("USER");
		recent_.clear();
		for (int i = 0; i < LP_RECENT_MAX; i++)
		{
			char l[128], e[128];
			int w = 0;
			std::string k = "views/launcher/recent/" + std::to_string(i);
			if (ubistry_get_for(u, (k + "/label").c_str(), l, sizeof(l)) != 0 ||
			    ubistry_get_for(u, (k + "/exec").c_str(), e, sizeof(e)) != 0)
				break;
			ubistry_get_for_int(u, (k + "/when").c_str(), &w);
			recent_.push_back({l, e, (long)w});
		}
	}

	static std::string ago(long when)
	{
		static const char *const wd[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
		long d = (long)gettime() - when;
		if (d < 60)
			return "now";
		if (d < 3600)
			return std::to_string(d / 60) + " min";
		if (d < 86400)
			return std::to_string(d / 3600) + " h";
		time_t t = (time_t)when;
		struct tm *lt = localtime(&t);
		return lt ? wd[lt->tm_wday % 7] : "";
	}

	void filter()
	{
		visible_.clear();
		std::string q = lower(query_);
		for (int i = 0; i < (int)apps_.size(); i++)
		{
			if (!q.empty())
			{
				if (lower(apps_[i].label).find(q) == std::string::npos)
					continue;
			}
			else if (cat_ != 0 && apps_[i].cat != cat_)
				continue;
			visible_.push_back(i);
		}
	}

	void draw()
	{
		const ui_theme &t = g_t;
		int px = LP_MARGIN_X, py = LP_MARGIN_T;
		ui_fill(cv_, 0, 0, ww_, wh_, t.panel);

		/* --- rail ------------------------------------------------------ */
		ui_fill(cv_, px, py, LP_RAIL_W, ph_, t.surface);
		ui_fill(cv_, px + LP_RAIL_W, py, 1, ph_, ui_mix(t.panel, t.divider, t.divider_a));

		const char *user = getenv("USER");
		std::string un = (user && user[0]) ? user : "user";
		int ay = py + 16;
		ui_outlined_round(cv_, px + 16, ay, 32, 32, 16, t.accent, t.accent_900);
		char ini[2] = {(char)((un[0] >= 'a' && un[0] <= 'z') ? un[0] - 32 : un[0]), 0};
		text(*bold_, px + 16 + (32 - (int)bold_->TextWidth(ini)) / 2, ay + 16, ini, t.accent_200);
		text(*font_, px + 16 + 32 + 8, ay + 9, un.c_str(), t.text);
		char host[64];
		if (ubistry_get_str("/system/hostname", host, sizeof(host)) != 0)
			std::strncpy(host, "ubix.local", sizeof(host) - 1), host[sizeof(host) - 1] = '\0';
		text(tiny_, px + 16 + 32 + 8, ay + 25, host, t.text_faint);

		static const char *const cat_icons[] = {
		    "grid", "folder", "monitor", "sliders", "terminal", "code", "music"};
		cat_r_.clear();
		int ry = ay + 32 + 16;
		for (int i = 0; i < (int)cats_.size(); i++)
		{
			Rect r = {px + 8, ry, LP_RAIL_W - 16, UI_ROW_H};
			cat_r_.push_back(r);
			bool sel = (i == cat_ && query_.empty());
			if (sel)
				ui_fill_round(cv_, r.x, r.y, r.w, r.h, UI_R_MD, t.accent_900);
			else if (i == hov_cat_)
				ui_fill_round(cv_, r.x, r.y, r.w, r.h, UI_R_MD, t.raised);
			uint32_t fg = sel ? t.accent_200 : t.text;
			ui_draw_icon_tint(cv_,
			                  ui_icon_get(16, cat_icons[i % 7]),
			                  r.x + 8,
			                  r.y + 6,
			                  sel ? t.accent_400 : t.text_muted);
			text(*font_, r.x + 8 + 16 + 8, r.y + r.h / 2, cats_[i].c_str(), fg);
			int n = 0;
			for (auto &a : apps_)
				n += (i == 0 || a.cat == i);
			std::string ns = std::to_string(n);
			text(tiny_,
			     r.x + r.w - 8 - (int)tiny_.TextWidth(ns.c_str()),
			     r.y + r.h / 2,
			     ns.c_str(),
			     t.text_faint);
			ry += UI_ROW_H + 2;
		}

		/* rail actions: Settings (left), Log out (right) */
		int by = py + ph_ - 8 - UI_CONTROL_H;
		settings_r_ = {px + 8, by, UI_CONTROL_H, UI_CONTROL_H};
		power_r_ = {px + LP_RAIL_W - 8 - UI_CONTROL_H, by, UI_CONTROL_H, UI_CONTROL_H};
		const Rect *btns[2] = {&settings_r_, &power_r_};
		const char *bicons[2] = {"sliders", "power"};
		for (int i = 0; i < 2; i++)
		{
			const Rect &b = *btns[i];
			if (hov_btn_ == i || (i == 1 && power_open_))
				ui_fill_round(cv_, b.x, b.y, b.w, b.h, UI_R_MD, t.raised);
			ui_draw_icon_tint(cv_, ui_icon_get(16, bicons[i]), b.x + 8, b.y + 8, t.text_muted);
		}

		/* --- main column ------------------------------------------------ */
		int mx = px + LP_RAIL_W + 1 + 16, mw = pw_ - LP_RAIL_W - 1 - 32;
		int sy = py + 16;
		ui_outlined_round(cv_, mx, sy, mw, 36, 18, t.accent, t.ground);
		ui_focus_ring(cv_, mx, sy, mw, 36, 18, t.accent);
		/* keycap hint: Super opens the launcher */
		{
			int kw = 12 + (int)tiny_.TextWidth("Super");
			int kx = mx + mw - 10 - kw;
			ui_outlined_round(cv_, kx, sy + 9, kw, 18, UI_R_SM, t.edge, t.surface);
			text(tiny_, kx + 6, sy + 18, "Super", t.text_muted);
		}
		ui_draw_icon_tint(cv_, ui_icon_get(16, "search"), mx + 12, sy + 10, t.text_muted);
		int qx = mx + 12 + 16 + 8;
		if (query_.empty())
			text(*font_, qx + 3, sy + 18, "Search apps, or type a command to run...", t.text_faint);
		else
			text(*font_, qx, sy + 18, query_.c_str(), t.text);
		ui_fill(cv_, qx + (int)font_->TextWidth(query_.c_str()) + 1, sy + 10, 1, 16, t.accent_400);

		bool searching = !query_.empty();
		int hy = sy + 36 + 12;
		section(mx + 8, hy + 14, searching ? "Results" : (cat_ == 0 ? "All apps" : cats_[cat_]));
		int gy = hy + 28;

		tile_r_.clear();
		int max_rows = searching || recent_.empty() ? 4 : 3;
		int shown = 0;
		for (int vi = 0; vi < (int)visible_.size() && shown < max_rows * 4; vi++, shown++)
		{
			const LaunchApp &a = apps_[visible_[vi]];
			Rect r = {
			    mx + (shown % 4) * (LP_TILE_W + 4), gy + (shown / 4) * LP_TILE_H, LP_TILE_W, LP_TILE_H};
			tile_r_.push_back(r);
			if (vi == hov_tile_ || (searching && vi == 0))
				ui_fill_round(cv_, r.x, r.y, r.w, r.h, UI_R_MD, t.surface);
			draw_app_icon(cv_, a.label, a.exec, 48, r.x + (r.w - 48) / 2, r.y + 8);
			char lbl[40];
			fit_label(*small_, lbl, sizeof(lbl), a.label.c_str(), r.w - 8);
			text(*small_, r.x + (r.w - (int)small_->TextWidth(lbl)) / 2, r.y + 8 + 48 + 6 + 8, lbl, t.text);
		}
		if (visible_.empty())
			text(*font_,
			     mx + 8,
			     gy + 16,
			     searching ? "No app matches - Enter runs it as a command" : "Nothing here yet",
			     t.text_muted);

		/* Continue: recently launched apps */
		recent_r_.clear();
		if (!searching && !recent_.empty())
		{
			int cy = gy + 3 * LP_TILE_H + 12;
			section(mx + 8, cy + 14, "Continue");
			cy += 28;
			for (int i = 0; i < (int)recent_.size(); i++)
			{
				Rect r = {mx, cy, mw, UI_ROW_H};
				recent_r_.push_back(r);
				if (i == hov_recent_)
					ui_fill_round(cv_, r.x, r.y, r.w, r.h, UI_R_MD, t.surface);
				draw_app_icon(cv_, recent_[i].label, recent_[i].exec, 16, r.x + 8, r.y + 6);
				text(*font_, r.x + 8 + 16 + 8, r.y + r.h / 2, recent_[i].label.c_str(), t.text);
				std::string w = ago(recent_[i].when);
				text(tiny_,
				     r.x + r.w - 8 - (int)tiny_.TextWidth(w.c_str()),
				     r.y + r.h / 2,
				     w.c_str(),
				     t.text_faint);
				cy += UI_ROW_H;
			}
		}

		if (power_open_)
			draw_power_menu();

		ui_alpha_shape(cv_,
		               px,
		               py,
		               pw_,
		               ph_,
		               UI_R_XL,
		               t.panel_a,
		               t.edge_strong,
		               t.shadow_panel_y,
		               t.shadow_panel_reach,
		               t.shadow_panel_a,
		               t.shadow);
	}

      public:
	void init(ogScalableFont *font, ogScalableFont *small, ogScalableFont *bold)
	{
		font_ = font;
		small_ = small;
		bold_ = bold;
		tiny_.Load(FONT_PATH, 12);
	}

	bool is_open() const
	{
		return open_;
	}
	uint32_t win_id() const
	{
		return win_id_;
	}

	/* Open above the taskbar strip, starting on "All" with an empty search. */
	void show(int screen_w, int screen_h, ubix::Mailbox &mbox)
	{
		if (open_)
			return;
		cats_.assign(1, "All");
		apps_.clear();
		collect("/views/startmenu", 0, 0);
		/* top-level leaves (Settings, About) belong to a "System" category */
		bool loose = false;
		for (auto &a : apps_)
			loose |= (a.cat == 0);
		if (loose)
		{
			cats_.push_back("System");
			for (auto &a : apps_)
				if (a.cat == 0)
					a.cat = (int)cats_.size() - 1;
		}
		load_recent();
		cat_ = 0;
		query_.clear();
		hov_cat_ = hov_tile_ = hov_recent_ = hov_btn_ = hov_pow_ = -1;
		power_open_ = false;
		filter();

		pw_ = std::min(LP_W, screen_w - 2 * UI_TASKBAR_INSET);
		ph_ = std::min(LP_H, screen_h - TB_H - LP_MARGIN_T);
		ww_ = LP_MARGIN_X + pw_ + 12;
		wh_ = LP_MARGIN_T + ph_ + LP_MARGIN_B;
		wx_ = UI_TASKBAR_INSET - LP_MARGIN_X;
		wy_ = screen_h - TB_H - ph_ - LP_MARGIN_T;

		mpi_message_t claim = {};
		struct display_claim_req *creq = (struct display_claim_req *)claim.data;
		claim.header = DISPLAY_CLAIM;
		creq->x = wx_;
		creq->y = wy_;
		creq->w = ww_;
		creq->h = wh_;
		creq->sender_pid = ubix::pid();
		creq->no_decor = 1;
		creq->wants_motion = 1;
		creq->translucent = 1;
		std::strncpy(creq->title, "launcher", sizeof(creq->title) - 1);
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		mpi_message_t reply;
		await_reply(mbox, reply, DISPLAY_ACK, DISPLAY_DENIED);
		if (reply.header != DISPLAY_ACK)
			return;
		struct display_ack *da = (struct display_ack *)reply.data;
		win_id_ = da->window_id;
		surf_.ogAttach(da->shm_base, (uint32_t)ww_, (uint32_t)wh_, OG_PIXFMT_32BPP);
		cv_.px = (uint32_t *)surf_.ogGetPtr(0, 0);
		cv_.w = ww_;
		cv_.h = wh_;
		cv_.stride = (int)((uint8_t *)surf_.ogGetPtr(0, 1) - (uint8_t *)surf_.ogGetPtr(0, 0)) / 4;
		open_ = true;
		draw();
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

	void redraw()
	{
		if (!open_)
			return;
		draw();
		send_flip_msg(win_id_);
	}

	/* Record a launch at the top of the user's recent list (deduplicated). */
	void remember(const std::string &label, const std::string &exec)
	{
		const char *u = getenv("USER");
		if (u == nullptr || u[0] == '\0')
			return;
		load_recent();
		std::vector<Recent> r;
		r.push_back({label, exec, (long)gettime()});
		for (auto &e : recent_)
			if (e.exec != exec && (int)r.size() < LP_RECENT_MAX)
				r.push_back(e);
		for (int i = 0; i < (int)r.size(); i++)
		{
			std::string k = "views/launcher/recent/" + std::to_string(i);
			ubistry_set_user(u, (k + "/label").c_str(), r[i].label.c_str());
			ubistry_set_user(u, (k + "/exec").c_str(), r[i].exec.c_str());
			ubistry_set_user_int(u, (k + "/when").c_str(), (int)r[i].when);
		}
	}

	/* Pointer event on the launcher window (window-local coordinates). */
	LaunchAction on_mouse(const display_mouse_ev *me, bool click)
	{
		LaunchAction act;

		/* While the power pop-over is open it takes the pointer. */
		if (power_open_)
		{
			int hp = -1;
			for (int i = 0; i < 3 && me->x >= 0; i++)
				if (pow_r_[i].in(me->x, me->y))
					hp = i;
			if (hp != hov_pow_)
			{
				hov_pow_ = hp;
				act.kind = LaunchAction::REDRAW;
			}
			if (!click)
				return act;
			power_open_ = false;
			hov_pow_ = -1;
			if (hp == 0)
				act.kind = LaunchAction::LOGOUT;
			else if (hp == 1)
				act.kind = LaunchAction::RESTART;
			else if (hp == 2)
				act.kind = LaunchAction::SHUTDOWN;
			else
				act.kind = LaunchAction::REDRAW; /* clicked elsewhere: just close it */
			return act;
		}

		int hc = -1, ht = -1, hr = -1, hb = -1;
		if (me->x >= 0)
		{
			for (int i = 0; i < (int)cat_r_.size(); i++)
				if (cat_r_[i].in(me->x, me->y))
					hc = i;
			for (int i = 0; i < (int)tile_r_.size(); i++)
				if (tile_r_[i].in(me->x, me->y))
					ht = i;
			for (int i = 0; i < (int)recent_r_.size(); i++)
				if (recent_r_[i].in(me->x, me->y))
					hr = i;
			hb = settings_r_.in(me->x, me->y) ? 0 : (power_r_.in(me->x, me->y) ? 1 : -1);
		}
		if (hc != hov_cat_ || ht != hov_tile_ || hr != hov_recent_ || hb != hov_btn_)
		{
			hov_cat_ = hc;
			hov_tile_ = ht;
			hov_recent_ = hr;
			hov_btn_ = hb;
			act.kind = LaunchAction::REDRAW;
		}
		if (!click)
			return act;
		if (hc >= 0)
		{
			cat_ = hc;
			query_.clear();
			filter();
			hov_tile_ = -1;
			act.kind = LaunchAction::REDRAW;
		}
		else if (ht >= 0 && ht < (int)visible_.size())
		{
			act.kind = LaunchAction::LAUNCH;
			act.label = apps_[visible_[ht]].label;
			act.exec = apps_[visible_[ht]].exec;
		}
		else if (hr >= 0)
		{
			act.kind = LaunchAction::LAUNCH;
			act.label = recent_[hr].label;
			act.exec = recent_[hr].exec;
		}
		else if (hb == 0)
		{
			act.kind = LaunchAction::LAUNCH;
			act.label = "Settings";
			act.exec = "/bin/settings";
		}
		else if (hb == 1)
		{
			power_open_ = true;
			hov_pow_ = -1;
			act.kind = LaunchAction::REDRAW;
		}
		return act;
	}

	/* Keystroke while the launcher has focus: type to search, Enter to run. */
	LaunchAction on_key(uint32_t kc)
	{
		LaunchAction act;
		act.kind = LaunchAction::REDRAW;
		if (kc == 0x1B && power_open_)
			power_open_ = false; /* Esc closes the pop-over first */
		else if (kc == 0x1B)
			act.kind = LaunchAction::CLOSE;
		else if (kc == '\n' || kc == '\r')
		{
			if (!visible_.empty() && (!query_.empty() || cat_ != 0))
			{
				act.kind = LaunchAction::LAUNCH;
				act.label = apps_[visible_[0]].label;
				act.exec = apps_[visible_[0]].exec;
			}
			else if (!query_.empty())
			{
				act.kind = LaunchAction::LAUNCH; /* not an app: run it as a command */
				act.label = query_;
				act.exec = query_[0] == '/' ? query_ : "/bin/" + query_;
			}
			else
				act.kind = LaunchAction::NONE;
		}
		else if (kc == '\b' || kc == 0x7F)
		{
			if (!query_.empty())
				query_.pop_back();
			filter();
		}
		else if (kc >= 0x20 && kc < 0x7F && query_.size() < 48)
		{
			query_ += (char)kc;
			filter();
		}
		else
			act.kind = LaunchAction::NONE;
		hov_tile_ = -1;
		return act;
	}
};

class Taskbar
{
	ogSurface surf_;
	ogScalableFont font_;       /* 12px regular */
	ogScalableFont font_bold_;  /* 12px bold: launcher label, clock time */
	ogScalableFont font_small_; /* 11px: volume % */
	ogScalableFont font_clock_; /* 12.5px: tray date + time */
	uint32_t win_id_ = 0;
	uint32_t sw_ = 0;
	uint32_t sh_ = 0;
	std::vector<TrackedWin> tracked_;
	MixerFlyout mixer_;
	LauncherPanel panel_;           /* the uBix launcher pop-over */
	CalendarPanel cal_;             /* the clock\'s month pop-over */
	ubix::Mailbox *mbox_ = nullptr; /* our mailbox (set in init) */
	Launcher launcher_;
	bool btn_pressed_ = false;
	bool prev_down_ = false;  /* previous button-1 state, for release-edge clicks */
	bool prev_mdown_ = false; /* previous middle-button state */
	uint32_t focused_id_ = 0; /* active window (DISPLAY_FOCUS); highlights its tab */

	/* Hover state on the strip (updated from pointer-motion events). */
	bool hover_start_ = false; /* cursor over the launcher pill */
	int hover_tab_ = -1;       /* window item under the cursor, or -1 */
	bool hover_vol_ = false;   /* cursor over the volume tray item */
	bool hover_clock_ = false; /* cursor over the clock */

	/* Bar layout (layout()): floating + rounded normally; on small screens
	 * (< 1000px wide, e.g. 800x480 Pi panels) edge-to-edge, square, and the
	 * tray drops the date (design NOTES, "Smaller screens"). */
	bool compact_ = false;
	int bar_x_ = BAR_X, bar_y_ = BAR_Y, item_y_ = ITEM_Y, task_w_ = TASK_W, bar_r_ = UI_R_XL;

	void layout()
	{
		compact_ = sw_ < UI_COMPACT_W;
		bar_x_ = compact_ ? 0 : BAR_X;
		bar_y_ = compact_ ? TB_H - BAR_H : BAR_Y;
		bar_r_ = compact_ ? 0 : UI_R_XL;
		item_y_ = bar_y_ + (BAR_H - ITEM_H) / 2;
		task_w_ = TASK_W;
	}

	/* Geometry from the last draw_strip(), used by the hit tests (strip-local x). */
	int start_x_ = 0, start_w_ = 0;
	int tabs_x_ = 0, tabs_n_ = 0;
	int vol_x_ = 0, vol_w_ = 0;
	int net_x_ = 0, net_w_ = 0;
	bool hover_net_ = false;

	/* Network item: default interface state (ubix_net_status, polled 1/s). */
	bool net_have_ = false, net_online_ = false;
	int clock_x_ = 0, clock_w_ = 0;

	/* System tray: master volume mirrored from ubistry (/aural/volume, /aural/mute). */
	int volume_ = 100;
	bool muted_ = false;

	/* The strip's pixels (set by attach()); shapes are drawn with views/uidraw.h. */
	ui_canvas cv_;

	/* Point surf_ and cv_ at the (re)claimed shared buffer. */
	void attach(void *shm)
	{
		surf_.ogAttach(shm, sw_, TB_H, OG_PIXFMT_32BPP);
		cv_.px = (uint32_t *)surf_.ogGetPtr(0, 0);
		cv_.w = (int)sw_;
		cv_.h = TB_H;
		cv_.stride = (int)((uint8_t *)surf_.ogGetPtr(0, 1) - (uint8_t *)surf_.ogGetPtr(0, 0)) / 4;
	}

	/* Draw text vertically centred on row cy, anti-aliased over whatever is
	 * already drawn (transparent background: no cell fill). */
	void text(ogScalableFont &f, int x, int cy, const char *str, uint32_t fg)
	{
		font_fg(f, fg);
		f.SetBGColor(0, 0, 0, 0);
		f.PutString(surf_, x, cy - (int)f.GetHeight() / 2 + 1, str);
	}

	/**
	 * Give the strip its floating shape: per-pixel alpha for the rounded bar
	 * (92% fill, opaque 1px edge) and its soft offset shadow below/around it;
	 * everything else is fully clear.  Runs after all content is drawn.
	 */
	void shape_pass()
	{
		ui_alpha_shape(cv_,
		               bar_x_,
		               bar_y_,
		               (int)sw_ - 2 * bar_x_,
		               BAR_H,
		               bar_r_,
		               g_t.taskbar_a,
		               g_t.edge,
		               g_t.shadow_bar_y,
		               g_t.shadow_bar_reach,
		               g_t.shadow_bar_a,
		               g_t.shadow);
	}

	void draw_strip()
	{
		layout();
		int sw = (int)sw_;
		int bw = sw - 2 * bar_x_;
		const int cy = item_y_ + ITEM_H / 2;

		/* Bar fill over its bounding box; shape_pass() rounds and edges it. */
		ui_fill(cv_, 0, 0, sw, TB_H, g_t.taskbar);

		/* Launcher pill: ubix mark + "uBix", outlined; accent when the menu is open. */
		bool open = panel_.is_open();
		start_x_ = bar_x_ + 1 + 4;
		start_w_ = 8 + 24 + 8 + (int)font_bold_.TextWidth("uBix") + 16;
		uint32_t pill_bg = open ? g_t.accent_900 : ((hover_start_ || btn_pressed_) ? g_t.raised : g_t.taskbar);
		ui_outlined_round(
		    cv_, start_x_, item_y_, start_w_, ITEM_H, UI_R_PILL, open ? g_t.accent : g_t.edge, pill_bg);
		ui_draw_icon(cv_, ui_icon_get(24, "ubix-mark"), start_x_ + 8, item_y_ + 4);
		text(font_bold_, start_x_ + 8 + 24 + 8, cy, "uBix", g_t.text);

		/* Divider. */
		int x = start_x_ + start_w_ + 4 + 4;
		ui_fill(cv_, x, bar_y_ + (BAR_H - 20) / 2, 1, 20, ui_mix(g_t.taskbar, g_t.divider, g_t.divider_a));
		x += 1 + 4 + 4;

		/* Tray, laid out from the right: [volume] [clock]. */
		char date[24], tstr[16];
		get_clock_strs(date, sizeof(date), tstr, sizeof(tstr));
		if (compact_)
			date[0] = '\0'; /* small screens keep just the time */
		int right = bar_x_ + bw - 1 - 4;
		clock_w_ =
		    16 + (date[0] ? (int)font_clock_.TextWidth(date) + 12 : 0) + (int)font_clock_.TextWidth(tstr) + 16;
		clock_x_ = right - clock_w_;
		char vol[8];
		std::snprintf(vol, sizeof(vol), "%d%%", muted_ ? 0 : volume_);
		vol_w_ = 8 + 16 + 4 + (int)font_small_.TextWidth(vol) + 8;
		vol_x_ = clock_x_ - 2 - vol_w_;
		net_w_ = net_have_ ? 32 : 0;
		net_x_ = vol_x_ - (net_w_ ? 2 + net_w_ : 0);

		/* Running windows: icon + title, a short mark on top (accent = focused). */
		tabs_x_ = x;
		tabs_n_ = 0;
		/* Items drop their label (40px, icon only) when 180px items won't fit. */
		int avail = net_x_ - 8 - x;
		if (!tracked_.empty() && (int)tracked_.size() * (TASK_W + 4) > avail)
			task_w_ = 40;
		bool icon_only = task_w_ < 100;
		int i = 0;
		for (const auto &tw : tracked_)
		{
			if (x + task_w_ > net_x_ - 8)
				break;
			bool active = (tw.id == focused_id_);
			bool hover = (i == hover_tab_);
			uint32_t bg = active ? g_t.neutral_800 : (hover ? g_t.raised : g_t.taskbar);
			if (active || hover)
				ui_fill_round(cv_, x, item_y_, task_w_, ITEM_H, UI_R_MD, bg);
			ui_fill_round(
			    cv_, x + task_w_ / 2 - 8, item_y_, 16, 2, 1, active ? g_t.accent : g_t.neutral_600);
			draw_app_icon(cv_, tw.title, "", 16, icon_only ? x + (task_w_ - 16) / 2 : x + 8, item_y_ + 8);
			if (!icon_only)
			{
				char label[48];
				fit_label(font_, label, sizeof(label), tw.title.c_str(), task_w_ - 32 - 12);
				text(font_, x + 32, cy, label, active ? g_t.text : g_t.text_muted);
			}
			x += task_w_ + 4;
			i++;
			tabs_n_++;
		}

		/* Network tray item: faded while the link is down / no address yet. */
		if (net_w_)
		{
			if (hover_net_)
				ui_fill_round(cv_, net_x_, item_y_, net_w_, ITEM_H, UI_R_MD, g_t.raised);
			uint32_t nc = net_online_ ? g_t.text_muted : ui_mix(g_t.taskbar, g_t.text_muted, 110);
			ui_draw_icon_tint(cv_, ui_icon_get(16, "ethernet"), net_x_ + 8, item_y_ + 8, nc);
		}

		/* Volume tray item. */
		if (hover_vol_ || mixer_.is_open())
			ui_fill_round(cv_, vol_x_, item_y_, vol_w_, ITEM_H, UI_R_MD, g_t.raised);
		ui_draw_icon_tint(
		    cv_, ui_icon_get(16, muted_ ? "volume-mute" : "volume"), vol_x_ + 8, item_y_ + 8, g_t.text_muted);
		text(font_small_, vol_x_ + 8 + 16 + 4, cy, vol, g_t.text_muted);

		/* Clock: muted date + bright time, same size (design: 12.5px, time 500). */
		if (hover_clock_)
			ui_fill_round(cv_, clock_x_, item_y_, clock_w_, ITEM_H, UI_R_MD, g_t.raised);
		text(font_clock_, clock_x_ + 16, cy, date, g_t.neutral_400);
		text(font_clock_, clock_x_ + 16 + (int)font_clock_.TextWidth(date) + 12, cy, tstr, g_t.text);

		shape_pass();
	}

	bool start_hit(int mx) const
	{
		return mx >= start_x_ && mx < start_x_ + start_w_;
	}

	/* Tray hit-test: true if mx is over the volume item. */
	bool vol_hit(int mx) const
	{
		return mx >= vol_x_ && mx < vol_x_ + vol_w_;
	}

	bool net_hit(int mx) const
	{
		return net_w_ && mx >= net_x_ && mx < net_x_ + net_w_;
	}

	bool clock_hit(int mx) const
	{
		return mx >= clock_x_ && mx < clock_x_ + clock_w_;
	}

	int winbtn_hit(int mx) const
	{
		for (int i = 0; i < tabs_n_; i++)
		{
			int x = tabs_x_ + i * (task_w_ + 4);
			if (mx >= x && mx < x + task_w_)
				return i;
		}
		return -1;
	}

	/* Ask views to minimize / close a window (DISPLAY_WINCTL). */
	void win_ctl(uint32_t id, uint32_t op)
	{
		mpi_message_t msg = {};
		struct display_winctl *wc = (struct display_winctl *)msg.data;
		msg.header = DISPLAY_WINCTL;
		wc->window_id = id;
		wc->op = op;
		ubix::post_message("views", DISPLAY_WINCTL, msg);
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
		mbox_ = &mbox;
		launcher_.init();

		/* Query screen geometry */
		mpi_message_t msg = {};
		struct display_query *dq = (struct display_query *)msg.data;
		msg.header = DISPLAY_QUERY;
		std::strncpy(dq->reply, "taskbar", sizeof(dq->reply) - 1);
		dq->reply[sizeof(dq->reply) - 1] = '\0';
		ubix::post_message("views", DISPLAY_QUERY, msg);

		mpi_message_t reply;
		await_reply(mbox, reply, DISPLAY_INFO, DISPLAY_INFO);
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
		creq->translucent = 1;  /* floating bar: we draw shape + shadow in alpha */
		std::strncpy(creq->title, "taskbar", sizeof(creq->title) - 1);
		creq->title[sizeof(creq->title) - 1] = '\0';
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		creq->reply[sizeof(creq->reply) - 1] = '\0';
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		await_reply(mbox, reply, DISPLAY_ACK, DISPLAY_DENIED);
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

		attach(shm);

		if (!font_.Load(font_path, FONT_SIZE))
		{
			std::printf("taskbar: font load failed\n");
			return false;
		}
		if (!font_bold_.Load(FONT_BOLD_PATH, FONT_SIZE))
			font_bold_.Load(font_path, FONT_SIZE); /* fall back to regular */
		font_small_.Load(font_path, FONT_SMALL_SIZE);
		font_clock_.Load(font_path, FONT_CLOCK_SIZE);
		panel_.init(&font_, &font_small_, &font_bold_);
		cal_.init(&font_, &font_small_);

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
		cal_.refresh(); /* an open calendar follows the clock */
	}

	void send_flip()
	{
		send_flip_msg(win_id_);
	}

	/* Re-read master volume/mute from ubistry into the cached members. */
	/* Re-read the default interface's state; true if it changed. */
	bool refresh_net()
	{
		struct net_status ns;
		bool have = ubix_net_status(&ns) == 0;
		bool online = have && ns.up && ns.link && (ns.ip[0] | ns.ip[1] | ns.ip[2] | ns.ip[3]) != 0;
		bool changed = have != net_have_ || online != net_online_;
		net_have_ = have;
		net_online_ = online;
		return changed;
	}

	bool refresh_volume()
	{
		int v = 0, m = 0;
		int old_vol = volume_;
		bool old_mute = muted_;
		if (ubistry_get_int("/aural/volume", &v) == 0)
			volume_ = v < 0 ? 0 : (v > 100 ? 100 : v);
		muted_ = (ubistry_get_int("/aural/mute", &m) == 0) && (m != 0);
		return volume_ != old_vol || muted_ != old_mute;
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
		creq->translucent = 1;  /* floating bar: we draw shape + shadow in alpha */
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
				g_deferred.push_back(reply); /* handled by main() afterwards */
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
		attach(da->shm_base);
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
		mixer_.hide();
		panel_.hide();
		cal_.hide();
	}

	/* Carry out what the launcher asked for. */
	void do_launch_action(const LaunchAction &act)
	{
		switch (act.kind)
		{
			case LaunchAction::REDRAW:
				panel_.redraw();
				break;
			case LaunchAction::CLOSE:
				close_menus();
				draw_strip();
				send_flip();
				break;
			case LaunchAction::LAUNCH:
			{
				MenuItem it;
				it.label = act.label;
				it.exec = act.exec;
				panel_.remember(act.label, act.exec);
				close_menus();
				draw_strip();
				send_flip();
				dispatch(it);
				break;
			}
			case LaunchAction::RESTART:
			case LaunchAction::SHUTDOWN:
				close_menus();
				power(act.kind == LaunchAction::SHUTDOWN);
				break;
			case LaunchAction::LOGOUT:
				close_menus();
				::exit(0); /* log out — vlogin shows the lock screen */
			default:
				break;
		}
	}

	/**
	 * Restart or shut down: cover the screen with the power page (so the user
	 * sees what is happening while disks sync), then ask for it via
	 * ubix_power() — through init when the session user is not root.
	 */
	void power(bool off)
	{
		mpi_message_t claim = {};
		struct display_claim_req *creq = (struct display_claim_req *)claim.data;
		claim.header = DISPLAY_CLAIM;
		creq->x = 0;
		creq->y = 0;
		creq->w = (int32_t)sw_;
		creq->h = (int32_t)sh_;
		creq->sender_pid = ubix::pid();
		creq->no_decor = 1;
		std::strncpy(creq->title, "power", sizeof(creq->title) - 1);
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		mpi_message_t reply;
		await_reply(*mbox_, reply, DISPLAY_ACK, DISPLAY_DENIED);
		if (reply.header == DISPLAY_ACK)
		{
			struct display_ack *da = (struct display_ack *)reply.data;
			ogSurface ps;
			ps.ogAttach(da->shm_base, sw_, sh_, OG_PIXFMT_32BPP);
			ui_canvas pc;
			pc.px = (uint32_t *)ps.ogGetPtr(0, 0);
			pc.w = (int)sw_;
			pc.h = (int)sh_;
			pc.stride = (int)((uint8_t *)ps.ogGetPtr(0, 1) - (uint8_t *)ps.ogGetPtr(0, 0)) / 4;
			ogScalableFont title;
			title.Load(FONT_PATH, 19); /* 16px */
			ui_power_screen(pc,
			                ps,
			                title,
			                font_,
			                off ? "Shutting down..." : "Restarting...",
			                "Closing apps and syncing disks");
			send_flip_msg(da->window_id);
			struct timespec ts = {0, 700 * 1000 * 1000}; /* let it be seen */
			nanosleep(&ts, nullptr);
		}
		ubix_power(off ? UBIX_POWER_OFF : UBIX_POWER_REBOOT);
	}

	/* A key while one of our pop-overs has focus (only the launcher types). */
	void on_key(const display_key *dk)
	{
		if (!dk->pressed || !panel_.is_open())
			return;
		do_launch_action(panel_.on_key(dk->keycode));
	}

	/* Super key: open the launcher (or close it if open). */
	void toggle_launcher(ubix::Mailbox &mbox)
	{
		bool was = panel_.is_open();
		close_menus();
		if (!was)
			panel_.show((int)sw_, (int)sh_, mbox);
		draw_strip();
		send_flip();
	}

	/* Focus moved to another window: an open launcher closes, like a menu. */
	void focus_changed(uint32_t id)
	{
		if (id != 0 && panel_.is_open())
		{
			close_menus();
			draw_strip();
			send_flip();
		}
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

	/* Update hover highlight from a pointer event (motion or click).  Negative
	 * coords are the "cursor left" signal; they clear the relevant hover. */
	void update_hover(const display_mouse_ev *me)
	{
		bool exited = (me->x < 0);
		if (me->window_id == win_id_)
		{
			bool hs = !exited && start_hit(me->x);
			int ht = exited ? -1 : winbtn_hit(me->x);
			bool hv = !exited && vol_hit(me->x);
			bool hc = !exited && clock_hit(me->x);
			bool hn = !exited && net_hit(me->x);
			if (hs != hover_start_ || ht != hover_tab_ || hv != hover_vol_ || hc != hover_clock_ ||
			    hn != hover_net_)
			{
				hover_start_ = hs;
				hover_tab_ = ht;
				hover_vol_ = hv;
				hover_clock_ = hc;
				hover_net_ = hn;
				draw_strip();
				send_flip();
			}
		}
	}

	void on_mouse(const display_mouse_ev *me, ubix::Mailbox &mbox)
	{
		bool down = (me->buttons & 1) != 0;
		bool click = !down && prev_down_; /* release edge = a completed click */
		prev_down_ = down;
		bool mdown = (me->buttons & 4) != 0; /* middle button */
		bool mclick = !mdown && prev_mdown_;
		prev_mdown_ = mdown;

		update_hover(me);

		/* Launcher: hover, categories, launching. */
		if (panel_.is_open() && me->window_id == panel_.win_id())
		{
			do_launch_action(panel_.on_mouse(me, click));
			return;
		}

		/* Mixer flyout: drag a slider (press + motion); clicks stay inside it. */
		if (me->window_id == mixer_.win_id() && mixer_.is_open())
		{
			mixer_.on_event(me, font_);
			if (mixer_.master() != volume_ || mixer_.master_muted() != muted_)
			{
				volume_ = mixer_.master(); /* the tray follows the slider live */
				muted_ = mixer_.master_muted();
				draw_strip();
				send_flip();
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
		if (mclick && me->window_id == win_id_)
		{
			int mi = winbtn_hit(me->x); /* middle-click closes the window */
			if (mi >= 0)
				win_ctl(tracked_[mi].id, WINCTL_CLOSE);
			return;
		}
		if (!click)
			return;

		int wi = winbtn_hit(me->x);
		if (wi >= 0)
		{
			/* Design: clicking the focused window's item minimizes it; any
			 * other (or minimized) window is raised / restored. */
			close_menus();
			if (tracked_[wi].id == focused_id_)
				win_ctl(tracked_[wi].id, WINCTL_MINIMIZE);
			else
				raise_window(tracked_[wi].id);
			return;
		}
		if (net_hit(me->x))
		{
			close_menus();
			MenuItem it;
			it.label = "Settings";
			it.exec = "/bin/settings network"; /* opens on the Network pane */
			dispatch(it);
			return;
		}
		if (clock_hit(me->x))
		{
			bool was = cal_.is_open();
			close_menus();
			if (!was)
				cal_.show(clock_x_ + clock_w_, (int)sh_, mbox);
			return;
		}
		if (vol_hit(me->x))
		{
			/* Toggle the mixer flyout: master volume + a slider per live app. */
			if (mixer_.is_open())
			{
				close_menus();
			}
			else
			{
				close_menus();
				int tray_right = vol_x_ + vol_w_;
				mixer_.set_master(volume_, muted_);
				mixer_.show(tray_right, (int)sh_, mbox, font_);
			}
			return;
		}
		bool in_btn = start_hit(me->x);
		if (in_btn)
		{
			if (panel_.is_open())
				close_menus();
			else
			{
				close_menus();
				panel_.show((int)sw_, (int)sh_, mbox);
			}
			draw_strip(); /* pill shows the open state */
			send_flip();
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

	int last_sec = -1, last_min = gettime() / 60;
	for (;;)
	{
		/* Once a second, pick up volume/mute changes; repaint only when the
		 * volume or the clock's minute actually changed (the strip and its
		 * shape pass are not free on Pi-class hardware). */
		int now = gettime();
		if (now % 60 != last_sec)
		{
			last_sec = now % 60;
			bool changed = tb.refresh_volume();
			changed |= tb.refresh_net();
			if (changed || now / 60 != last_min)
			{
				last_min = now / 60;
				tb.draw();
				tb.send_flip();
			}
		}

		/* Block on the mailbox instead of busy-yielding: wake instantly on a client
		 * message (input/focus/notify/theme/resize), or after ~0.2 s (20 ticks) to
		 * repaint the live seconds clock.  Drops an idle taskbar from ~82% CPU to
		 * ~1% — the residual is the unavoidable cost of a ticking seconds display. */
		/* Messages set aside while waiting for a reply come first, in order. */
		auto next = [&](mpi_message_t &m) -> bool
		{
			if (g_deferred.empty())
				return mbox.try_fetch(m);
			m = g_deferred.front();
			g_deferred.pop_front();
			return true;
		};
		mpi_message_t reply;
		if (!next(reply) && !mbox.wait(reply, 20))
			continue;
		do
		{
			if (reply.header == DISPLAY_KEY)
			{
				tb.on_key((const display_key *)reply.data);
				continue;
			}

			if (reply.header == DISPLAY_LAUNCHER)
			{
				tb.toggle_launcher(mbox);
				continue;
			}

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
				tb.focus_changed(df->window_id);
				tb.draw();
				tb.send_flip();
				continue;
			}

			if (reply.header != DISPLAY_MOUSE)
				continue;

			struct display_mouse_ev *me = (struct display_mouse_ev *)reply.data;
			tb.on_mouse(me, mbox);
		} while (next(reply));
	}

	return 0;
}
