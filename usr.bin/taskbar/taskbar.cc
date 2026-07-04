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
#include <ctime>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogPixelFmt.h>
#include <objgfx/ogImage.h> /* PNG/BMP app icons from /usr/share/icons */
#include <ubistry/ubistry.h>
#include <audio/aural_proto.h>

extern char **environ; /* inherited session env, forwarded to launched apps */

#define FONT_PATH "/var/fonts/DejaVuSans.ttf"
#define FONT_SIZE 14

/* Taskbar geometry */
#define TB_H 32
#define BTN_W 104       /* start button: hamburger icon + "uBixOS" */
#define CLOCK_W 86      /* two stacked lines: time over date (right-aligned) */
#define CLOCK_SIZE 11   /* small clock font so both lines fit the 32px bar */
#define WIN_BTN_W 40    /* icon-only window button (no title text) */
#define WIN_BTN_ICON 22 /* glyph tile size inside a window button */
#define TRAY_W 34       /* system-tray area (volume) left of the clock */
#define TB_OPACITY 214  /* strip translucency (0..255, 255=opaque); compositor blends */
#define TB_BLUR 10      /* backdrop blur radius (px) — acrylic frost behind the strip */

/* Start-menu geometry (a Menu sizes its height to its item count). */
#define MENU_W 180
#define MENU_ITEM_H 28
#define MENU_MAX_ITEMS 24
#define MENU_FOOTER_H 44 /* user + power row at the bottom of the start menu */

/* Windows 11-style pinned grid (start menu in grid mode). */
#define GRID_COLS 4       /* tiles per row                              */
#define GRID_TILE_W 116   /* per-tile cell width (icon + label)         */
#define GRID_TILE_H 92    /* per-tile cell height                       */
#define GRID_ICON 46      /* icon tile (rounded square) size            */
#define GRID_PAD 22       /* panel inner padding                        */
#define GRID_SEARCH_H 40  /* search pill row height                     */
#define GRID_SECTION_H 30 /* "Pinned" section-label row                 */

/* Mixer flyout geometry: a titled card with one vertical slider per column
 * (master + each live app).  Tuned for a calm, modern pop-over: a thin rounded
 * track, a circular knob, and generous breathing room. */
#define FLY_PAD 16       /* card inset from the window edge          */
#define MIX_HEADER_H 24  /* "Volume" title band                      */
#define MIX_COL_W 58     /* per-slider column width                  */
#define MIX_TRACK_W 5    /* slider groove width (rounded)            */
#define MIX_TRACK_TOP 60 /* y of the track top (header + value row)  */
#define MIX_TRACK_H 110  /* slider travel                            */
#define MIX_KNOB_R 8     /* knob radius                              */
#define MIX_LABEL_GAP 12 /* gap from track bottom to the name label  */

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
static bool apply_theme(void)
{
	const char *user = getenv("USER");
	int accent = 0;

	/* Resolve the accent: per-user first (ubistry_get_for falls back to the system
	 * value internally).  A zero result is pure black — never a real accent colour, so
	 * treat it as unset (a missing/zeroed per-user override) and force the seeded
	 * system default rather than blacking out the whole bar. */
	if (ubistry_get_for_int((user && user[0]) ? user : nullptr, "views/theme/accent", &accent) != 0 ||
	    ((uint32_t)accent & 0x00FFFFFFu) == 0)
	{
		if (ubistry_get_int("/views/theme/accent", &accent) != 0 || ((uint32_t)accent & 0x00FFFFFFu) == 0)
			return false; /* ubistry not ready / nothing usable — keep palette, retry later */
	}

	/* Derived so the window-button fill equals the title-bar accent (a) and the
	 * bar sits a touch darker — matching the compositor's chrome. */
	uint32_t a = (uint32_t)accent & 0x00FFFFFFu;
	TB_BG = scale_color(a, 8, 10);
	TB_SEP = scale_color(a, 5, 10);
	TB_BTN_N = scale_color(a, 10, 10);
	TB_BTN_P = scale_color(a, 13, 10);
	FLY_BG_C = scale_color(a, 9, 10);
	FLY_ITEM_C = scale_color(a, 13, 10);
	return true;
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

/* A colourful per-app tile fill (Win11 icons are varied colours), chosen from a
 * fixed palette by hashing the label — stable per app, lively as a set. */
static uint32_t tile_color(const std::string &label)
{
	static const uint32_t pal[] = {
	    0x000078D4u, 0x0000B294u, 0x006CBF4Bu, 0x00B36AE2u, 0x00F2A33Cu, 0x00E5595Cu, 0x0039C2C9u, 0x00E06CA8u};
	uint32_t h = 2166136261u;
	for (char c : label)
		h = (h ^ (uint8_t)c) * 16777619u;
	return pal[h % (sizeof(pal) / sizeof(pal[0]))];
}

/* First letter of the label, uppercased — the placeholder tile monogram. */
static char app_initial(const std::string &label)
{
	for (char c : label)
		if (c > ' ')
			return (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
	return '?';
}

/**
 * Draw one app's icon into @surf: a colour-coded rounded tile + a simple
 * hand-drawn glyph keyed off @matchkey (case-insensitive substring match on the
 * exec path or window title — terminal, browser, folder, gear, chart, card, …),
 * falling back to a monogram letter from @label for apps we have no glyph for.
 *
 * All glyph geometry is expressed relative to a 46px reference tile and scaled to
 * @sz, so the same drawing serves the 46px Start-menu grid and the ~22px taskbar
 * window buttons.  Glyphs are intentionally minimal/geometric — no bitmap assets.
 *
 * @param matchkey  Match string (exec path or title); matched case-insensitively.
 * @param label     Monogram source when no glyph matches.
 */
static void draw_app_glyph(ogSurface &surf,
                           int ix,
                           int iy,
                           int sz,
                           const std::string &matchkey,
                           const std::string &label,
                           ogScalableFont &font)
{
	int cx = ix + sz / 2, cy = iy + sz / 2;
	auto S = [sz](int n) { return n * sz / 46; }; /* scale 46px reference → sz */
	const uint32_t W = COL_WHITE;

	std::string e = matchkey;
	for (char &c : e)
		if (c >= 'A' && c <= 'Z')
			c += 32;
	auto has = [&](const char *k) { return e.find(k) != std::string::npos; };

	uint32_t tile;
	if (has("term"))
		tile = 0x001C2430u; /* near-black terminal */
	else if (has("nsfb") || has("net"))
		tile = 0x000078D4u; /* browser blue */
	else if (has("doom"))
		tile = 0x00A11E1Eu; /* doom red */
	else if (has("tessera"))
		tile = 0x008A4FD6u; /* purple */
	else if (has("cubitaire"))
		tile = 0x001E8E4Bu; /* felt green */
	else if (has("files"))
		tile = 0x00E0A63Cu; /* folder amber */
	else if (has("disk"))
		tile = 0x006B7280u; /* disk gray */
	else if (has("activity"))
		tile = 0x00159E8Cu; /* teal */
	else if (has("settings"))
		tile = 0x00566072u; /* slate */
	else if (has("about"))
		tile = 0x000078D4u; /* info blue */
	else
		tile = tile_color(label);

	surf.ogFillRoundRect(ix, iy, ix + sz, iy + sz, S(12), tile);

	if (has("term"))
	{ /* prompt chevron + cursor underscore */
		surf.ogLine(cx - S(7), cy - S(7), cx + S(1), cy, W);
		surf.ogLine(cx + S(1), cy, cx - S(7), cy + S(7), W);
		surf.ogFillRect(cx + S(3), cy + S(6), cx + S(11), cy + S(8), W);
	}
	else if (has("nsfb") || has("net"))
	{ /* globe: circle + equator + meridian */
		surf.ogCircle(cx, cy, S(13), W);
		surf.ogHLine(cx - S(13), cx + S(13), cy, W);
		surf.ogVLine(cx, cy - S(13), cy + S(13), W);
	}
	else if (has("doom"))
	{ /* crosshair */
		surf.ogCircle(cx, cy, S(11), W);
		surf.ogHLine(cx - S(15), cx + S(15), cy, W);
		surf.ogVLine(cx, cy - S(15), cy + S(15), W);
	}
	else if (has("tessera"))
	{ /* 2x2 mosaic */
		for (int a = 0; a < 2; a++)
			for (int b = 0; b < 2; b++)
			{
				int qx = cx - S(12) + a * S(13), qy = cy - S(12) + b * S(13);
				surf.ogFillRoundRect(qx, qy, qx + S(10), qy + S(10), S(2), W);
			}
	}
	else if (has("cubitaire"))
	{ /* playing card + red diamond pip */
		surf.ogFillRoundRect(cx - S(9), cy - S(12), cx + S(9), cy + S(12), S(3), W);
		surf.ogFillTriangle(cx, cy - S(6), cx - S(5), cy, cx + S(5), cy, 0x00C41E24u);
		surf.ogFillTriangle(cx - S(5), cy, cx + S(5), cy, cx, cy + S(6), 0x00C41E24u);
	}
	else if (has("files"))
	{ /* folder: tab + body */
		surf.ogFillRect(cx - S(13), cy - S(9), cx - S(2), cy - S(5), W);
		surf.ogFillRoundRect(cx - S(13), cy - S(7), cx + S(13), cy + S(11), S(3), W);
	}
	else if (has("disk"))
	{ /* disk platter */
		surf.ogCircle(cx, cy, S(13), W);
		surf.ogFillCircle(cx, cy, S(3), W);
	}
	else if (has("activity"))
	{ /* bar chart */
		surf.ogFillRect(cx - S(12), cy + S(2), cx - S(6), cy + S(12), W);
		surf.ogFillRect(cx - S(3), cy - S(6), cx + S(3), cy + S(12), W);
		surf.ogFillRect(cx + S(6), cy - S(2), cx + S(12), cy + S(12), W);
	}
	else if (has("settings"))
	{ /* gear: white donut + 4 teeth */
		surf.ogFillCircle(cx, cy, S(12), W);
		surf.ogFillCircle(cx, cy, S(5), tile);
		surf.ogFillRect(cx - S(2), cy - S(16), cx + S(2), cy - S(10), W);
		surf.ogFillRect(cx - S(2), cy + S(10), cx + S(2), cy + S(16), W);
		surf.ogFillRect(cx - S(16), cy - S(2), cx - S(10), cy + S(2), W);
		surf.ogFillRect(cx + S(10), cy - S(2), cx + S(16), cy + S(2), W);
	}
	else if (has("about"))
	{ /* info "i" */
		surf.ogFillCircle(cx, cy - S(8), S(2), W);
		surf.ogFillRect(cx - S(1), cy - S(3), cx + S(2), cy + S(11), W);
	}
	else
	{ /* fallback: monogram letter */
		char mono[2] = {app_initial(label), 0};
		int mw = (int)font.TextWidth(mono);
		font_fg(font, W);
		font_bg(font, tile);
		font.PutString(surf, ix + (sz - mw) / 2, iy + (sz - FONT_SIZE) / 2, mono);
	}
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

/* Fill @time_s ("HH:MM") and @date_s ("MM/DD/YYYY"), local time, DST-aware, for
 * the Windows 11-style stacked taskbar clock.  Buffers need >= 8 and >= 12 bytes. */
static void get_clock_strs(char *time_s, char *date_s)
{
	if (!g_tz_loaded)
		load_timezone();
	time_t t = (time_t)gettime();
	struct tm *lt = localtime(&t);
	if (lt == nullptr)
	{
		time_s[0] = '\0';
		date_s[0] = '\0';
		return;
	}
	snprintf(time_s, 8, "%02d:%02d", lt->tm_hour, lt->tm_min);
	snprintf(date_s, 12, "%02d/%02d/%04d", lt->tm_mon + 1, lt->tm_mday, lt->tm_year + 1900);
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
					 * spaces so callers can pass args ("/usr/bin/settings about"). */
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
	bool grid_ = false;   /* Windows 11-style pinned tile grid (start menu) */
	int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
	int hover_item_ = -1; /* item under the cursor (highlight) */
	int active_row_ = -1; /* row whose submenu is open (stays highlighted) */
	std::vector<MenuItem> items_;

	/* Cached decoded app icons (grid mode): /usr/share/icons/<exec-basename>.png,
	 * loaded once per menu-open so redraws just blit.  Fixed array (not a vector)
	 * to avoid ogSurface copies on reallocation.  has_icon_[i] gates use of icon_[i];
	 * apps without a PNG fall back to the hand-drawn glyph. */
	ogSurface icon_[MENU_MAX_ITEMS];
	bool has_icon_[MENU_MAX_ITEMS] = {false};

	/* ── Windows 11 pinned-grid geometry (shared by draw_grid + sizing) ─────── */
	static int grid_cols()
	{
		return GRID_COLS;
	}
	int grid_rows() const
	{
		return ((int)items_.size() + GRID_COLS - 1) / GRID_COLS;
	}
	static int grid_gy0() /* y of the first tile row */
	{
		return GRID_PAD + GRID_SEARCH_H + 10 + GRID_SECTION_H;
	}
	static int grid_panel_w()
	{
		return GRID_PAD * 2 + GRID_COLS * GRID_TILE_W;
	}
	int grid_panel_h() const
	{
		return grid_gy0() + grid_rows() * GRID_TILE_H + 10 + MENU_FOOTER_H;
	}

	void draw(ogScalableFont &font)
	{
		if (grid_)
		{
			draw_grid(font);
			return;
		}
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

	/* Bottom row: the logged-in user on the left, a power button on the right.
	 * In grid mode the row sits at the panel bottom and gains a monogram avatar. */
	void draw_footer(ogScalableFont &font)
	{
		int ftop = grid_ ? (h_ - MENU_FOOTER_H) : (int)items_.size() * MENU_ITEM_H;
		int fcy = ftop + MENU_FOOTER_H / 2;
		int lx = grid_ ? GRID_PAD : 14;

		surf_.ogFillRect(8, ftop, w_ - 9, ftop, FLY_ITEM_C); /* separator */

		const char *user = getenv("USER");
		const char *uname = (user && user[0]) ? user : "user";

		if (grid_)
		{
			/* Round monogram avatar (matches the login), then the name beside it. */
			int av = 26, ax = lx, ay = fcy - av / 2;
			uint32_t ac = tile_color(uname);
			surf_.ogFillRoundRect(ax, ay, ax + av, ay + av, av / 2, ac);
			char mono[2] = {app_initial(uname), 0};
			int mw = (int)font.TextWidth(mono);
			font_fg(font, COL_WHITE);
			font_bg(font, ac);
			font.PutString(surf_, ax + (av - mw) / 2, ay + (av - FONT_SIZE) / 2, mono);
			lx = ax + av + 10;
		}
		font_fg(font, 0x00C8C8D2u);
		font_bg(font, FLY_BG_C);
		font.PutString(surf_, lx, fcy - FONT_SIZE / 2, uname);

		/* Power glyph: a ring with a vertical line breaking its top. */
		int pcx = w_ - 18, pcy = fcy;
		surf_.ogCircle(pcx, pcy, 7, 0x00FF6E6Eu);
		surf_.ogFillRect(pcx - 1, pcy - 10, pcx + 1, pcy - 4, FLY_BG_C);
		surf_.ogVLine(pcx, pcy - 9, pcy - 1, 0x00FF6E6Eu);
	}

	/* Fit a label to a pixel width by dropping trailing chars (no ellipsis: the
	 * Latin-1 font has no '…').  Returns a copy that renders within @maxw. */
	std::string fit_label(ogScalableFont &font, const std::string &s, int maxw)
	{
		if ((int)font.TextWidth(s.c_str()) <= maxw)
			return s;
		std::string t = s;
		while (t.size() > 1 && (int)font.TextWidth((t + ".").c_str()) > maxw)
			t.pop_back();
		return t + ".";
	}

	/* Draw one grid app's icon: a real per-app PNG (from /usr/share/icons) if we
	 * have one, else the shared hand-drawn glyph (see draw_app_glyph). */
	void draw_app_icon(int ix, int iy, int sz, const MenuItem &it, ogSurface *icon, ogScalableFont &font)
	{
		/* A real per-app icon (PNG from /usr/share/icons) wins: nearest-neighbour
		 * scale it into the tile.  (ogScaleBuf is a stub, so scale by hand.) */
		if (icon != nullptr)
		{
			int iw = (int)icon->ogGetMaxX() + 1, ih = (int)icon->ogGetMaxY() + 1;
			if (iw > 0 && ih > 0)
			{
				for (int dy = 0; dy < sz; dy++)
					for (int dx = 0; dx < sz; dx++)
					{
						uInt8 r, g, b;
						icon->ogUnpack(icon->ogGetPixel(dx * iw / sz, dy * ih / sz), r, g, b);
						surf_.ogSetPixel(ix + dx, iy + dy, r, g, b, 255);
					}
				return;
			}
		}

		draw_app_glyph(surf_, ix, iy, sz, it.exec, it.label, font);
	}

	/* Windows 11-style pinned grid: search pill, "Pinned" label, a grid of
	 * colourful icon tiles with labels, and the user/power footer. */
	void draw_grid(ogScalableFont &font)
	{
		surf_.ogFillRect(0, 0, w_ - 1, h_ - 1, FLY_BG_C);

		/* Search pill (visual): rounded field, magnifier, muted placeholder. */
		int sx = GRID_PAD, sy = GRID_PAD, sw = w_ - 2 * GRID_PAD;
		surf_.ogFillRoundRect(sx, sy, sx + sw, sy + GRID_SEARCH_H, GRID_SEARCH_H / 2, FLY_ITEM_C);
		int mgx = sx + 20, mgy = sy + GRID_SEARCH_H / 2;
		surf_.ogCircle(mgx, mgy, 6, 0x00A6AEBBu);
		surf_.ogLine(mgx + 4, mgy + 4, mgx + 10, mgy + 10, 0x00A6AEBBu);
		font_fg(font, 0x008A93A3u);
		font_bg(font, FLY_ITEM_C);
		font.PutString(surf_, sx + 36, sy + (GRID_SEARCH_H - FONT_SIZE) / 2, "Search");

		/* Section label. */
		int secy = sy + GRID_SEARCH_H + 10;
		font_fg(font, 0x008A93A3u);
		font_bg(font, FLY_BG_C);
		font.PutString(surf_, GRID_PAD, secy, "Pinned");

		/* Tile grid. */
		int gy0 = grid_gy0();
		for (int i = 0; i < (int)items_.size(); i++)
		{
			int col = i % GRID_COLS, row = i / GRID_COLS;
			int cx = GRID_PAD + col * GRID_TILE_W;
			int cy = gy0 + row * GRID_TILE_H;
			bool hl = (i == hover_item_);
			uint32_t cellbg = hl ? FLY_ITEM_C : FLY_BG_C;
			if (hl)
				surf_.ogFillRoundRect(
				    cx + 4, cy + 2, cx + GRID_TILE_W - 4, cy + GRID_TILE_H - 4, 8, FLY_ITEM_C);

			int ix = cx + (GRID_TILE_W - GRID_ICON) / 2, iy = cy + 8;
			draw_app_icon(ix, iy, GRID_ICON, items_[i], has_icon_[i] ? &icon_[i] : nullptr, font);

			std::string lbl = fit_label(font, items_[i].label, GRID_TILE_W - 10);
			int lw = (int)font.TextWidth(lbl.c_str());
			font_fg(font, COL_WHITE);
			font_bg(font, cellbg);
			font.PutString(surf_, cx + (GRID_TILE_W - lw) / 2, iy + GRID_ICON + 7, lbl.c_str());
		}

		draw_footer(font);
	}

	void load_fallback()
	{
		items_.clear();
		items_.push_back({"Terminal", "/usr/bin/term", "", false});
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

	/* Map a pointer position to an item index.  Grid mode uses both axes (tiles);
	 * list mode uses y only (rows). */
	int hit_item(int x, int y) const
	{
		if (!open_)
			return -1;
		if (grid_)
		{
			int gy0 = grid_gy0();
			if (y < gy0 || x < GRID_PAD || x >= GRID_PAD + GRID_COLS * GRID_TILE_W)
				return -1;
			int col = (x - GRID_PAD) / GRID_TILE_W;
			int row = (y - gy0) / GRID_TILE_H;
			if (col < 0 || col >= GRID_COLS || row < 0)
				return -1;
			int i = row * GRID_COLS + col;
			return (i >= 0 && i < (int)items_.size()) ? i : -1;
		}
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

	/* Total panel size.  Grid mode is fixed-width with a computed height; list
	 * mode stacks items (+ footer). */
	int height() const
	{
		if (grid_)
			return grid_panel_h();
		return (int)items_.size() * MENU_ITEM_H + (footer_ ? MENU_FOOTER_H : 0);
	}

	/* True if (x,y) hits the footer's power button. */
	bool footer_power_hit(int x, int y) const
	{
		if (!open_ || !footer_)
			return false;
		int ftop = grid_ ? (h_ - MENU_FOOTER_H) : (int)items_.size() * MENU_ITEM_H;
		return (y >= ftop && y < ftop + MENU_FOOTER_H && x >= w_ - 32);
	}

	void set_grid(bool g)
	{
		grid_ = g;
	}

	/* Populate from a registry container; fall back to a built-in menu if the
	 * registry is unavailable or empty so the desktop is never broken. */
	/* Append each leaf (exec) entry under a registry container to items_ — used to
	 * flatten categories into the grid. */
	void add_leaves(const std::string &container)
	{
		char names[UB_NAMES_MAX];
		if (ubistry_enum(container.c_str(), names, sizeof(names)) <= 0)
			return;
		std::string ns(names);
		size_t start = 0;
		while (start < ns.size() && (int)items_.size() < MENU_MAX_ITEMS)
		{
			size_t nl = ns.find('\n', start);
			std::string child = ns.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
			start = (nl == std::string::npos) ? ns.size() : nl + 1;
			if (child.empty())
				continue;
			std::string cpath = container + "/" + child;
			char buf[128];
			MenuItem it;
			it.path = cpath;
			it.label = (ubistry_get_str((cpath + "/label").c_str(), buf, sizeof(buf)) == 0) ? buf : child;
			if (ubistry_get_str((cpath + "/exec").c_str(), buf, sizeof(buf)) == 0)
				it.exec = buf;
			if (!it.exec.empty())
				items_.push_back(it);
		}
	}

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

			std::string cpath = std::string(regpath) + "/" + child;
			char buf[128];
			std::string label =
			    (ubistry_get_str((cpath + "/label").c_str(), buf, sizeof(buf)) == 0) ? buf : child;

			char scratch[UB_NAMES_MAX];
			bool is_sub = (ubistry_enum((cpath + "/items").c_str(), scratch, sizeof(scratch)) >= 0);

			/* Grid mode is a flat pinned list — pull leaves out of categories and
			 * never keep a submenu entry. */
			if (grid_ && is_sub)
			{
				add_leaves(cpath + "/items");
				continue;
			}

			MenuItem it;
			it.path = cpath;
			it.label = label;
			it.submenu = (!grid_) && is_sub;
			if (!it.submenu && ubistry_get_str((cpath + "/exec").c_str(), buf, sizeof(buf)) == 0)
				it.exec = buf;
			if (grid_ && it.exec.empty()) /* skip a categoryless/exec-less entry in the grid */
				continue;
			items_.push_back(it);
		}
		if (items_.empty())
			load_fallback();
		if (grid_)
			load_icons();
	}

	/* Decode each grid app's /usr/share/icons/<exec-basename>.png once, so redraws
	 * only blit.  Absent PNGs leave has_icon_[i] false → the hand-drawn glyph shows.
	 * (The self-contained ELF-embedded alternative is docs/design/app-icon-embedding-plan.md.) */
	void load_icons()
	{
		ogImage img;
		for (int i = 0; i < (int)items_.size() && i < MENU_MAX_ITEMS; i++)
		{
			has_icon_[i] = false;
			std::string exe = items_[i].exec;
			if (exe.empty() || exe[0] == '@')
				continue;
			size_t sp = exe.find(' '); /* strip any arguments */
			if (sp != std::string::npos)
				exe = exe.substr(0, sp);
			size_t sl = exe.rfind('/');
			std::string base = (sl == std::string::npos) ? exe : exe.substr(sl + 1);
			std::string path = "/usr/share/icons/" + base + ".png";
			if (img.Load(path.c_str(), icon_[i]))
				has_icon_[i] = true;
		}
	}

	void show(int x, int y, ubix::Mailbox &mbox, ogScalableFont &font)
	{
		if (open_ || items_.empty())
			return;

		w_ = grid_ ? grid_panel_w() : MENU_W;
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

		/* The claim ACK shares the taskbar's one mailbox with input traffic, so
		 * motion/flip events queued while views services the claim can arrive
		 * first.  Reading only the next message treated a queued mouse event as
		 * a denial — abandoning the window views HAD created: it leaked, stayed
		 * black (never drawn), and stacked one translucent panel per Start
		 * click, grinding the compositor down.  Fetch until the ACK arrives
		 * (bounded); a hover event discarded during menu-open is harmless. */
		mpi_message_t reply;
		bool acked = false;
		for (int i = 0; i < 200000; i++)
		{
			if (mbox.try_fetch(reply))
			{
				if (reply.header == DISPLAY_ACK)
				{
					acked = true;
					break;
				}
				continue; /* queued input event — discard during the claim */
			}
			ubix::yield();
		}
		if (!acked)
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
	int active_col_ = -1; /* column under a held button (0 = master, 1.. = app) */
	int hover_col_ = -1;  /* column under the cursor (knob highlight)            */

	int master_vol_ = 100;
	bool master_mute_ = false;
	struct aural_list_reply data_ = {}; /* live streams (from AURAL_LIST) */

	/* Master column plus one per live stream. */
	int columns() const
	{
		return 1 + (int)data_.count;
	}

	/* Display level 0..100 for column @p c (0 = master). */
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
		return (c == 0) ? "Master" : data_.streams[c - 1].name;
	}

	/* Column under x (flyout-relative), or -1 outside any slider column. */
	int col_at(int x) const
	{
		if (x < FLY_PAD)
			return -1;
		int c = (x - FLY_PAD) / MIX_COL_W;
		return (c >= 0 && c < columns()) ? c : -1;
	}

	/* Map a flyout-relative y onto a 0..100 slider level (clamped to track). */
	static int level_at(int y)
	{
		int top = MIX_TRACK_TOP, bot = MIX_TRACK_TOP + MIX_TRACK_H;
		if (y <= top)
			return 100;
		if (y >= bot)
			return 0;
		return (bot - y) * 100 / MIX_TRACK_H;
	}

	void draw_slider(ogScalableFont &font, int c)
	{
		int x0 = FLY_PAD + c * MIX_COL_W;
		int cx = x0 + MIX_COL_W / 2;
		int top = MIX_TRACK_TOP, bot = MIX_TRACK_TOP + MIX_TRACK_H;
		int lvl = col_level(c);
		int kn_y = bot - lvl * MIX_TRACK_H / 100;
		bool master = (c == 0);
		bool lit = (c == active_col_ || c == hover_col_);
		int hw = MIX_TRACK_W / 2;

		uint32_t panel = FLY_BG_C;
		uint32_t groove = ogSurface::ogBlendColor(panel, 0x00000000u, 96);
		/* Filled portion: master in a brighter accent, apps in the calmer one. */
		uint32_t fill = master ? ogSurface::ogBlendColor(FLY_ITEM_C, COL_WHITE, 80) : FLY_ITEM_C;
		if (lit)
			fill = ogSurface::ogBlendColor(fill, COL_WHITE, 48);

		/* Rounded groove, then the filled portion from the knob to the bottom. */
		surf_.ogFillRoundRect(cx - hw, top, cx + hw, bot, hw, groove);
		if (kn_y < bot - 1)
			surf_.ogFillRoundRect(cx - hw, kn_y, cx + hw, bot, hw, fill);

		/* Knob: a white disc with a thin ring and an accent pip; grows when lit. */
		int r = MIX_KNOB_R + (lit ? 1 : 0);
		surf_.ogFillCircle(cx, kn_y, r, COL_WHITE);
		surf_.ogCircle(cx, kn_y, r, ogSurface::ogBlendColor(panel, 0x00000000u, 70));
		surf_.ogFillCircle(cx, kn_y, r - 4, fill);

		/* Name below the track: trimmed to the column width, then centred under it. */
		char nm[24];
		fit_label(font, nm, sizeof(nm), col_name(c), MIX_COL_W - 6);
		int nx = x0 + (MIX_COL_W - (int)font.TextWidth(nm)) / 2;
		font_fg(font, master ? ogSurface::ogBlendColor(panel, COL_WHITE, 230) : 0x00AEB6C2u);
		font_bg(font, panel);
		font.PutString(surf_, nx, bot + MIX_LABEL_GAP, nm);

		/* Persistent level under the name (centred, quiet secondary tone). */
		char pct[8];
		std::snprintf(pct, sizeof(pct), "%d%%", lvl);
		int px = x0 + (MIX_COL_W - (int)font.TextWidth(pct)) / 2;
		font_fg(font, ogSurface::ogBlendColor(panel, COL_WHITE, 140));
		font.PutString(surf_, px, bot + MIX_LABEL_GAP + FONT_SIZE + 3, pct);
	}

	void draw(ogScalableFont &font)
	{
		/* Card: a 1px darker frame around a rounded inner fill reads as a floating
		 * panel (the compositor already lays a soft drop shadow around the window;
		 * the window itself is opaque + rectangular, so true rounded outer corners
		 * are not possible — this fakes the card edge instead). */
		uint32_t panel = FLY_BG_C;
		uint32_t edge = ogSurface::ogBlendColor(panel, 0x00000000u, 80);
		surf_.ogFillRect(0, 0, w_ - 1, h_ - 1, edge);
		surf_.ogFillRoundRect(1, 1, w_ - 2, h_ - 2, 10, panel);

		/* Header: a quiet title with a hairline rule under it.  Per-slider values
		 * live persistently under each knob, so the header never reflows. */
		int hy = (MIX_HEADER_H - FONT_SIZE) / 2 + 8;
		font_fg(font, ogSurface::ogBlendColor(panel, COL_WHITE, 215));
		font_bg(font, panel);
		font.PutString(surf_, FLY_PAD, hy, "Volume");
		surf_.ogHLine(
		    FLY_PAD, w_ - 1 - FLY_PAD, MIX_HEADER_H + 8, ogSurface::ogBlendColor(panel, COL_WHITE, 30));

		/* A faint divider sets the master column apart from the app columns. */
		if (columns() > 1)
		{
			int sx = FLY_PAD + MIX_COL_W;
			surf_.ogVLine(sx,
			              MIX_TRACK_TOP - 10,
			              MIX_TRACK_TOP + MIX_TRACK_H + 10,
			              ogSurface::ogBlendColor(panel, COL_WHITE, 24));
		}

		for (int c = 0; c < columns(); c++)
			draw_slider(font, c);
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

		w_ = FLY_PAD * 2 + columns() * MIX_COL_W;
		/* track + name line + value line + bottom padding */
		h_ = MIX_TRACK_TOP + MIX_TRACK_H + MIX_LABEL_GAP + 2 * FONT_SIZE + 14;
		x_ = anchor_right - w_;
		if (x_ < 0)
			x_ = 0;
		y_ = screen_h - TB_H - h_;
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
		std::strncpy(creq->title, "mixer", sizeof(creq->title) - 1);
		creq->title[sizeof(creq->title) - 1] = '\0';
		std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
		creq->reply[sizeof(creq->reply) - 1] = '\0';
		ubix::post_message("views", DISPLAY_CLAIM, claim);

		/* Same shared-mailbox hazard as Menu::show — fetch until the claim ACK
		 * arrives (bounded) instead of misreading a queued input event as a
		 * denial and leaking the created window. */
		mpi_message_t reply;
		bool acked = false;
		for (int i = 0; i < 200000; i++)
		{
			if (mbox.try_fetch(reply))
			{
				if (reply.header == DISPLAY_ACK)
				{
					acked = true;
					break;
				}
				continue; /* queued input event — discard during the claim */
			}
			ubix::yield();
		}
		if (!acked)
			return;

		struct display_ack *da = (struct display_ack *)reply.data;
		win_id_ = da->window_id;
		surf_.ogAttach(da->shm_base, (uint32_t)w_, (uint32_t)h_, OG_PIXFMT_32BPP);
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
			int hc = (me->x < 0) ? -1 : col_at(me->x);
			if (hc != hover_col_)
			{
				hover_col_ = hc;
				draw(font);
				send_flip_msg(win_id_);
			}
			return;
		}

		if (active_col_ < 0)
			active_col_ = col_at(me->x); /* lock the column on press */
		if (active_col_ < 0)
			return;

		hover_col_ = active_col_;
		apply_level(active_col_, level_at(me->y));
		draw(font);
		send_flip_msg(win_id_);
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
	ogScalableFont clockfont_;    /* small face for the stacked time/date */
	bool have_clockfont_ = false; /* clockfont_ loaded (else fall back to font_) */
	uint32_t win_id_ = 0;
	uint32_t sw_ = 0;
	uint32_t sh_ = 0;
	std::vector<TrackedWin> tracked_;
	Menu start_menu_;
	Menu submenu_;
	MixerFlyout mixer_;
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
			/* Icon-only tab: the app's glyph tile, keyed off the window title
			 * (case-insensitive), centred in the button — matching the Start grid. */
			int gx = wx + (WIN_BTN_W - WIN_BTN_ICON) / 2, gy = (TB_H - WIN_BTN_ICON) / 2 + 1;
			draw_app_glyph(surf_, gx, gy, WIN_BTN_ICON, tw.title, tw.title, font_);
			wx += WIN_BTN_W + 2;
			i++;
		}

		/* System tray: volume glyph. */
		draw_volume_glyph(tray_x + TRAY_W / 2, TB_H / 2);

		/* Clock — Windows 11 style: time stacked over the date, right-aligned,
		 * both lines centred vertically in the bar. */
		char tstr[8], dstr[12];
		get_clock_strs(tstr, dstr);
		ogScalableFont &cf = have_clockfont_ ? clockfont_ : font_;
		int cfh = (int)cf.GetHeight();
		int right = (int)sw - 12; /* right margin */
		int gap = 2;
		int ttop = (TB_H - (2 * cfh + gap)) / 2;
		font_fg(cf, COL_WHITE);
		font_bg(cf, TB_BG);
		int tw = (int)cf.TextWidth(tstr);
		cf.PutString(surf_, right - tw, ttop, tstr);
		font_fg(cf, 0x00C8C8D2u); /* date slightly muted */
		int dw = (int)cf.TextWidth(dstr);
		cf.PutString(surf_, right - dw, ttop + cfh + gap, dstr);
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

	/* Screen-x centre of window button @idx, or -1 if it isn't laid out (matches
	 * winbtn_hit's layout exactly). */
	int winbtn_center_x(int idx) const
	{
		int wx = 2 + BTN_W + 4;
		int tray_x = (int)sw_ - CLOCK_W - 2 - TRAY_W;
		for (int i = 0; i < (int)tracked_.size(); i++)
		{
			if (wx + WIN_BTN_W > tray_x - 4)
				break;
			if (i == idx)
				return wx + WIN_BTN_W / 2;
			wx += WIN_BTN_W + 2;
		}
		return -1;
	}

	/* Ask the compositor to show (id != 0) or hide (id == 0) a hover thumbnail of
	 * the given window, centred on @anchor_x.  The compositor owns the pixels. */
	void send_preview(uint32_t id, int anchor_x)
	{
		mpi_message_t msg = {};
		struct display_preview *dp = (struct display_preview *)msg.data;
		msg.header = DISPLAY_PREVIEW;
		dp->window_id = id;
		dp->anchor_x = anchor_x;
		ubix::post_message("views", DISPLAY_PREVIEW, msg);
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
		creq->wants_motion = 1;     /* receive hover motion for highlighting */
		creq->opacity = TB_OPACITY; /* translucent strip (compositor blends it) */
		creq->blur_radius = TB_BLUR;
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
		have_clockfont_ = clockfont_.Load(font_path, CLOCK_SIZE);

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
		creq->wants_motion = 1;     /* receive hover motion for highlighting */
		creq->opacity = TB_OPACITY; /* translucent strip (compositor blends it) */
		creq->blur_radius = TB_BLUR;
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
		mixer_.hide();
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
				launcher_.launch("/usr/bin/settings about"); /* open Settings on About */
			return;
		}
		launcher_.launch(it.exec.c_str());
	}

	/* Open the submenu for a top-level entry beside its row, on-screen. */
	void open_submenu(const MenuItem &parent, int row, ubix::Mailbox &mbox)
	{
		submenu_.hide();
		submenu_.load((parent.path + "/items").c_str());

		/* Gap so the submenu clears the compositor's ~10px window drop shadow:
		 * opening flush let the submenu's left shadow darken the parent menu's
		 * right edge — the reported "submenu overlaps the main menu". */
		const int GAP = 10; /* == draw_window_shadow() reach in compositor.cc */
		int sx = start_menu_.x() + start_menu_.w() + GAP;
		int sy = start_menu_.y() + row * MENU_ITEM_H;
		int sh_px = submenu_.count() * MENU_ITEM_H;
		/* No room on the right?  Open to the LEFT of the parent — but only with
		 * real room there; never clamp into a spot that overlaps the parent (the
		 * old `x - MENU_W` could go negative and get pinned onto the menu). */
		if (sx + MENU_W > (int)sw_)
		{
			int left = start_menu_.x() - MENU_W - GAP;
			sx = (left >= 0) ? left : (int)sw_ - MENU_W;
		}
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
				if (ht != hover_tab_)
				{
					if (ht >= 0)
						send_preview(tracked_[ht].id, winbtn_center_x(ht));
					else
						send_preview(0, 0);
				}
				hover_start_ = hs;
				hover_tab_ = ht;
				hover_vol_ = hv;
				draw_strip();
				send_flip();
			}
		}
		else if (me->window_id == start_menu_.win_id() && start_menu_.is_open())
		{
			if (start_menu_.set_hover(exited ? -1 : start_menu_.hit_item(me->x, me->y)))
				start_menu_.redraw(font_);
		}
		else if (me->window_id == submenu_.win_id() && submenu_.is_open())
		{
			if (submenu_.set_hover(exited ? -1 : submenu_.hit_item(me->x, me->y)))
				submenu_.redraw(font_);
		}
	}

	void on_mouse(const display_mouse_ev *me, ubix::Mailbox &mbox)
	{
		bool down = (me->buttons & 1) != 0;
		bool click = !down && prev_down_; /* release edge = a completed click */
		prev_down_ = down;

		update_hover(me);

		/* Mixer flyout: drag a slider (press + motion); clicks stay inside it. */
		if (me->window_id == mixer_.win_id() && mixer_.is_open())
		{
			mixer_.on_event(me, font_);
			return;
		}

		/* Submenu click: dispatch the chosen leaf. */
		if (me->window_id == submenu_.win_id() && submenu_.is_open())
		{
			if (click)
			{
				const MenuItem *it = submenu_.item(submenu_.hit_item(me->x, me->y));
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
				int row = start_menu_.hit_item(me->x, me->y);
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
			send_preview(0, 0); /* dismiss the hover thumbnail on activation */
			hover_tab_ = -1;
			raise_window(tracked_[wi].id);
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
				int tray_right = (int)sw_ - CLOCK_W - 2;
				mixer_.set_master(volume_, muted_);
				mixer_.show(tray_right, (int)sh_, mbox, font_);
			}
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
				start_menu_.set_grid(true); /* Windows 11-style pinned tile grid */
				start_menu_.load("/views/startmenu");
				start_menu_.set_footer(true); /* user + power row */
				/* Anchored bottom-left, a small gap above the taskbar and off the edge. */
				start_menu_.show(8, (int)sh_ - TB_H - start_menu_.height() - 8, mbox, font_);
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

	/* Derive the palette from the user's accent before first paint.  This can fail if
	 * ubistry isn't serving yet at taskbar startup (an ordering race that SMP timing
	 * makes likely — the symptom is a gray bar instead of the theme accent); the event
	 * loop below retries until it succeeds. */
	bool theme_loaded = apply_theme();

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
			/* Theme not loaded yet (ubistry wasn't ready at startup)?  Keep trying
			 * until it answers, then repaint with the real accent. */
			if (!theme_loaded && apply_theme())
				theme_loaded = true;
			tb.refresh_volume(); /* pick up volume/mute changes once a second */
			tb.draw();
			tb.send_flip();
		}

		/* Block on the mailbox instead of busy-yielding: wake instantly on a client
		 * message (input/focus/notify/theme/resize), or after ~0.2 s (20 ticks) to
		 * repaint the live seconds clock.  Drops an idle taskbar from ~82% CPU to
		 * ~1% — the residual is the unavoidable cost of a ticking seconds display. */
		mpi_message_t reply;
		if (!mbox.wait(reply, 20))
			continue;
		do
		{
			if (reply.header == DISPLAY_KEY)
				continue;

			if (reply.header == DISPLAY_THEME)
			{
				if (apply_theme())
					theme_loaded = true;
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
		} while (mbox.try_fetch(reply));
	}

	return 0;
}
