/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * vLogin — graphical login screen for UbixOS.
 *
 * Claims a full-screen, decoration-free window from the views compositor,
 * draws a simple login UI using objGFX, authenticates via the authd MPI
 * daemon, then forks taskbar on success.  When the taskbar exits (session
 * ends) vLogin redraws the login screen and loops for the next user.
 *
 * Launch order: init → views → vlogin (via etc/init.d/25-vlogin).
 * vLogin polls for the compositor to become ready before claiming its window.
 */

#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <ctime>

#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>

#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogImage.h>
#include <objgfx/ogPixelFmt.h>
#include <ubistry/ubistry.h>
#include <authd.h>
#include <views/theme.h>
#include <views/uidraw.h>
#include <api/power.h>
#include <ctime>
#include <vector>

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */

#define FONT_PATH "/var/fonts/DejaVuSans.ttf"
#define FONT_BOLD_PATH "/var/fonts/DejaVuSans-Bold.ttf"
#define VIEWS_MBOX "views"
#define TASKBAR_PATH "/usr/bin/taskbar"
#define MAX_FIELD 31 /* max username / password length */

/* Lock-screen layout (ubixos-v2 design canvas): big clock top-left, the
 * sign-in card anchored bottom-left, a status footer.  Proportions follow the
 * design's clamp()s so the screen also works at 800x480. */
#define LG_CARD_W 380
#define LG_PAD 32    /* card padding and gap between its sections */
#define LG_AVATAR 64 /* avatar ring diameter */
#define LG_PILL_H 44 /* password / username pill */
#define LG_SEG_H 38  /* Desktop / Console segmented control */
#define LAST_USER_KEY "/views/login/last_user"

extern "C" int pidStatus(int pid);
extern "C" int gettime(); /* seconds since epoch (libubix_api) */
/* musl gates these POSIX prototypes behind a feature macro the -nostdinc world
 * build does not set; the symbols are in libc, so declare them directly. */
extern "C" int setenv(const char *name, const char *value, int overwrite);
extern "C" void tzset(void);

/* musl gates these behind a feature macro the -nostdinc world does not set. */
extern "C" int setenv(const char *name, const char *value, int overwrite);
extern "C" void tzset(void);
extern "C" int nanosleep(const struct timespec *req, struct timespec *rem);

/* What the login screen shows; owned by main()'s event loop. */
struct LoginState
{
	std::string last_user; /* remembered user ("" = none: always "other" mode) */
	bool other = false;    /* typing a username instead of the remembered user */
	std::string user, pass;
	bool in_pass = true;     /* focus: password pill (else the username pill) */
	std::string err;         /* error line (red); empty = hint */
	bool busy = false;       /* authenticating */
	bool power_open = false; /* footer power pop-over (Restart / Shut down) */

	/* The account being signed in to. */
	const std::string &account() const
	{
		return other ? user : last_user;
	}
};

/* Clickable regions from the last draw (screen coordinates). */
struct LoginHits
{
	int sign_x = 0, sign_y = 0, sign_d = 0; /* round sign-in button */
	int user_x = 0, user_y = 0, user_w = 0; /* username pill (other mode) */
	int pass_x = 0, pass_y = 0, pass_w = 0; /* password pill */
	int link_x = 0, link_y = 0, link_w = 0; /* "Other user..." link */
	int pwr_x = 0, pwr_y = 0;               /* footer power button (32x32) */
	int pop_x = 0, pop_y = 0, pop_w = 0;    /* power pop-over; rows of 34 at pop_y + 4 + i*35 */

	static bool in(int px, int py, int x, int y, int w, int h)
	{
		return px >= x && px < x + w && py >= y && py < y + h;
	}
};

/* ------------------------------------------------------------------ */
/* LoginUI — draws the lock screen onto an ogSurface                   */
/* ------------------------------------------------------------------ */

class LoginUI
{
	ogSurface &surf_;
	ui_canvas cv_;
	ogScalableFont font_;       /* 12px: greeting, hint, links, footer */
	ogScalableFont font_bold_;  /* 12px bold: footer brand */
	ogScalableFont font_field_; /* 14px: typed username */
	ogScalableFont font_name_;  /* 22px: user name */
	ogScalableFont font_init_;  /* 24px: avatar initial */
	ogScalableFont font_date_;  /* 18px: date under the clock */
	ogScalableFont font_clock_; /* ~104px: clock */
	int sw_, sh_;
	struct ui_theme t_;

	std::vector<uint32_t> bg_; /* wallpaper stretched to the screen + scrim */
	bool have_bg_ = false;

	int left_, clock_y_;   /* left column x, clock top */
	bool compact_ = false; /* small screen (e.g. 800x480): centred card, 32px clock */
	int pad_ = LG_PAD, av_ = LG_AVATAR;

      public:
	bool loaded = false;
	LoginHits hits;

	explicit LoginUI(ogSurface &s, int sw, int sh) : surf_(s), sw_(sw), sh_(sh)
	{
		t_ = ui_theme_load(nullptr); /* system default: nobody is logged in yet */

		/* clamp(32px, 8vw, 128px) / clamp(24px, 10vh, 120px) */
		left_ = clampi(sw_ * 8 / 100, 32, 128);
		clock_y_ = clampi(sh_ * 10 / 100, 24, 120);
		int clock_css = clampi(std::min(sw_ * 9 / 100, sh_ * 13 / 100), 48, 112);
		compact_ = sw_ < 1000 || sh_ < 600;
		if (compact_)
		{
			/* design "Smaller screens": card centred at 340px, clock 32px */
			clock_css = 32;
			clock_y_ = 16;
			left_ = 32;
			pad_ = 20;
			av_ = 48;
		}

		loaded = font_.Load(FONT_PATH, 14);
		if (!font_bold_.Load(FONT_BOLD_PATH, 14))
			font_bold_.Load(FONT_PATH, 14);
		font_field_.Load(FONT_PATH, 16);
		font_name_.Load(FONT_PATH, 26);
		font_init_.Load(FONT_PATH, 28);
		font_date_.Load(FONT_PATH, 21);
		font_clock_.Load(FONT_PATH, clock_css * 1164 / 1000); /* CSS px -> line height */

		cv_.px = (uint32_t *)surf_.ogGetPtr(0, 0);
		cv_.w = sw_;
		cv_.h = sh_;
		cv_.stride = (int)((uint8_t *)surf_.ogGetPtr(0, 1) - (uint8_t *)surf_.ogGetPtr(0, 0)) / 4;

		load_timezone();
		load_background();
	}

	static int clampi(int v, int lo, int hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	/* Apply /system/timezone ($TZ) so localtime() matches the taskbar clock. */
	static void load_timezone()
	{
		char tz[64];
		if (ubistry_get_str("/system/timezone", tz, sizeof(tz)) != 0)
		{
			std::strncpy(tz, "EST5EDT,M3.2.0,M11.1.0", sizeof(tz) - 1);
			tz[sizeof(tz) - 1] = '\0';
		}
		setenv("TZ", tz, 1);
		tzset();
	}

	/* Minutes since the epoch (the clock face changes when this does). */
	static long minute_now()
	{
		return (long)gettime() / 60;
	}

	/* Decode the system-default wallpaper, stretch it to the screen and bake in
	 * the lock scrim: a horizontal gradient, heavy behind the left column
	 * (0.90 -> 0.62 at 42% -> 0.12 at the right edge) so the wallpaper still
	 * shows on the right.  Missing image falls back to a solid ground. */
	void load_background()
	{
		char mode[32] = {0};
		char path[256] = {0};
		if (ubistry_get_str("views/desktop/mode", mode, sizeof(mode)) == 0 && std::strcmp(mode, "image") != 0)
			return;
		if (ubistry_get_str("views/desktop/image", path, sizeof(path)) != 0 || path[0] == '\0')
			std::strncpy(path, "/var/background/ubix-nocturne.png", sizeof(path) - 1);

		ogImage img;
		ogSurface src;
		if (!img.Load(path, src))
			return;
		int iw = (int)src.ogGetMaxX() + 1;
		int ih = (int)src.ogGetMaxY() + 1;
		if (iw <= 0 || ih <= 0 || sw_ <= 0 || sh_ <= 0)
			return;

		bool light = t_.mode == UI_THEME_LIGHT;
		uint32_t scrim = light ? t_.ground : t_.scrim;
		int a0 = light ? 235 : 230, a1 = light ? 179 : 159, a2 = light ? 26 : 31; /* /256 */
		std::vector<uint32_t> col_a((size_t)sw_);
		int knee = sw_ * 42 / 100;
		for (int x = 0; x < sw_; x++)
			col_a[x] = (uint32_t)(x < knee ? a0 + (a1 - a0) * x / (knee ? knee : 1)
			                               : a1 + (a2 - a1) * (x - knee) / (sw_ - knee ? sw_ - knee : 1));

		bg_.resize((size_t)sw_ * (size_t)sh_);
		for (int dy = 0; dy < sh_; dy++)
		{
			int sy = dy * ih / sh_;
			for (int dx = 0; dx < sw_; dx++)
			{
				int sx = dx * iw / sw_;
				uInt8 r, g, b;
				src.ogUnpack(src.ogGetPixel(sx, sy), r, g, b);
				uint32_t c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
				bg_[(size_t)dy * sw_ + dx] = ui_mix(c, scrim, col_a[dx]);
			}
		}
		have_bg_ = true;
	}

	/* Screen rect changed by the last draw() (for a partial DISPLAY_FLIP). */
	int dirty_x = 0, dirty_y = 0, dirty_w = 0, dirty_h = 0;

	/**
	 * Draw the lock screen for st.  The static layer (wallpaper + scrim, clock,
	 * card panel and shadow, hint, session control, links, footer) is cached
	 * and rebuilt only when one of its inputs changes; a keystroke repaints just
	 * the avatar and input pills from the cache, keeping typing responsive.
	 * Sets dirty_* to the area that changed.
	 */
	void draw(const LoginState &st)
	{
		/* Column bottom: 72px footer band + clamp(0, 6vh, 72) breathing room. */
		int col_bottom = sh_ - 72 - clampi(sh_ * 6 / 100, 0, 72);
		bool show_link = !st.last_user.empty();
		int links_h = show_link ? 16 : 0;
		int ch =
		    pad_ + av_ + pad_ + (st.other ? LG_PILL_H + 12 : 0) + LG_PILL_H + 12 + 16 + pad_ + LG_SEG_H + pad_;
		int cw = std::min(compact_ ? 340 : LG_CARD_W, sw_ - 64);
		int cx = left_, cy = col_bottom - links_h - (show_link ? 24 : 0) - ch;
		if (compact_)
		{
			/* centred, below the small clock, above the footer */
			int top = clock_y_ + (int)font_clock_.GetHeight() + 8 + 21 + 12;
			cx = (sw_ - cw) / 2;
			cy = top + (sh_ - 56 - links_h - (show_link ? 12 : 0) - top - ch) / 2;
			if (cy < top)
				cy = top;
		}

		std::string key = std::to_string((int)st.other) + "|" + st.last_user + "|" +
		                  std::to_string(minute_now()) + "|" + st.err + "|" + std::to_string((int)st.busy) +
		                  "|" + std::to_string((int)st.power_open);
		if (key != base_key_ || base_.empty())
		{
			draw_static(st, cx, cy, cw, ch, col_bottom, show_link);
			base_.resize((size_t)sw_ * (size_t)sh_);
			for (int y = 0; y < sh_; y++)
				std::memcpy(&base_[(size_t)y * sw_], cv_.px + (size_t)y * cv_.stride, (size_t)sw_ * 4);
			base_key_ = key;
			dirty_x = dirty_y = 0;
			dirty_w = sw_;
			dirty_h = sh_;
		}
		else
		{
			/* Restore just the avatar + pills band (with room for the avatar
			 * glow and the focus rings) from the cached layer. */
			int y0 = cy + pad_ - 16;
			int y1 = cy + pad_ + av_ + pad_ + (st.other ? LG_PILL_H + 12 : 0) + LG_PILL_H + 6;
			for (int y = y0; y < y1; y++)
				std::memcpy(
				    cv_.px + (size_t)y * cv_.stride + cx, &base_[(size_t)y * sw_ + cx], (size_t)cw * 4);
			dirty_x = cx;
			dirty_y = y0;
			dirty_w = cw;
			dirty_h = y1 - y0;
		}
		draw_card_dynamic(st, cx, cy, cw);
	}

      private:
	std::vector<uint32_t> base_; /* cached static layer (screen-sized) */
	std::string base_key_;       /* inputs base_ was drawn for */

	/* Render the static layer into the surface. */
	void draw_static(const LoginState &st, int cx, int cy, int cw, int ch, int col_bottom, bool show_link)
	{
		if (have_bg_)
			for (int y = 0; y < sh_; y++)
				std::memcpy(cv_.px + (size_t)y * cv_.stride,
				            &bg_[(size_t)y * sw_],
				            (size_t)sw_ * sizeof(uint32_t));
		else
			ui_fill(cv_, 0, 0, sw_, sh_, t_.console);

		draw_clock();
		draw_card_static(st, cx, cy, cw, ch);

		/* Links under the card. */
		if (show_link)
		{
			std::string l = st.other ? "Back to " + st.last_user : std::string("Other user...");
			hits.link_x = cx + 16;
			hits.link_y = cy + ch + (compact_ ? 8 : 24); /* under the card */
			hits.link_w = (int)font_.TextWidth(l.c_str());
			text(font_,
			     hits.link_x,
			     hits.link_y + 8,
			     l.c_str(),
			     t_.mode == UI_THEME_LIGHT ? 0x595D6Cu : 0xCFD3E5u);
		}
		else
			hits.link_w = 0;

		draw_footer(st);
	}

      private:
	/* Draw text vertically centred on row cy, anti-aliased over the surface. */
	void text(ogScalableFont &f, int x, int cy, const char *str, uint32_t fg)
	{
		f.SetFGColor((fg >> 16) & 0xFF, (fg >> 8) & 0xFF, fg & 0xFF, 255);
		f.SetBGColor(0, 0, 0, 0);
		f.PutString(surf_, x, cy - (int)f.GetHeight() / 2 + 1, str);
	}

	/* Huge clock + long date, top-left (local time). */
	void draw_clock()
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
		time_t now = (time_t)gettime();
		struct tm *lt = localtime(&now);
		if (lt == nullptr)
			return;
		char tm_s[16], date[48];
		ui_format_time(tm_s, sizeof(tm_s), lt, ui_clock_24h(nullptr));
		std::snprintf(date,
		              sizeof(date),
		              "%s, %s %d, %d",
		              wd[lt->tm_wday % 7],
		              mo[lt->tm_mon % 12],
		              lt->tm_mday,
		              lt->tm_year + 1900);
		int th = (int)font_clock_.GetHeight();
		text(font_clock_, left_ - th / 20, clock_y_ + th / 2, tm_s, t_.text);
		text(
		    font_date_, left_, clock_y_ + th + 8 + 10, date, t_.mode == UI_THEME_LIGHT ? 0x595D6Cu : 0xCFD3E5u);
	}

	static const char *greeting()
	{
		time_t now = (time_t)gettime();
		struct tm *lt = localtime(&now);
		int h = lt ? lt->tm_hour : 12;
		return h < 5 ? "Good night" : h < 12 ? "Good morning" : h < 18 ? "Good afternoon" : "Good evening";
	}

	/* A 44px pill input: ground fill, 1px edge (accent focused / red on error),
	 * focus ring, leading 16px icon, contents drawn by the caller. */
	void pill(int x, int y, int w, bool focused, bool error, const char *icon)
	{
		uint32_t edge = error ? t_.danger : (focused ? t_.accent : t_.edge);
		ui_outlined_round(cv_, x, y, w, LG_PILL_H, LG_PILL_H / 2, edge, t_.ground);
		if (focused)
			ui_focus_ring(cv_, x, y, w, LG_PILL_H, LG_PILL_H / 2, t_.accent);
		ui_draw_icon_tint(cv_, ui_icon_get(16, icon), x + 14, y + (LG_PILL_H - 16) / 2, t_.text_faint);
	}

	/* The per-keystroke layer: avatar, name and the input pills. */
	void draw_card_dynamic(const LoginState &st, int x, int y, int w)
	{
		int ix = x + pad_, iw = w - 2 * pad_;
		int yy = y + pad_;

		/* Avatar: glow, 1px accent ring, 3px gap, gradient disc, initial. */
		const std::string &acct = st.account();
		int ax = ix, ay = yy;
		ui_shadow(cv_, ax, ay, av_, av_, av_ / 2, 0, 14, 150, t_.accent_700);
		ui_outlined_round(cv_, ax, ay, av_, av_, av_ / 2, t_.accent, t_.ground);
		int d = av_ - 8;
		for (int r = 0; r < d; r++) /* vertical accent-800 -> accent-900 gradient disc */
			for (int c = 0; c < d; c++)
			{
				uint32_t a = ui_cov(ui_rr_dist4(ax + 4 + c, ay + 4 + r, ax + 4, ay + 4, d, d, d / 2));
				if (a)
					cv_.blend(ax + 4 + c,
					          ay + 4 + r,
					          ui_mix(t_.accent_800, t_.accent_900, (uint32_t)(r * 256 / d)),
					          a);
			}
		if (acct.empty())
			ui_draw_icon_tint(
			    cv_, ui_icon_get(24, "user"), ax + (av_ - 24) / 2, ay + (av_ - 24) / 2, t_.accent_200);
		else
		{
			char ini[2] = {(char)((acct[0] >= 'a' && acct[0] <= 'z') ? acct[0] - 32 : acct[0]), 0};
			text(font_init_,
			     ax + (av_ - (int)font_init_.TextWidth(ini)) / 2,
			     ay + av_ / 2,
			     ini,
			     t_.accent_200);
		}
		int tx = ax + av_ + 24;
		text(font_, tx, ay + av_ / 2 - 13, greeting(), t_.text_muted);
		text(font_name_, tx, ay + av_ / 2 + 9, acct.empty() ? "Who's there?" : acct.c_str(), t_.text);
		yy += av_ + pad_;

		/* Username pill (other user). */
		if (st.other)
		{
			bool f = !st.in_pass && !st.busy;
			pill(ix, yy, iw, f, false, "user");
			int mid = yy + LG_PILL_H / 2;
			if (st.user.empty() && !f)
				text(font_field_, ix + 40, mid, "Username", t_.text_faint);
			else
				text(font_field_, ix + 40, mid, st.user.c_str(), t_.text);
			if (f)
				ui_fill(cv_,
				        ix + 40 + (int)font_field_.TextWidth(st.user.c_str()) + 1,
				        mid - 8,
				        1,
				        16,
				        t_.accent_400);
			hits.user_x = ix;
			hits.user_y = yy;
			hits.user_w = iw;
			yy += LG_PILL_H + 12;
		}
		else
			hits.user_w = 0;

		/* Password pill with the round sign-in button inside it. */
		bool pf = st.in_pass && !st.busy;
		pill(ix, yy, iw, pf, !st.err.empty(), "lock");
		int mid = yy + LG_PILL_H / 2;
		if (st.pass.empty() && !pf)
			text(font_field_, ix + 40, mid, "Password", t_.text_faint);
		for (size_t i = 0; i < st.pass.size(); i++)
			ui_fill_round(cv_, ix + 40 + (int)i * 12, mid - 4, 8, 8, 4, t_.text);
		if (pf)
			ui_fill(cv_, ix + 40 + (int)st.pass.size() * 12 + 1, mid - 8, 1, 16, t_.accent_400);
		hits.pass_x = ix;
		hits.pass_y = yy;
		hits.pass_w = iw;
		int bd = 34, bx = ix + iw - 5 - bd, by = yy + (LG_PILL_H - bd) / 2;
		ui_outlined_round(cv_, bx, by, bd, bd, bd / 2, t_.accent, st.busy ? t_.accent_800 : t_.ground);
		ui_draw_icon_tint(cv_, ui_icon_get(16, "arrow-right"), bx + 9, by + 9, t_.accent_300);
		hits.sign_x = bx;
		hits.sign_y = by;
		hits.sign_d = bd;
	}

	/* The card's cached part: shadow, translucent panel, hairline, hint and
	 * session control (everything below the pills). */
	void draw_card_static(const LoginState &st, int x, int y, int w, int h)
	{
		/* Translucent panel (78% of the ground over the dimmed wallpaper), one
		 * soft shadow, accent hairline. */
		ui_shadow(cv_, x, y, w, h, UI_R_XL, 16, 20, t_.shadow_win_a, t_.shadow);
		for (int yy = y; yy < y + h; yy++)
			for (int xx = x; xx < x + w; xx++)
			{
				uint32_t a = ui_cov(ui_rr_dist4(xx, yy, x, y, w, h, UI_R_XL));
				if (a)
					cv_.blend(xx, yy, t_.ground, a * 200 >> 8);
			}
		ui_hairline(cv_, x + 1, y, w - 2, t_.accent);

		int ix = x + pad_, iw = w - 2 * pad_;
		int yy = y + pad_ + av_ + pad_ + (st.other ? LG_PILL_H + 12 : 0) + LG_PILL_H + 12;

		/* Hint / error line. */
		const char *hint = st.busy           ? "Signing in..."
		                   : !st.err.empty() ? st.err.c_str()
		                   : st.other        ? "Tab switches field \xB7 Enter signs in"
		                                     : "Press Enter to sign in";
		text(font_, ix + 24, yy + 8, hint, st.err.empty() ? t_.text_muted : t_.danger);
		yy += 16 + pad_;

		/* Session: Desktop (selected) / Console (not available yet). */
		ui_fill_round(cv_, ix, yy, iw, LG_SEG_H, LG_SEG_H / 2, t_.surface);
		int half = (iw - 6 - 2) / 2;
		ui_outlined_round(cv_, ix + 3, yy + 3, half, 32, 16, t_.edge, t_.ground);
		segment(ix + 3, yy + 3, half, "monitor", "Desktop", t_.text);
		segment(ix + 3 + half + 2, yy + 3, half, "terminal", "Console", ui_mix(t_.surface, t_.text_muted, 115));
	}

	void segment(int x, int y, int w, const char *icon, const char *label, uint32_t col)
	{
		int lw = 15 + 8 + (int)font_.TextWidth(label);
		int lx = x + (w - lw) / 2;
		ui_draw_icon_tint(cv_, ui_icon_get(16, icon), lx, y + 8, col);
		text(font_, lx + 15 + 8, y + 16, label, col);
	}

	/* Footer: ring-and-dot mark, name, host; keyboard + network at the right. */
	void draw_footer(const LoginState &st)
	{
		int y = sh_ - 20 - 16; /* centre row of the 32px footer band */
		uint32_t muted = t_.mode == UI_THEME_LIGHT ? 0x595D6Cu : 0xCFD3E5u;
		/* the mark: 16px ring (2px accent-400) + 6px accent-300 dot top-right */
		ui_stroke_round(cv_, left_, y - 8, 16, 16, 8, 2, t_.accent_400);
		ui_fill_round(cv_, left_ + 12 - 2, y - 8 - 2, 6, 6, 3, t_.accent_300);
		text(font_bold_, left_ + 16 + 12, y, "uBixOS", t_.text);
		char host[64];
		if (ubistry_get_str("/system/hostname", host, sizeof(host)) != 0)
			std::strncpy(host, "ubix.local", sizeof(host) - 1), host[sizeof(host) - 1] = '\0';
		text(font_, left_ + 16 + 12 + (int)font_bold_.TextWidth("uBixOS") + 12, y, host, t_.text_muted);

		int rx = sw_ - std::max(16, std::min(sw_ * 3 / 100, 32));

		/* Power button (btn-icon) at the far right, with its pop-over. */
		rx -= UI_CONTROL_H;
		hits.pwr_x = rx;
		hits.pwr_y = y - UI_CONTROL_H / 2;
		if (st.power_open)
			ui_fill_round(cv_, rx, hits.pwr_y, UI_CONTROL_H, UI_CONTROL_H, UI_R_MD, t_.raised);
		ui_draw_icon_tint(cv_, ui_icon_get(16, "power"), rx + 8, y - 8, muted);
		if (st.power_open)
			draw_power_menu(rx + UI_CONTROL_H, hits.pwr_y - 12);
		rx -= 8;

		rx -= 16 + 24;
		ui_draw_icon_tint(cv_, ui_icon_get(16, "ethernet"), rx + 12, y - 8, muted);
		int kw = 16 + 8 + (int)font_.TextWidth("EN");
		rx -= kw + 12;
		ui_draw_icon_tint(cv_, ui_icon_get(16, "keyboard"), rx, y - 8, muted);
		text(font_, rx + 16 + 8, y, "EN", muted);
	}

	/* Restart / Shut down pop-over, right edge at right, bottom edge at bottom. */
	void draw_power_menu(int right, int bottom)
	{
		static const char *const labels[2] = {"Restart", "Shut down"};
		static const char *const icons[2] = {"restart", "power"};
		int w = 168, h = 4 + 2 * 34 + 1 + 4;
		int x = right - w, y = bottom - h;
		ui_shadow(cv_, x, y, w, h, UI_R_MD, 8, 14, t_.shadow_win_a, t_.shadow);
		ui_outlined_round(cv_, x, y, w, h, UI_R_MD, t_.edge, t_.ground);
		for (int i = 0; i < 2; i++)
		{
			int ry = y + 4 + i * 35;
			ui_draw_icon_tint(cv_, ui_icon_get(16, icons[i]), x + 4 + 16, ry + 9, t_.text_muted);
			text(font_, x + 4 + 16 + 16 + 16, ry + 17, labels[i], t_.text);
		}
		hits.pop_x = x;
		hits.pop_y = y;
		hits.pop_w = w;
	}

      public:
	/* Cover the screen with the restart / shut-down page (caller flips). */
	void draw_power(bool off)
	{
		ogScalableFont title;
		title.Load(FONT_PATH, 19); /* 16px */
		ui_power_screen(cv_,
		                surf_,
		                title,
		                font_,
		                off ? "Shutting down..." : "Restarting...",
		                "Closing apps and syncing disks");
		base_key_.clear(); /* the cached layer no longer matches the screen */
		dirty_x = dirty_y = 0;
		dirty_w = sw_;
		dirty_h = sh_;
	}
};

/* ------------------------------------------------------------------ */
/* Auth via authd MPI (mirrors the pattern in bin/login/main.c)        */
/* ------------------------------------------------------------------ */

static struct auth_response do_auth(const std::string &reply_mbox,
                                    const std::string &username,
                                    const std::string &password)
{
	struct auth_response resp;
	::memset(&resp, 0, sizeof(resp));

	struct auth_request req;
	::memset(&req, 0, sizeof(req));
	::strncpy(req.reply_mbox, reply_mbox.c_str(), AUTH_MBOX_MAX - 1);
	::strncpy(req.username, username.c_str(), AUTH_USER_MAX - 1);
	::strncpy(req.password, password.c_str(), AUTH_PASS_MAX - 1);

	mpi_message_t msg = {};
	msg.header = AUTHD_MSG_REQUEST;
	::memcpy(msg.data, &req, sizeof(req));

	/* Drain any stale reply left in our mailbox from an earlier attempt, so we
	 * accept only THIS request's response (otherwise a slow first attempt's late
	 * reply would be consumed by the next attempt). */
	mpi_message_t rmsg;
	while (mpi_fetchMessage(reply_mbox.c_str(), &rmsg) == 0)
		;

	if (mpi_postMessage(AUTHD_MBOX, AUTHD_MSG_REQUEST, &msg) != 0)
		return resp; /* authd not reachable */

	/* Block for the reply rather than spin a fixed number of yields.  authd verifies
	 * with PBKDF2 (50k iterations) which takes tens of ms; under SMP it runs in
	 * parallel on another core, so a brief yield-poll elapses long before the hash
	 * completes and mis-reports a correct-but-slow login as rejected ("incorrect on
	 * first attempt").  mpi_waitMessage sleeps until a message arrives; the loop
	 * skips any non-response message and bounds the total wait (~5 s) against a dead
	 * authd. */
	for (int i = 0; i < 50; i++)
	{
		if (mpi_waitMessage(reply_mbox.c_str(), &rmsg, 10 /* ticks ≈ 100 ms */) == 0 &&
		    rmsg.header == AUTHD_MSG_RESPONSE)
		{
			::memcpy(&resp, rmsg.data, sizeof(resp));
			return resp;
		}
	}

	return resp; /* timeout — authd unresponsive */
}

/* ------------------------------------------------------------------ */
/* Session: fork taskbar, wait for it to exit                          */
/* ------------------------------------------------------------------ */

/**
 * Tell the views compositor which user owns the session so it renders that
 * user's desktop (per-user settings).  An empty name reverts to the system
 * default at logout.  Best-effort: ignored if views is unreachable.
 */
static void set_display_user(const std::string &username)
{
	mpi_message_t msg = {};
	struct display_set_user *su = (struct display_set_user *)msg.data;

	::snprintf(su->user, sizeof(su->user), "%s", username.c_str());
	msg.header = DISPLAY_SET_USER;
	mpi_postMessage(VIEWS_MBOX, DISPLAY_SET_USER, &msg);
}

static void run_session(const struct auth_response &resp, const std::string &username)
{
	static const char *taskbar_argv[] = {"taskbar", nullptr};
	char home_env[140], user_env[48], logname_env[48], shell_env[140], path_env[64];

	::snprintf(home_env, sizeof(home_env), "HOME=%s", resp.home[0] ? resp.home : "/");
	::snprintf(user_env, sizeof(user_env), "USER=%s", username.c_str());
	::snprintf(logname_env, sizeof(logname_env), "LOGNAME=%s", username.c_str());
	::snprintf(shell_env, sizeof(shell_env), "SHELL=%s", resp.shell[0] ? resp.shell : "/bin/shell");
	::snprintf(path_env, sizeof(path_env), "PATH=/bin:/sbin:/usr/bin:/usr/sbin");

	static const char *taskbar_envp[7];
	taskbar_envp[0] = home_env;
	taskbar_envp[1] = user_env;
	taskbar_envp[2] = logname_env;
	taskbar_envp[3] = shell_env;
	taskbar_envp[4] = path_env;
	taskbar_envp[5] = "LD_LIBRARY_PATH=/lib:/usr/lib";
	taskbar_envp[6] = nullptr;

	int pid = ::fork();
	if (pid == 0)
	{
		/* Put the session in its own process group (== the taskbar's pid) so
		 * everything it launches (apps, their children) inherits the group and
		 * vlogin can tear the whole session down on logout.  Set it in the child
		 * too, race-free against the parent's setpgid. */
		::setpgid(0, 0);
		::setuid(resp.uid);
		::setgid(resp.gid);
		/* Start the GUI session in the user's home directory, mirroring
		 * /bin/login's text-console behaviour.  Everything the taskbar
		 * launches (terminal → tcsh, apps) inherits this cwd, so the shell
		 * opens in $HOME instead of "/". */
		::chdir(resp.home[0] ? resp.home : "/");
		::execve(TASKBAR_PATH, (char *const *)taskbar_argv, (char *const *)taskbar_envp);
		::_exit(1);
	}
	if (pid < 0)
		return;

	/* Session group = taskbar pid (also set here to win the fork/exec race). */
	::setpgid(pid, pid);

	/* Apply this user's desktop for the lifetime of the session. */
	set_display_user(username);

	/* Wait for the session leader (taskbar) to exit (logout). */
	while (pidStatus(pid) == pid)
		ubix::yield();

	/* Tear down the rest of the session: kill every process still in the
	 * group — the apps the user launched, which would otherwise keep running
	 * (and holding memory + windows) across the next login.  vlogin itself is in
	 * a different group, so it survives to show the login screen again. */
	::kill(-pid, SIGKILL);

	/* Revert to the system-default desktop at logout. */
	set_display_user("");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	/* Unique mailboxes for this process */
	std::string my_mbox = "vlogin." + std::to_string(ubix::pid());
	std::string auth_mbox = "vlogin.auth." + std::to_string(ubix::pid());

	ubix::Mailbox mbox;
	mbox.assign(my_mbox);
	if (!mbox.create())
	{
		std::printf("vlogin: mbox create failed\n");
		return 1;
	}

	mpi_createMbox(auth_mbox.c_str());

	std::printf("vlogin: waiting for views compositor\n");

	/* Query the compositor for the screen dimensions.  Retryable: at cold
	 * session start views may still be loading (demand-paged binary, fonts,
	 * wallpaper), and the bounded yield-poll below can expire long before it
	 * is ready to answer — the claim loop re-runs this before each retry so a
	 * later attempt claims with the real geometry, not the 800x600 fallback. */
	int scr_w = 800, scr_h = 600;
	auto query_screen = [&]() -> bool
	{
		mpi_message_t qmsg = {};
		struct display_query *dq = (struct display_query *)qmsg.data;
		qmsg.header = DISPLAY_QUERY;
		::strncpy(dq->reply, my_mbox.c_str(), sizeof(dq->reply) - 1);
		while (mpi_postMessage(VIEWS_MBOX, DISPLAY_QUERY, &qmsg) != 0)
			ubix::yield();

		mpi_message_t reply;
		for (int i = 0; i < 5000; i++)
		{
			if (mbox.try_fetch(reply) && reply.header == DISPLAY_INFO)
			{
				struct display_info *di = (struct display_info *)reply.data;
				if (di->screen_w > 0)
					scr_w = (int)di->screen_w;
				if (di->screen_h > 0)
					scr_h = (int)di->screen_h;
				return true;
			}
			ubix::yield();
		}
		return false;
	};
	{
		bool got = query_screen();
		std::printf("vlogin: screen %dx%d%s\n", scr_w, scr_h, got ? "" : " (timeout, using defaults)");
	}

	/* ---- Window claim/release helpers -------------------------------- */
	uint32_t win_id = 0;
	void *shm = nullptr;
	int act_w = 0, act_h = 0;

	auto claim_window = [&]() -> bool
	{
		mpi_message_t msg = {};
		struct display_claim_req *cr = (struct display_claim_req *)msg.data;
		msg.header = DISPLAY_CLAIM;
		cr->x = 0;
		cr->y = 0;
		cr->w = scr_w;
		cr->h = scr_h;
		cr->sender_pid = ubix::pid();
		cr->no_decor = 1;
		::strncpy(cr->title, "vLogin", sizeof(cr->title) - 1);
		::strncpy(cr->reply, my_mbox.c_str(), sizeof(cr->reply) - 1);
		ubix::post_message(VIEWS_MBOX, DISPLAY_CLAIM, msg);

		/* Bounded ack wait — an unbounded fetch loop hung vlogin forever if the
		 * claim was dropped (compositor busy/restarting).  The mailbox also
		 * carries strays (a DISPLAY_INFO from a timed-out query arriving late),
		 * so fetch UNTIL the ACK rather than judging the first message — a
		 * stray misread as a denial abandoned the window views had actually
		 * created, leaving a black never-drawn box on the login screen.  A
		 * timeout is treated as a deny; the retry loop re-queries + re-claims
		 * (and releases any late-arriving ACK's window when draining). */
		mpi_message_t reply;
		bool acked = false;
		for (int i = 0; i < 20000; i++)
		{
			if (mbox.try_fetch(reply))
			{
				if (reply.header == DISPLAY_ACK)
				{
					acked = true;
					break;
				}
				continue; /* stray (late DISPLAY_INFO etc.) — discard */
			}
			ubix::yield();
		}
		if (!acked)
		{
			std::printf("vlogin: DISPLAY_CLAIM unanswered\n");
			return false;
		}

		struct display_ack *da = (struct display_ack *)reply.data;
		win_id = da->window_id;
		shm = da->shm_base;
		act_w = da->w;
		act_h = da->h;
		std::printf("vlogin: window %u shm=%p %dx%d\n", win_id, shm, act_w, act_h);
		return (shm != nullptr && act_w > 0 && act_h > 0);
	};

	auto release_window = [&]()
	{
		mpi_message_t m = {};
		struct display_release *dr = (struct display_release *)m.data;
		m.header = DISPLAY_RELEASE;
		dr->window_id = win_id;
		ubix::post_message(VIEWS_MBOX, DISPLAY_RELEASE, m);

		/* Unmap our view of the shared window buffer.  views frees its own copy
		 * on DISPLAY_RELEASE, but the shared frames are only reclaimed once every
		 * mapper unmaps — and vlogin loops (it does not exit between logins), so
		 * without this each login would strand a full-screen (~scr_w*scr_h*4)
		 * mapping.  munmap drops vlogin's reference; the frame frees when views
		 * has also released it. */
		if (shm != nullptr && act_w > 0 && act_h > 0)
			munmap(shm, (size_t)act_w * (size_t)act_h * 4u);
		win_id = 0;
		shm = nullptr;
	};

	/* ---- Outer session loop: claim → login → session → release → repeat */
	for (;;)
	{
		/* Claim with retry.  At session start vlogin can race the compositor:
		 * the query times out (stale 800x600 defaults) and the claim is denied
		 * or unanswered.  Exiting here killed the login screen for the whole
		 * boot (background drawn, no dialog); instead pause, re-query the real
		 * geometry, and re-claim — views finishes starting within a few
		 * hundred ms.  ~60 tries ≈ 15 s ceiling before giving up for real. */
		int tries = 0;
		while (!claim_window())
		{
			if (++tries >= 60)
			{
				std::printf("vlogin: failed to claim window after %d tries\n", tries);
				return 1;
			}
			mpi_message_t drain;
			if (mbox.wait(drain, 25) && drain.header == DISPLAY_ACK)
			{
				/* A previous claim's ACK arrived after we gave up on it: views
				 * created that window and nobody owns it.  Release it, or it
				 * lingers as a black never-drawn box behind the login UI. */
				struct display_ack *da = (struct display_ack *)drain.data;
				mpi_message_t rel = {};
				struct display_release *dr = (struct display_release *)rel.data;
				rel.header = DISPLAY_RELEASE;
				dr->window_id = da->window_id;
				ubix::post_message(VIEWS_MBOX, DISPLAY_RELEASE, rel);
			}
			query_screen();
		}

		ogSurface surf;
		if (!surf.ogAttach(shm, (uint32_t)act_w, (uint32_t)act_h, OG_PIXFMT_32BPP))
		{
			std::printf("vlogin: ogAttach failed\n");
			return 1;
		}

		LoginUI ui(surf, act_w, act_h);
		if (!ui.loaded)
		{
			std::printf("vlogin: font load failed (%s)\n", FONT_PATH);
			return 1;
		}

		std::printf("vlogin: UI ready, entering event loop\n");

		auto flip = [&]()
		{
			mpi_message_t m = {};
			struct display_flip *fl = (struct display_flip *)m.data;
			m.header = DISPLAY_FLIP;
			fl->window_id = win_id;
			fl->dirty_x = ui.dirty_x; /* only what the last draw() changed */
			fl->dirty_y = ui.dirty_y;
			fl->dirty_w = ui.dirty_w;
			fl->dirty_h = ui.dirty_h;
			ubix::post_message(VIEWS_MBOX, DISPLAY_FLIP, m);
		};

		LoginState st;
		bool do_session = false;
		struct auth_response session_resp = {};

		/* Remembered user: the card offers them first ("Other user..." to switch). */
		{
			char last[64];
			if (ubistry_get_str(LAST_USER_KEY, last, sizeof(last)) == 0)
				st.last_user = last;
		}
		auto reset = [&]()
		{
			st.other = st.last_user.empty();
			st.user.clear();
			st.pass.clear();
			st.err.clear();
			st.busy = false;
			st.in_pass = !st.other;
		};
		auto redraw = [&]()
		{
			ui.draw(st);
			flip();
		};
		/* Switch between the remembered user and typing another name. */
		auto toggle_other = [&]()
		{
			if (st.last_user.empty())
				return;
			st.other = !st.other;
			st.user.clear();
			st.pass.clear();
			st.err.clear();
			st.in_pass = !st.other;
		};
		/* Authenticate the shown account; true = start the session. */
		auto submit = [&]() -> bool
		{
			if (st.account().empty())
			{
				st.in_pass = false;
				return false;
			}
			st.busy = true;
			st.err.clear();
			redraw();
			session_resp = do_auth(auth_mbox, st.account(), st.pass);
			st.busy = false;
			if (session_resp.ok)
			{
				ubistry_set_str(LAST_USER_KEY, st.account().c_str());
				return true;
			}
			st.err = "Wrong password \xB7 try again";
			st.pass.clear();
			st.in_pass = true;
			return false;
		};

		reset();
		redraw();
		long shown_minute = LoginUI::minute_now();
		uint8_t prev_buttons = 0; /* last mouse button state, for release-edge clicks */

		/* ---- Inner login loop ------------------------------------- */
		for (;;)
		{
			mpi_message_t ev;
			/* Block up to ~250 ms for input; on timeout, blink the caret (and,
			 * later, tick a clock) rather than busy-spinning on yield(). */
			if (!mbox.wait(ev, 25))
			{
				/* Idle: keep the big clock current. */
				if (LoginUI::minute_now() != shown_minute)
				{
					shown_minute = LoginUI::minute_now();
					redraw();
				}
				ubix::yield();
				continue;
			}

			if (ev.header == DISPLAY_CLOSE)
				goto done;

			if (ev.header == DISPLAY_MOUSE)
			{
				/* Clicks: sign-in button, fields (focus), "Other user..." link. */
				struct display_mouse_ev *me = (struct display_mouse_ev *)ev.data;
				bool click = (prev_buttons & 1) && !(me->buttons & 1); /* release edge */
				prev_buttons = me->buttons;
				if (!click || st.busy)
					continue;
				const LoginHits &h = ui.hits;
				int mx = me->x, my = me->y;

				/* Power: the pop-over takes the click while open. */
				if (st.power_open)
				{
					st.power_open = false;
					for (int i = 0; i < 2; i++)
						if (LoginHits::in(
						        mx, my, h.pop_x + 4, h.pop_y + 4 + i * 35, h.pop_w - 8, 34))
						{
							bool off = (i == 1);
							ui.draw_power(off);
							flip();
							struct timespec ts = {0, 700 * 1000 * 1000};
							nanosleep(&ts, nullptr);
							ubix_power(off ? UBIX_POWER_OFF : UBIX_POWER_REBOOT);
						}
					redraw();
					continue;
				}
				if (LoginHits::in(mx, my, h.pwr_x, h.pwr_y, UI_CONTROL_H, UI_CONTROL_H))
				{
					st.power_open = true;
					redraw();
					continue;
				}
				int dx = mx - (h.sign_x + h.sign_d / 2), dy = my - (h.sign_y + h.sign_d / 2);
				if (dx * dx + dy * dy <= (h.sign_d / 2) * (h.sign_d / 2))
				{
					if (submit())
					{
						do_session = true;
						break;
					}
				}
				else if (h.link_w && LoginHits::in(mx, my, h.link_x, h.link_y, h.link_w, 16))
					toggle_other();
				else if (h.user_w && LoginHits::in(mx, my, h.user_x, h.user_y, h.user_w, LG_PILL_H))
					st.in_pass = false;
				else if (LoginHits::in(mx, my, h.pass_x, h.pass_y, h.pass_w, LG_PILL_H))
					st.in_pass = true;
				else
					continue;
				redraw();
				continue;
			}

			if (ev.header != DISPLAY_KEY)
				continue;

			struct display_key *dk = (struct display_key *)ev.data;
			if (!dk->pressed || st.busy)
				continue;

			uint32_t kc = dk->keycode;

			if (kc == 0x1B && st.power_open)
				st.power_open = false; /* Esc closes the power pop-over first */
			else if (kc == 0x1B)           /* Esc: remembered user <-> other user */
				toggle_other();
			else if (kc == '\t')
			{
				if (st.other)
					st.in_pass = !st.in_pass;
			}
			else if (kc == '\n' || kc == '\r')
			{
				if (!st.in_pass)
				{
					if (!st.user.empty())
						st.in_pass = true;
				}
				else if (submit())
				{
					do_session = true;
					break; /* exit inner loop → release → session */
				}
			}
			else if (kc == '\b' || kc == 0x7F) /* BS or DEL — both keyboards send 0x7F */
			{
				std::string &field = st.in_pass ? st.pass : st.user;
				if (!field.empty())
					field.pop_back();
			}
			else if (kc >= 0x20 && kc < 0x7F)
			{
				std::string &field = st.in_pass ? st.pass : st.user;
				if ((int)field.size() < MAX_FIELD)
					field += (char)kc;
				st.err.clear();
			}

			redraw();
		}

		/* Release window so the session (taskbar + apps) owns the screen */
		release_window();

		if (do_session)
		{
			std::printf("vlogin: starting session for %s\n", st.account().c_str());
			run_session(session_resp, st.account());
		}
		/* Loop back to claim_window for the next user */
	}

done:
	release_window();
	mbox.destroy();
	return 0;
}
