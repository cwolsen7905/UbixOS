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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sys.h>
#include <sys/sched.h>
#include <sys/mpi.h>
#include <fb/fb.h>
#include <views/display_proto.h>

static void
launch(const char *path)
{
	pid_t pid = fork();
	if (pid == 0) {
		char *argv[] = { (char *)path, NULL };
		char *envp[] = { NULL };
		execve(path, argv, envp);
		_exit(1);
	}
}

#define TB_H        32
#define TB_BG       FB_RGB(0x00, 0x3C, 0x8C)
#define TB_BTN      FB_RGB(0x00, 0x50, 0xB0)
#define TB_BTN_PRESS FB_RGB(0x00, 0x70, 0xD0)
#define TB_SEP      FB_RGB(0x00, 0x28, 0x68)
#define BTN_W       80
#define CLOCK_W     80

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

static uint32_t  g_sw;
static uint32_t  g_sh;
static uint32_t  g_win_id;
static void     *g_tb_shm  = NULL;

/* Flyout state */
#define FLY_W        120
#define FLY_H         80
#define FLY_BG       FB_RGB(0x00, 0x28, 0x60)
#define FLY_ITEM     FB_RGB(0x00, 0x40, 0x80)
#define FLY_ITEM_H    (FLY_H / 2)

static uint32_t  g_fly_id   = 0;
static void     *g_fly_shm  = NULL;
static int       g_fly_open = 0;

static void
draw_flyout(void)
{
	fb_set_target(g_fly_shm, FLY_W, FLY_H, FLY_W * 4, 32);
	fb_rect(0, 0, FLY_W, FLY_H, FLY_BG);

	fb_rect(2, 2, FLY_W - 4, FLY_ITEM_H - 4, FLY_ITEM);
	fb_text(8, 10, "Terminal", FB_WHITE, FLY_ITEM);

	fb_rect(2, FLY_ITEM_H + 2, FLY_W - 4, FLY_ITEM_H - 4, FLY_ITEM);
	fb_text(8, FLY_ITEM_H + 10, "About", FB_WHITE, FLY_ITEM);

	/* Restore fb target to taskbar buffer */
	fb_set_target(g_tb_shm, g_sw, TB_H, g_sw * 4, 32);
}

static void
send_fly_flip(void)
{
	mpi_message_t msg;
	struct display_flip *fl = (struct display_flip *)msg.data;
	msg.header   = DISPLAY_FLIP;
	fl->window_id = g_fly_id;
	fl->dirty_x   = 0;
	fl->dirty_y   = 0;
	fl->dirty_w   = 0;
	fl->dirty_h   = 0;
	mpi_postMessage("views", DISPLAY_FLIP, &msg);
}

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
	creq->sender_pid  = getpid();
	strncpy(creq->title, "flyout", sizeof(creq->title) - 1);
	creq->title[sizeof(creq->title) - 1] = '\0';
	strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
	creq->reply[sizeof(creq->reply) - 1] = '\0';
	mpi_postMessage("views", DISPLAY_CLAIM, &claim);

	mpi_message_t reply;
	while (mpi_fetchMessage("taskbar", &reply) != 0)
		sched_yield();

	if (reply.header != DISPLAY_ACK)
		return;

	struct display_ack *da = (struct display_ack *)reply.data;
	g_fly_id   = da->window_id;
	g_fly_shm  = da->shm_base;
	g_fly_open = 1;

	draw_flyout();
	send_fly_flip();
}

static void
hide_flyout(void)
{
	if (!g_fly_open)
		return;

	mpi_message_t msg;
	struct display_release *rel = (struct display_release *)msg.data;
	msg.header    = DISPLAY_RELEASE;
	rel->window_id = g_fly_id;
	mpi_postMessage("views", DISPLAY_RELEASE, &msg);

	g_fly_open = 0;
	g_fly_id   = 0;
	g_fly_shm  = NULL;
}

static void
draw_taskbar(int btn_pressed)
{
	/* Background */
	fb_rect(0, 0, (int)g_sw, TB_H, TB_BG);
	/* Top edge separator */
	fb_rect(0, 0, (int)g_sw, 1, TB_SEP);

	/* Launcher button — highlight when pressed */
	uint32_t btn_color = btn_pressed ? TB_BTN_PRESS : TB_BTN;
	fb_rect(2, 2, BTN_W, TB_H - 4, btn_color);
	fb_rect_outline(2, 2, BTN_W, TB_H - 4, TB_SEP);
	fb_text(10, 12, "UbixOS", FB_WHITE, btn_color);

	/* Clock */
	char tstr[12];
	get_time_str(tstr);
	int cx = (int)g_sw - CLOCK_W - 2;
	fb_rect(cx, 2, CLOCK_W, TB_H - 4, TB_BTN);
	fb_rect_outline(cx, 2, CLOCK_W, TB_H - 4, TB_SEP);
	fb_text(cx + 8, 12, tstr, FB_WHITE, TB_BTN);
}

static void
send_flip(void)
{
	mpi_message_t msg;
	struct display_flip *fl = (struct display_flip *)msg.data;
	msg.header   = DISPLAY_FLIP;
	fl->window_id = g_win_id;
	fl->dirty_x   = 0;
	fl->dirty_y   = 0;
	fl->dirty_w   = 0;   /* 0 = full redraw */
	fl->dirty_h   = 0;
	mpi_postMessage("views", DISPLAY_FLIP, &msg);
}

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	if (mpi_createMbox("taskbar") != 0) {
		printf("taskbar: mpi_createMbox failed\n");
		return 1;
	}

	/* Query screen geometry */
	mpi_message_t msg;
	struct display_query *dq = (struct display_query *)msg.data;
	msg.header = DISPLAY_QUERY;
	strncpy(dq->reply, "taskbar", sizeof(dq->reply) - 1);
	dq->reply[sizeof(dq->reply) - 1] = '\0';
	mpi_postMessage("views", DISPLAY_QUERY, &msg);

	mpi_message_t reply;
	while (mpi_fetchMessage("taskbar", &reply) != 0)
		sched_yield();

	if (reply.header != DISPLAY_INFO) {
		printf("taskbar: unexpected reply to DISPLAY_QUERY\n");
		return 1;
	}

	struct display_info *di = (struct display_info *)reply.data;
	uint32_t sw = di->screen_w;
	uint32_t sh = di->screen_h;
	g_sw = sw;
	g_sh = sh;

	/* Claim bottom strip */
	mpi_message_t claim;
	struct display_claim_req *creq = (struct display_claim_req *)claim.data;
	claim.header      = DISPLAY_CLAIM;
	creq->x           = 0;
	creq->y           = (int32_t)(sh - TB_H);
	creq->w           = (int32_t)sw;
	creq->h           = TB_H;
	creq->sender_pid  = getpid();
	strncpy(creq->title, "taskbar", sizeof(creq->title) - 1);
	creq->title[sizeof(creq->title) - 1] = '\0';
	strncpy(creq->reply, "taskbar", sizeof(creq->reply) - 1);
	creq->reply[sizeof(creq->reply) - 1] = '\0';
	mpi_postMessage("views", DISPLAY_CLAIM, &claim);

	while (mpi_fetchMessage("taskbar", &reply) != 0)
		sched_yield();

	if (reply.header != DISPLAY_ACK) {
		printf("taskbar: DISPLAY_CLAIM denied\n");
		return 1;
	}

	struct display_ack *da = (struct display_ack *)reply.data;
	g_win_id = da->window_id;
	void *shm = da->shm_base;
	g_tb_shm  = shm;

	if (!shm) {
		printf("taskbar: shm_base is NULL\n");
		return 1;
	}

	printf("taskbar: window %u at 0x%X, %dx%d+%d+%d\n",
	    g_win_id, (uint32_t)(uintptr_t)shm,
	    da->w, da->h, da->x, da->y);

	fb_set_target(shm, sw, TB_H, sw * 4, 32);

	int btn_pressed = 0;
	draw_taskbar(0);
	send_flip();

	int last_sec = -1;
	for (;;) {
		/* Update clock when the second changes */
		int t = gettime() % 60;
		if (t != last_sec) {
			last_sec = t;
			draw_taskbar(btn_pressed);
			send_flip();
		}

		/* Process incoming events */
		while (mpi_fetchMessage("taskbar", &reply) == 0) {
			if (reply.header == DISPLAY_KEY)
				continue;
			if (reply.header != DISPLAY_MOUSE)
				continue;
			struct display_mouse_ev *me =
			    (struct display_mouse_ev *)reply.data;

			int pressed = (me->buttons & 1) != 0;

			if (me->window_id == g_fly_id) {
				/* Click released inside flyout */
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
				send_flip();

				/* Launcher button: x in [2, 2+BTN_W) */
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
