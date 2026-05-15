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

#define FONT_PATH   "sys:/var/fonts/ROM8X8.DPF"

/* Taskbar geometry */
#define TB_H        32
#define BTN_W       80
#define CLOCK_W     80

/* Flyout geometry */
#define FLY_W       120
#define FLY_H        80
#define FLY_ITEM_H  (FLY_H / 2)

/* Colours: format matches FB_RGB(r,g,b) = (r<<16)|(g<<8)|b */
static const uint32_t TB_BG        = 0x003C8Cu;
static const uint32_t TB_BTN_N     = 0x0050B0u;
static const uint32_t TB_BTN_P     = 0x0070D0u;
static const uint32_t TB_SEP       = 0x002868u;
static const uint32_t FLY_BG_C     = 0x002860u;
static const uint32_t FLY_ITEM_C   = 0x004080u;
static const uint32_t COL_WHITE     = 0x00FFFFFFu;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static void
font_fg(ogBitFont &f, uint32_t c)
{
	f.SetFGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

static void
font_bg(ogBitFont &f, uint32_t c)
{
	f.SetBGColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

/* ------------------------------------------------------------------ */
/* Time formatting                                                      */
/* ------------------------------------------------------------------ */

#define MINUTE  60
#define HOUR    (60 * MINUTE)
#define DAY     (24 * HOUR)
#define YEAR    (365 * DAY)

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

static void
get_time_str(char *buf)
{
	int t = gettime();
	int year = (t / YEAR) + 1970;
	t -= YEAR * (year - 1970);
	t -= DAY * (((year - 1970) + 1) / 4);

	int month = 0;
	for (int i = 11; i >= 0; i--) {
		if ((t - month_secs[i]) > 0) {
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
/* State                                                                */
/* ------------------------------------------------------------------ */

static ubix::Mailbox g_tb_mbox;

static uint32_t  g_sw, g_sh;
static uint32_t  g_win_id;
static void     *g_tb_shm  = NULL;

static uint32_t  g_fly_id  = 0;
static void     *g_fly_shm = NULL;
static int       g_fly_open = 0;

static ogSurface g_tb_surf;
static ogSurface g_fly_surf;
static ogBitFont g_font;

/* Window list tracked via DISPLAY_NOTIFY */
#define WIN_BTN_W    96

struct TrackedWin {
	uint32_t    id;
	std::string title;
};
static std::vector<TrackedWin> g_tracked;

/* ------------------------------------------------------------------ */
/* Drawing                                                              */
/* ------------------------------------------------------------------ */

static void
draw_taskbar(int btn_pressed)
{
	uint32_t btn_color = btn_pressed ? TB_BTN_P : TB_BTN_N;
	int sw = (int)g_sw;

	g_tb_surf.ogFillRect(0, 0, sw - 1, TB_H - 1, TB_BG);
	g_tb_surf.ogFillRect(0, 0, sw - 1, 0, TB_SEP);

	/* Launcher button */
	g_tb_surf.ogFillRect(2, 2, 2 + BTN_W - 1, TB_H - 3, btn_color);
	g_tb_surf.ogRect(2, 2, 2 + BTN_W - 1, TB_H - 3, TB_SEP);
	font_fg(g_font, COL_WHITE);
	font_bg(g_font, btn_color);
	g_font.PutString(g_tb_surf, 10, 12, "UbixOS");

	/* Window list buttons */
	int wx = 2 + BTN_W + 4;
	int clock_x = sw - CLOCK_W - 2;
	for (const auto &tw : g_tracked) {
		if (wx + WIN_BTN_W > clock_x - 4)
			break;
		g_tb_surf.ogFillRect(wx, 2, wx + WIN_BTN_W - 1,
		    TB_H - 3, TB_BTN_N);
		g_tb_surf.ogRect(wx, 2, wx + WIN_BTN_W - 1,
		    TB_H - 3, TB_SEP);
		font_fg(g_font, COL_WHITE);
		font_bg(g_font, TB_BTN_N);
		g_font.PutString(g_tb_surf, wx + 4, 12, tw.title.c_str());
		wx += WIN_BTN_W + 2;
	}

	/* Clock */
	char tstr[12];
	get_time_str(tstr);
	g_tb_surf.ogFillRect(clock_x, 2, clock_x + CLOCK_W - 1,
	    TB_H - 3, TB_BTN_N);
	g_tb_surf.ogRect(clock_x, 2, clock_x + CLOCK_W - 1,
	    TB_H - 3, TB_SEP);
	font_fg(g_font, COL_WHITE);
	font_bg(g_font, TB_BTN_N);
	g_font.PutString(g_tb_surf, clock_x + 8, 12, tstr);
}

static void
win_add(uint32_t id, const char *title)
{
	g_tracked.push_back({id, std::string(title)});
}

static void
win_remove(uint32_t id)
{
	auto it = std::find_if(g_tracked.begin(), g_tracked.end(),
	    [id](const TrackedWin &w) { return w.id == id; });
	if (it != g_tracked.end())
		g_tracked.erase(it);
}

/* Return index of window button hit at taskbar-relative x, or -1 */
static int
winbtn_hit(int mx)
{
	int wx = 2 + BTN_W + 4;
	int clock_x = (int)g_sw - CLOCK_W - 2;
	for (int i = 0; i < (int)g_tracked.size(); i++) {
		if (wx + WIN_BTN_W > clock_x - 4)
			break;
		if (mx >= wx && mx < wx + WIN_BTN_W)
			return i;
		wx += WIN_BTN_W + 2;
	}
	return -1;
}

static void
raise_window(uint32_t id)
{
	mpi_message_t msg;
	struct display_raise *dr = (struct display_raise *)msg.data;
	msg.header    = DISPLAY_RAISE;
	dr->window_id = id;
	ubix::post_message("views", DISPLAY_RAISE, msg);
}

static void
draw_flyout(void)
{
	g_fly_surf.ogFillRect(0, 0, FLY_W - 1, FLY_H - 1, FLY_BG_C);

	g_fly_surf.ogFillRect(2, 2, FLY_W - 3, FLY_ITEM_H - 3, FLY_ITEM_C);
	font_fg(g_font, COL_WHITE);
	font_bg(g_font, FLY_ITEM_C);
	g_font.PutString(g_fly_surf, 8, 10, "Terminal");

	g_fly_surf.ogFillRect(2, FLY_ITEM_H + 2,
	    FLY_W - 3, FLY_H - 3, FLY_ITEM_C);
	font_fg(g_font, COL_WHITE);
	font_bg(g_font, FLY_ITEM_C);
	g_font.PutString(g_fly_surf, 8, FLY_ITEM_H + 10, "About");
}

/* ------------------------------------------------------------------ */
/* MPI helpers                                                          */
/* ------------------------------------------------------------------ */

static void
send_flip(uint32_t win_id)
{
	mpi_message_t msg;
	struct display_flip *fl = (struct display_flip *)msg.data;
	msg.header    = DISPLAY_FLIP;
	fl->window_id = win_id;
	fl->dirty_x   = 0;
	fl->dirty_y   = 0;
	fl->dirty_w   = 0;
	fl->dirty_h   = 0;
	ubix::post_message("views", DISPLAY_FLIP, msg);
}

/* ------------------------------------------------------------------ */
/* Process launch                                                       */
/* ------------------------------------------------------------------ */

/*
 * Launcher helper: forked once at startup before any shared memory is
 * mapped.  Taskbar writes null-terminated paths down the pipe; the helper
 * forks+execs each one.  This keeps taskbar's COW-free shared buffers
 * intact across repeated launches.
 */
static int g_launcher_fd = -1;

static void
launcher_init(void)
{
	int pfd[2];
	if (::pipe(pfd) != 0)
		return;

	if (::fork() == 0) {
		::close(pfd[1]);
		char path[256];
		int  len = 0;
		char ch;
		for (;;) {
			if (::read(pfd[0], &ch, 1) <= 0)
				::_exit(0);
			if (ch != '\0') {
				if (len < (int)sizeof(path) - 1)
					path[len++] = ch;
				continue;
			}
			if (len == 0)
				continue;
			path[len] = '\0';
			len = 0;
			if (::fork() == 0) {
				char *argv[] = { path, nullptr };
				char *envp[] = { nullptr };
				::execve(path, argv, envp);
				::_exit(1);
			}
		}
	}

	::close(pfd[0]);
	g_launcher_fd = pfd[1];
}

static void
launch(const char *path)
{
	if (g_launcher_fd < 0)
		return;
	::write(g_launcher_fd, path, std::strlen(path) + 1);
}

/* ------------------------------------------------------------------ */
/* Flyout open / close                                                  */
/* ------------------------------------------------------------------ */

static void
show_flyout(void)
{
	if (g_fly_open)
		return;

	mpi_message_t claim;
	struct display_claim_req *creq = (struct display_claim_req *)claim.data;
	claim.header      = DISPLAY_CLAIM;
	creq->x           = 2;
	creq->y           = (int32_t)(g_sh - TB_H - FLY_H);
	creq->w           = FLY_W;
	creq->h           = FLY_H;
	creq->sender_pid  = ubix::pid();
	creq->no_decor    = 1;
	std::strncpy(creq->title, "flyout", sizeof(creq->title) - 1);
	creq->title[sizeof(creq->title) - 1] = '\0';
	std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
	creq->reply[sizeof(creq->reply) - 1] = '\0';

	ubix::post_message("views", DISPLAY_CLAIM, claim);

	mpi_message_t reply;
	while (!g_tb_mbox.try_fetch(reply))
		ubix::yield();

	if (reply.header != DISPLAY_ACK)
		return;

	struct display_ack *da = (struct display_ack *)reply.data;
	g_fly_id   = da->window_id;
	g_fly_shm  = da->shm_base;
	g_fly_open = 1;

	g_fly_surf.ogAttach(g_fly_shm, FLY_W, FLY_H, OG_PIXFMT_32BPP);
	draw_flyout();
	send_flip(g_fly_id);
}

static void
hide_flyout(void)
{
	if (!g_fly_open)
		return;

	mpi_message_t msg;
	struct display_release *rel = (struct display_release *)msg.data;
	msg.header     = DISPLAY_RELEASE;
	rel->window_id = g_fly_id;
	ubix::post_message("views", DISPLAY_RELEASE, msg);

	g_fly_open = 0;
	g_fly_id   = 0;
	g_fly_shm  = NULL;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	launcher_init();

	g_tb_mbox.assign("taskbar");
	if (!g_tb_mbox.create()) {
		std::printf("taskbar: mpi_createMbox failed\n");
		return 1;
	}

	/* Query screen geometry */
	mpi_message_t msg;
	struct display_query *dq = (struct display_query *)msg.data;
	msg.header = DISPLAY_QUERY;
	std::strncpy(dq->reply, "taskbar", sizeof(dq->reply) - 1);
	dq->reply[sizeof(dq->reply) - 1] = '\0';
	ubix::post_message("views", DISPLAY_QUERY, msg);

	mpi_message_t reply;
	while (!g_tb_mbox.try_fetch(reply))
		ubix::yield();

	if (reply.header != DISPLAY_INFO) {
		std::printf("taskbar: unexpected reply to DISPLAY_QUERY\n");
		return 1;
	}

	struct display_info *di = (struct display_info *)reply.data;
	g_sw = di->screen_w;
	g_sh = di->screen_h;

	/* Claim bottom strip */
	mpi_message_t claim;
	struct display_claim_req *creq = (struct display_claim_req *)claim.data;
	claim.header      = DISPLAY_CLAIM;
	creq->x           = 0;
	creq->y           = (int32_t)(g_sh - TB_H);
	creq->w           = (int32_t)g_sw;
	creq->h           = TB_H;
	creq->sender_pid  = ubix::pid();
	creq->no_decor    = 1;
	std::strncpy(creq->title, "taskbar", sizeof(creq->title) - 1);
	creq->title[sizeof(creq->title) - 1] = '\0';
	std::strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
	creq->reply[sizeof(creq->reply) - 1] = '\0';
	ubix::post_message("views", DISPLAY_CLAIM, claim);

	while (!g_tb_mbox.try_fetch(reply))
		ubix::yield();

	if (reply.header != DISPLAY_ACK) {
		std::printf("taskbar: DISPLAY_CLAIM denied\n");
		return 1;
	}

	struct display_ack *da = (struct display_ack *)reply.data;
	g_win_id  = da->window_id;
	g_tb_shm  = da->shm_base;

	if (!g_tb_shm) {
		std::printf("taskbar: shm_base is NULL\n");
		return 1;
	}

	g_tb_surf.ogAttach(g_tb_shm, g_sw, TB_H, OG_PIXFMT_32BPP);

	if (!g_font.Load(FONT_PATH, 0)) {
		std::printf("taskbar: font load failed\n");
		return 1;
	}

	std::printf("taskbar: window %u at 0x%X, %dx%d+%d+%d\n",
	    g_win_id, (uint32_t)(uintptr_t)g_tb_shm,
	    da->w, da->h, da->x, da->y);

	int btn_pressed = 0;
	draw_taskbar(0);
	send_flip(g_win_id);

	int last_sec = -1;
	for (;;) {
		int t = gettime() % 60;
		if (t != last_sec) {
			last_sec = t;
			draw_taskbar(btn_pressed);
			send_flip(g_win_id);
		}

		while (g_tb_mbox.try_fetch(reply)) {
			if (reply.header == DISPLAY_KEY)
				continue;

			if (reply.header == DISPLAY_NOTIFY) {
				struct display_notify *dn =
				    (struct display_notify *)reply.data;
				if (dn->added)
					win_add(dn->window_id, dn->title);
				else
					win_remove(dn->window_id);
				draw_taskbar(btn_pressed);
				send_flip(g_win_id);
				continue;
			}

			if (reply.header != DISPLAY_MOUSE)
				continue;

			struct display_mouse_ev *me =
			    (struct display_mouse_ev *)reply.data;
			int pressed = (me->buttons & 1) != 0;

			if (me->window_id == g_fly_id) {
				if (!pressed && g_fly_open) {
					int item = me->y / FLY_ITEM_H;
					hide_flyout();
					if (item == 0)
						launch("sys:/bin/term");
				}
				continue;
			}

			if (pressed != btn_pressed) {
				btn_pressed = pressed;
				draw_taskbar(btn_pressed);
				send_flip(g_win_id);

				/* Window list button click */
				if (!pressed) {
					int wi = winbtn_hit(me->x);
					if (wi >= 0) {
						raise_window(g_tracked[wi].id);
						continue;
					}
				}

				int in_btn = (me->x >= 2 &&
				    me->x < 2 + BTN_W);
				if (!pressed && in_btn) {
					if (g_fly_open)
						hide_flyout();
					else
						show_flyout();
				}
			}
		}

		ubix::yield();
	}

	return 0;
}
