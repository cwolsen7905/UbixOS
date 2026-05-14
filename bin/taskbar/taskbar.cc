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

extern "C" {
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sys.h>
#include <sys/sched.h>
#include <sys/mpi.h>
#include <views/display_proto.h>
}
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

static uint32_t  g_sw, g_sh;
static uint32_t  g_win_id;
static void     *g_tb_shm  = NULL;

static uint32_t  g_fly_id  = 0;
static void     *g_fly_shm = NULL;
static int       g_fly_open = 0;

static ogSurface g_tb_surf;
static ogSurface g_fly_surf;
static ogBitFont g_font;

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

	/* Clock */
	char tstr[12];
	get_time_str(tstr);
	int cx = sw - CLOCK_W - 2;
	g_tb_surf.ogFillRect(cx, 2, cx + CLOCK_W - 1, TB_H - 3, TB_BTN_N);
	g_tb_surf.ogRect(cx, 2, cx + CLOCK_W - 1, TB_H - 3, TB_SEP);
	font_fg(g_font, COL_WHITE);
	font_bg(g_font, TB_BTN_N);
	g_font.PutString(g_tb_surf, cx + 8, 12, tstr);
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
	static char views_mbox[] = "views";
	mpi_postMessage(views_mbox, DISPLAY_FLIP, &msg);
}

/* ------------------------------------------------------------------ */
/* Process launch                                                       */
/* ------------------------------------------------------------------ */

static void
launch(const char *path)
{
	if (fork() == 0) {
		char *argv[] = { (char *)path, NULL };
		char *envp[] = { NULL };
		execve(path, argv, envp);
		_exit(1);
	}
}

/* ------------------------------------------------------------------ */
/* Flyout open / close                                                  */
/* ------------------------------------------------------------------ */

static void
show_flyout(void)
{
	if (g_fly_open)
		return;

	static char tb_mbox[] = "taskbar";
	mpi_message_t claim;
	struct display_claim_req *creq = (struct display_claim_req *)claim.data;
	claim.header      = DISPLAY_CLAIM;
	creq->x           = 2;
	creq->y           = (int32_t)(g_sh - TB_H - FLY_H);
	creq->w           = FLY_W;
	creq->h           = FLY_H;
	creq->sender_pid  = getpid();
	creq->no_decor    = 1;
	strncpy(creq->title, "flyout", sizeof(creq->title) - 1);
	creq->title[sizeof(creq->title) - 1] = '\0';
	strncpy(creq->reply, tb_mbox, sizeof(creq->reply) - 1);
	creq->reply[sizeof(creq->reply) - 1] = '\0';

	static char views_mbox[] = "views";
	mpi_postMessage(views_mbox, DISPLAY_CLAIM, &claim);

	mpi_message_t reply;
	while (mpi_fetchMessage(tb_mbox, &reply) != 0)
		sched_yield();

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

	static char views_mbox[] = "views";
	mpi_message_t msg;
	struct display_release *rel = (struct display_release *)msg.data;
	msg.header     = DISPLAY_RELEASE;
	rel->window_id = g_fly_id;
	mpi_postMessage(views_mbox, DISPLAY_RELEASE, &msg);

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

	static char tb_mbox[]    = "taskbar";
	static char views_mbox[] = "views";

	if (mpi_createMbox(tb_mbox) != 0) {
		printf("taskbar: mpi_createMbox failed\n");
		return 1;
	}

	/* Query screen geometry */
	mpi_message_t msg;
	struct display_query *dq = (struct display_query *)msg.data;
	msg.header = DISPLAY_QUERY;
	strncpy(dq->reply, tb_mbox, sizeof(dq->reply) - 1);
	dq->reply[sizeof(dq->reply) - 1] = '\0';
	mpi_postMessage(views_mbox, DISPLAY_QUERY, &msg);

	mpi_message_t reply;
	while (mpi_fetchMessage(tb_mbox, &reply) != 0)
		sched_yield();

	if (reply.header != DISPLAY_INFO) {
		printf("taskbar: unexpected reply to DISPLAY_QUERY\n");
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
	creq->sender_pid  = getpid();
	creq->no_decor    = 1;
	strncpy(creq->title, "taskbar", sizeof(creq->title) - 1);
	creq->title[sizeof(creq->title) - 1] = '\0';
	strncpy(creq->reply, tb_mbox, sizeof(creq->reply) - 1);
	creq->reply[sizeof(creq->reply) - 1] = '\0';
	mpi_postMessage(views_mbox, DISPLAY_CLAIM, &claim);

	while (mpi_fetchMessage(tb_mbox, &reply) != 0)
		sched_yield();

	if (reply.header != DISPLAY_ACK) {
		printf("taskbar: DISPLAY_CLAIM denied\n");
		return 1;
	}

	struct display_ack *da = (struct display_ack *)reply.data;
	g_win_id  = da->window_id;
	g_tb_shm  = da->shm_base;

	if (!g_tb_shm) {
		printf("taskbar: shm_base is NULL\n");
		return 1;
	}

	g_tb_surf.ogAttach(g_tb_shm, g_sw, TB_H, OG_PIXFMT_32BPP);

	if (!g_font.Load(FONT_PATH, 0)) {
		printf("taskbar: font load failed\n");
		return 1;
	}

	printf("taskbar: window %u at 0x%X, %dx%d+%d+%d\n",
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

		while (mpi_fetchMessage(tb_mbox, &reply) == 0) {
			if (reply.header == DISPLAY_KEY)
				continue;
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

		sched_yield();
	}

	return 0;
}
