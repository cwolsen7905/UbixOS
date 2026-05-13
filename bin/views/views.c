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

/* Window table */
#define MAX_WINDOWS  16
#define WIN_BPP       4   /* shared buffers always 32bpp BGRX */

typedef struct {
	uint32_t id;
	int32_t  x, y, w, h;
	uint32_t pitch;
	void    *buf;
	int      active;
	char     mbox[64];
} win_t;

static win_t    windows[MAX_WINDOWS];
static uint32_t next_win_id = 1;

static win_t *
win_alloc(void)
{
	for (int i = 0; i < MAX_WINDOWS; i++)
		if (!windows[i].active) return &windows[i];
	return NULL;
}

static win_t *
win_find(uint32_t id)
{
	for (int i = 0; i < MAX_WINDOWS; i++)
		if (windows[i].active && windows[i].id == id)
			return &windows[i];
	return NULL;
}

/* Jailbar palette */
static const uint32_t jailbar_colors[4] = {
	FB_RGB(0x1A, 0x1A, 0x2E),
	FB_RGB(0x1A, 0x2E, 0x00),
	FB_RGB(0x2E, 0x00, 0x1A),
	FB_RGB(0x00, 0x1A, 0x1A),
};

/* Arrow cursor: 12x12, 1=fg, 0=shadow, 2=transparent */
#define CUR_W  12
#define CUR_H  12
static const uint8_t cursor_mask[CUR_H][CUR_W] = {
	{ 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
};

static struct fb_info fb;

/* Cursor state */
static int      cur_x    = 0;
static int      cur_y    = 0;
static uint32_t cur_saved[CUR_H * CUR_W];
static int      cur_drawn = 0;
static uint8_t  prev_buttons = 0;

static void
desktop_pixel_at(int x, int y, uint32_t *out)
{
	(void)y;
	*out = jailbar_colors[x & 3];
}

static void
cursor_save(int x, int y)
{
	for (int row = 0; row < CUR_H; row++)
		for (int col = 0; col < CUR_W; col++)
			desktop_pixel_at(x + col, y + row,
			    &cur_saved[row * CUR_W + col]);
}

static void
cursor_draw(int x, int y)
{
	for (int row = 0; row < CUR_H; row++) {
		for (int col = 0; col < CUR_W; col++) {
			uint8_t m = cursor_mask[row][col];
			if (m == 2)
				continue;
			uint32_t color = (m == 1) ? FB_WHITE : FB_RGB(0x40,0x40,0x40);
			fb_pixel(x + col, y + row, color);
		}
	}
}

static void
cursor_erase(int x, int y)
{
	for (int row = 0; row < CUR_H; row++)
		for (int col = 0; col < CUR_W; col++)
			fb_pixel(x + col, y + row,
			    cur_saved[row * CUR_W + col]);
}

/* Re-blit any window overlapping the rectangle [rx, rx+rw) x [ry, ry+rh) */
static void
reblit_windows(int rx, int ry, int rw, int rh)
{
	for (int i = 0; i < MAX_WINDOWS; i++) {
		win_t *w = &windows[i];
		if (!w->active) continue;

		int ix  = (rx > w->x) ? rx : w->x;
		int iy  = (ry > w->y) ? ry : w->y;
		int ix2 = (rx + rw < w->x + w->w) ? rx + rw : w->x + w->w;
		int iy2 = (ry + rh < w->y + w->h) ? ry + rh : w->y + w->h;
		int iw  = ix2 - ix;
		int ih  = iy2 - iy;

		if (iw <= 0 || ih <= 0) continue;

		int bx = ix - w->x;
		int by = iy - w->y;
		fb_blit(ix, iy, iw, ih,
		    (const uint32_t *)((uint8_t *)w->buf +
		        (uint32_t)by * w->pitch +
		        (uint32_t)bx * WIN_BPP),
		    (int)(w->pitch / WIN_BPP));
	}
}

static void
cursor_move(int dx, int dy)
{
	int old_x = cur_x, old_y = cur_y;

	if (cur_drawn)
		cursor_erase(old_x, old_y);

	cur_x += dx;
	cur_y += dy;

	if (cur_x < 0) cur_x = 0;
	if (cur_y < 0) cur_y = 0;
	if (cur_x >= (int)fb.width  - CUR_W) cur_x = (int)fb.width  - CUR_W;
	if (cur_y >= (int)fb.height - CUR_H) cur_y = (int)fb.height - CUR_H;

	/* Repair any window pixels the cursor was covering */
	reblit_windows(old_x, old_y, CUR_W, CUR_H);

	cursor_save(cur_x, cur_y);
	cursor_draw(cur_x, cur_y);
	cur_drawn = 1;
}

static void
draw_desktop(void)
{
	for (int y = 0; y < (int)fb.height; y++)
		for (int x = 0; x < (int)fb.width; x++)
			fb_pixel(x, y, jailbar_colors[x & 3]);
}

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	if (mpi_createMbox("views") != 0) {
		printf("views: mpi_createMbox failed\n");
		return 1;
	}

	mpi_message_t req;
	req.header = 0x82;
	req.data[0] = 'v'; req.data[1] = 'i'; req.data[2] = 'e';
	req.data[3] = 'w'; req.data[4] = 's'; req.data[5] = '\0';
	mpi_postMessage("system", 0x82, &req);

	mpi_message_t reply;
	while (mpi_fetchMessage("views", &reply) != 0)
		sched_yield();

	if (reply.data[0] == 0) {
		printf("views: VESA init failed\n");
		return 1;
	}

	if (fb_open(&fb) != 0) {
		printf("views: fb_open failed\n");
		return 1;
	}

	draw_desktop();

	/* Place cursor in centre */
	cur_x = (int)fb.width  / 2;
	cur_y = (int)fb.height / 2;
	cursor_save(cur_x, cur_y);
	cursor_draw(cur_x, cur_y);
	cur_drawn = 1;

	/* Launch taskbar as a child process */
	{
		int pid = fork();
		if (pid == 0) {
			char *argv_tb[] = { "taskbar", NULL };
			char *envp_tb[] = { NULL };
			execve("sys:/bin/taskbar", argv_tb, envp_tb);
			printf("views: failed to exec taskbar\n");
			exit(1);
		}
	}

	mpi_message_t msg;

	for (;;) {
		/* Drain all pending mouse events */
		mouse_event_t ev;
		while (fb_poll_mouse(&ev) == 0) {
			cursor_move(ev.dx, ev.dy);

			if (ev.buttons != prev_buttons) {
				/* Hit-test: find the window under the cursor */
				for (int i = 0; i < MAX_WINDOWS; i++) {
					win_t *w = &windows[i];
					if (!w->active || !w->mbox[0]) continue;
					if (cur_x < w->x || cur_x >= w->x + w->w)
						continue;
					if (cur_y < w->y || cur_y >= w->y + w->h)
						continue;
					mpi_message_t mev;
					struct display_mouse_ev *me =
					    (struct display_mouse_ev *)mev.data;
					mev.header    = DISPLAY_MOUSE;
					me->window_id = w->id;
					me->x  = (int16_t)(cur_x - w->x);
					me->y  = (int16_t)(cur_y - w->y);
					me->dx = 0;
					me->dy = 0;
					me->buttons = ev.buttons;
					mpi_postMessage(w->mbox, DISPLAY_MOUSE, &mev);
					break;
				}
				prev_buttons = ev.buttons;
			}
		}

		/* Dispatch pending MPI messages */
		while (mpi_fetchMessage("views", &msg) == 0) {
			switch (msg.header) {

			case DISPLAY_QUERY: {
				struct display_query *dq =
				    (struct display_query *)msg.data;
				if (!dq->reply[0]) break;
				mpi_message_t info;
				struct display_info *di =
				    (struct display_info *)info.data;
				info.header   = DISPLAY_INFO;
				di->screen_w  = fb.width;
				di->screen_h  = fb.height;
				di->bpp       = fb.bpp;
				mpi_postMessage(dq->reply, DISPLAY_INFO, &info);
				break;
			}

			case DISPLAY_CLAIM: {
				struct display_claim_req *creq =
				    (struct display_claim_req *)msg.data;
				mpi_message_t ack;
				struct display_ack *da =
				    (struct display_ack *)ack.data;

				win_t *w = win_alloc();
				if (!w || creq->sender_pid <= 0 ||
				    creq->w <= 0 || creq->h <= 0) {
					ack.header = DISPLAY_DENIED;
					if (creq->reply[0])
						mpi_postMessage(creq->reply,
						    DISPLAY_DENIED, &ack);
					break;
				}

				int32_t wx = creq->x, wy = creq->y;
				int32_t ww = creq->w, wh = creq->h;
				if (wx < 0) wx = 0;
				if (wy < 0) wy = 0;
				if (wx + ww > (int32_t)fb.width)
					ww = (int32_t)fb.width - wx;
				if (wy + wh > (int32_t)fb.height)
					wh = (int32_t)fb.height - wy;

				uint32_t pitch    = (uint32_t)ww * WIN_BPP;
				uint32_t buf_size = pitch * (uint32_t)wh;
				void *buf = malloc(buf_size);
				if (!buf) {
					ack.header = DISPLAY_DENIED;
					if (creq->reply[0])
						mpi_postMessage(creq->reply,
						    DISPLAY_DENIED, &ack);
					break;
				}
				memset(buf, 0, buf_size);

				uint32_t client_vaddr = 0;
				if (fb_share_buffer(creq->sender_pid, buf,
				    buf_size, &client_vaddr) != 0) {
					free(buf);
					ack.header = DISPLAY_DENIED;
					if (creq->reply[0])
						mpi_postMessage(creq->reply,
						    DISPLAY_DENIED, &ack);
					break;
				}

				w->id     = next_win_id++;
				w->x      = wx; w->y  = wy;
				w->w      = ww; w->h  = wh;
				w->pitch  = pitch;
				w->buf    = buf;
				w->active = 1;
				strncpy(w->mbox, creq->reply, sizeof(w->mbox) - 1);
				w->mbox[sizeof(w->mbox) - 1] = '\0';

				ack.header    = DISPLAY_ACK;
				da->window_id = w->id;
				da->shm_base  = (void *)client_vaddr;
				da->pitch     = (uint16_t)pitch;
				da->x = wx; da->y = wy;
				da->w = ww; da->h = wh;
				if (creq->reply[0])
					mpi_postMessage(creq->reply,
					    DISPLAY_ACK, &ack);

				printf("views: window %u pid %d "
				    "%dx%d+%d+%d shm=0x%X\n",
				    w->id, creq->sender_pid,
				    ww, wh, wx, wy, client_vaddr);
				break;
			}

			case DISPLAY_FLIP: {
				struct display_flip *fl =
				    (struct display_flip *)msg.data;
				win_t *w = win_find(fl->window_id);
				if (!w) break;

				int32_t dx = fl->dirty_x, dy = fl->dirty_y;
				int32_t dw = fl->dirty_w, dh = fl->dirty_h;
				if (dw <= 0 || dh <= 0) {
					dx = 0; dy = 0;
					dw = w->w; dh = w->h;
				}
				fb_blit(w->x + dx, w->y + dy, dw, dh,
				    (const uint32_t *)((uint8_t *)w->buf +
				        (uint32_t)dy * w->pitch +
				        (uint32_t)dx * WIN_BPP),
				    (int)(w->pitch / WIN_BPP));
				break;
			}

			case DISPLAY_RELEASE: {
				struct display_release *rel =
				    (struct display_release *)msg.data;
				win_t *w = win_find(rel->window_id);
				if (w) {
					int rx = w->x, ry = w->y;
					int rw = w->w, rh = w->h;
					free(w->buf);
					w->active = 0;

					/* Repaint desktop then surviving windows */
					for (int y = ry; y < ry + rh; y++)
						for (int x = rx; x < rx + rw; x++)
							fb_pixel(x, y,
							    jailbar_colors[x & 3]);
					reblit_windows(rx, ry, rw, rh);
				}
				break;
			}

			default:
				break;
			}
		}

		sched_yield();
	}

	return 0;
}
