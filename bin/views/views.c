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

/* ------------------------------------------------------------------ */
/* Window table                                                         */
/* ------------------------------------------------------------------ */

#define MAX_WINDOWS  16
#define WIN_BPP       4   /* shared buffers always 32bpp BGRX */

/* Decoration colours — match the term title bar palette */
#define DECOR_BG        FB_RGB(0x28, 0x48, 0x70)
#define DECOR_HI        FB_RGB(0x40, 0x70, 0xA8)
#define DECOR_SEP       FB_RGB(0x10, 0x20, 0x30)
#define DECOR_CLOSE_BG  FB_RGB(0x90, 0x22, 0x22)

typedef struct {
	uint32_t id;
	int32_t  x, y, w, h;   /* content position and size */
	uint32_t pitch;
	void    *buf;
	int      active;
	int      decor_h;       /* 0 = no decoration, DECOR_H = has title bar */
	char     title[64];
	char     mbox[64];
} win_t;

static win_t    windows[MAX_WINDOWS];
static uint32_t next_win_id = 1;

/* Z-order stack: z_stack[0] = bottom, z_stack[z_count-1] = top */
static win_t   *z_stack[MAX_WINDOWS];
static int      z_count = 0;

static win_t   *focused_win = NULL;

/* Drag state */
static int      dragging   = 0;
static win_t   *drag_win   = NULL;
static int      drag_off_x = 0;
static int      drag_off_y = 0;

static struct fb_info fb;

/* ------------------------------------------------------------------ */
/* Z-stack helpers                                                      */
/* ------------------------------------------------------------------ */

static void
z_push(win_t *w)
{
	if (z_count < MAX_WINDOWS)
		z_stack[z_count++] = w;
}

static void
z_remove(win_t *w)
{
	for (int i = 0; i < z_count; i++) {
		if (z_stack[i] == w) {
			for (int j = i; j < z_count - 1; j++)
				z_stack[j] = z_stack[j + 1];
			z_count--;
			return;
		}
	}
}

static void
z_raise(win_t *w)
{
	z_remove(w);
	z_push(w);
}

/* ------------------------------------------------------------------ */
/* Window table helpers                                                 */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Desktop background                                                   */
/* ------------------------------------------------------------------ */

static const uint32_t jailbar_colors[4] = {
	FB_RGB(0x1A, 0x1A, 0x2E),
	FB_RGB(0x1A, 0x2E, 0x00),
	FB_RGB(0x2E, 0x00, 0x1A),
	FB_RGB(0x00, 0x1A, 0x1A),
};

static void
desktop_fill_rect(int x, int y, int w, int h)
{
	int x2 = x + w, y2 = y + h;
	if (x  < 0) x  = 0;
	if (y  < 0) y  = 0;
	if (x2 > (int)fb.width)  x2 = (int)fb.width;
	if (y2 > (int)fb.height) y2 = (int)fb.height;
	for (int py = y; py < y2; py++)
		for (int px = x; px < x2; px++)
			fb_pixel(px, py, jailbar_colors[px & 3]);
}

static void
draw_desktop(void)
{
	desktop_fill_rect(0, 0, (int)fb.width, (int)fb.height);
}

/* ------------------------------------------------------------------ */
/* Server-side window decorations                                       */
/* ------------------------------------------------------------------ */

static void
draw_decor(win_t *w)
{
	int wx = w->x, wy = w->y, ww = w->w, dh = w->decor_h;

	fb_rect(wx, wy,          ww, dh, DECOR_BG);
	fb_rect(wx, wy,          ww, 1,  DECOR_HI);
	fb_rect(wx, wy + dh - 1, ww, 1,  DECOR_SEP);

	fb_text(wx + 6, wy + (dh - FB_FONT_H) / 2,
	    w->title, FB_WHITE, DECOR_BG);

	/* Close button — square on the right */
	int cx = wx + ww - dh;
	fb_rect(cx, wy + 1, dh - 1, dh - 2, DECOR_CLOSE_BG);
	fb_char(cx + (dh - FB_FONT_W) / 2, wy + (dh - FB_FONT_H) / 2,
	    'X', FB_WHITE, DECOR_CLOSE_BG);
}

/* ------------------------------------------------------------------ */
/* Cursor                                                               */
/* ------------------------------------------------------------------ */

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

static int      cur_x     = 0;
static int      cur_y     = 0;
static uint32_t cur_saved[CUR_H * CUR_W];
static int      cur_drawn = 0;
static uint8_t  prev_buttons = 0;

/* Read the pixels currently on the framebuffer under the cursor */
static void
cursor_save(int x, int y)
{
	for (int row = 0; row < CUR_H; row++) {
		if (y + row < 0 || y + row >= (int)fb.height) continue;
		const uint32_t *src = (const uint32_t *)
		    ((const uint8_t *)fb.base +
		    (uint32_t)(y + row) * fb.pitch);
		for (int col = 0; col < CUR_W; col++) {
			int px = x + col;
			cur_saved[row * CUR_W + col] =
			    (px >= 0 && px < (int)fb.width) ? src[px] : 0;
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

static void
cursor_draw(int x, int y)
{
	for (int row = 0; row < CUR_H; row++) {
		for (int col = 0; col < CUR_W; col++) {
			uint8_t m = cursor_mask[row][col];
			if (m == 2)
				continue;
			uint32_t color = (m == 1)
			    ? FB_WHITE
			    : FB_RGB(0x40, 0x40, 0x40);
			fb_pixel(x + col, y + row, color);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Compositing                                                          */
/* ------------------------------------------------------------------ */

/*
 * Blit all window content (and decorations) that overlap [rx,ry,rw,rh].
 * Used for cursor move repair — avoids a full desktop repaint.
 */
static void
reblit_rect(int rx, int ry, int rw, int rh)
{
	for (int i = 0; i < z_count; i++) {
		win_t *w = z_stack[i];
		int cy = w->y + w->decor_h;

		/* Content intersection */
		int x1 = (rx     > w->x)       ? rx     : w->x;
		int y1 = (ry     > cy)          ? ry     : cy;
		int x2 = (rx+rw  < w->x + w->w) ? rx+rw  : w->x + w->w;
		int y2 = (ry+rh  < cy  + w->h)  ? ry+rh  : cy   + w->h;
		if (x2 > x1 && y2 > y1) {
			int bx = x1 - w->x;
			int by = y1 - cy;
			fb_blit(x1, y1, x2 - x1, y2 - y1,
			    (const uint32_t *)((const uint8_t *)w->buf +
			        (uint32_t)by * w->pitch +
			        (uint32_t)bx * WIN_BPP),
			    (int)(w->pitch / WIN_BPP));
		}

		/* Decoration intersection — redraw the full bar if any overlap */
		if (w->decor_h > 0 &&
		    rx < w->x + w->w && rx + rw > w->x &&
		    ry < w->y + w->decor_h && ry + rh > w->y)
			draw_decor(w);
	}
}

/*
 * Full recomposite: desktop → windows (bottom to top) → cursor.
 * Called after DISPLAY_FLIP, drag moves, window raise/close.
 */
static void
composite_all(void)
{
	draw_desktop();
	for (int i = 0; i < z_count; i++) {
		win_t *w = z_stack[i];
		fb_blit(w->x, w->y + w->decor_h, w->w, w->h,
		    (const uint32_t *)w->buf,
		    (int)(w->pitch / WIN_BPP));
		if (w->decor_h > 0)
			draw_decor(w);
	}
	cursor_save(cur_x, cur_y);
	cursor_draw(cur_x, cur_y);
	cur_drawn = 1;
}

/* ------------------------------------------------------------------ */
/* Cursor movement                                                      */
/* ------------------------------------------------------------------ */

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

	/* Repaint the vacated cursor area from desktop + windows */
	desktop_fill_rect(old_x, old_y, CUR_W, CUR_H);
	reblit_rect(old_x, old_y, CUR_W, CUR_H);

	cursor_save(cur_x, cur_y);
	cursor_draw(cur_x, cur_y);
	cur_drawn = 1;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

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

	cur_x = (int)fb.width  / 2;
	cur_y = (int)fb.height / 2;
	cursor_save(cur_x, cur_y);
	cursor_draw(cur_x, cur_y);
	cur_drawn = 1;

	/* Launch taskbar */
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
		/* Forward keyboard events to focused window */
		kbd_event_t kev;
		while (fb_poll_kbd(&kev) == 0) {
			if (focused_win && focused_win->mbox[0]) {
				mpi_message_t kmsg;
				struct display_key *dk =
				    (struct display_key *)kmsg.data;
				kmsg.header   = DISPLAY_KEY;
				dk->window_id = focused_win->id;
				dk->keycode   = kev.keycode;
				dk->pressed   = kev.pressed;
				mpi_postMessage(focused_win->mbox,
				    DISPLAY_KEY, &kmsg);
			}
		}

		/* Drain mouse events */
		mouse_event_t ev;
		while (fb_poll_mouse(&ev) == 0) {
			cursor_move(ev.dx, ev.dy);

			/* Update window position while dragging */
			if (dragging && drag_win && (ev.dx || ev.dy)) {
				drag_win->x = cur_x - drag_off_x;
				drag_win->y = cur_y - drag_off_y;
				if (drag_win->x < 0) drag_win->x = 0;
				if (drag_win->y < 0) drag_win->y = 0;
				composite_all();
			}

			if (ev.buttons != prev_buttons) {
				int pressed = (ev.buttons & 1) != 0;

				/* Release ends drag */
				if (!pressed && dragging) {
					dragging = 0;
					drag_win = NULL;
				}

				if (pressed) {
					/* Hit test: topmost window under cursor */
					win_t *hit = NULL;
					for (int i = z_count - 1; i >= 0; i--) {
						win_t *w = z_stack[i];
						if (cur_x < w->x ||
						    cur_x >= w->x + w->w)
							continue;
						if (cur_y < w->y ||
						    cur_y >= w->y + w->decor_h + w->h)
							continue;
						hit = w;
						break;
					}

					if (hit) {
						z_raise(hit);
						focused_win = hit;
						composite_all();

						int rel_y = cur_y - hit->y;
						if (hit->decor_h > 0 &&
						    rel_y < hit->decor_h) {
							int rel_x = cur_x - hit->x;
							if (rel_x >= hit->w - hit->decor_h) {
								/* Close button */
								if (hit->mbox[0]) {
									mpi_message_t cm;
									struct display_close *dc =
									    (struct display_close *)cm.data;
									cm.header     = DISPLAY_CLOSE;
									dc->window_id = hit->id;
									mpi_postMessage(hit->mbox,
									    DISPLAY_CLOSE, &cm);
								}
								free(hit->buf);
								hit->active = 0;
								z_remove(hit);
								focused_win = (z_count > 0)
								    ? z_stack[z_count - 1]
								    : NULL;
								composite_all();
							} else {
								/* Start drag */
								dragging   = 1;
								drag_win   = hit;
								drag_off_x = cur_x - hit->x;
								drag_off_y = cur_y - hit->y;
							}
						} else {
							/* Content area — send to client */
							if (hit->mbox[0]) {
								mpi_message_t mev;
								struct display_mouse_ev *me =
								    (struct display_mouse_ev *)mev.data;
								mev.header    = DISPLAY_MOUSE;
								me->window_id = hit->id;
								me->x  = (int16_t)(cur_x - hit->x);
								me->y  = (int16_t)(cur_y -
								    (hit->y + hit->decor_h));
								me->dx = 0;
								me->dy = 0;
								me->buttons = ev.buttons;
								mpi_postMessage(hit->mbox,
								    DISPLAY_MOUSE, &mev);
							}
						}
					}
				} else {
					/* Button release — forward to focused window */
					if (focused_win && focused_win->mbox[0]) {
						int rel_y = cur_y -
						    (focused_win->y + focused_win->decor_h);
						mpi_message_t mev;
						struct display_mouse_ev *me =
						    (struct display_mouse_ev *)mev.data;
						mev.header    = DISPLAY_MOUSE;
						me->window_id = focused_win->id;
						me->x  = (int16_t)(cur_x - focused_win->x);
						me->y  = (int16_t)rel_y;
						me->dx = 0;
						me->dy = 0;
						me->buttons = ev.buttons;
						mpi_postMessage(focused_win->mbox,
						    DISPLAY_MOUSE, &mev);
					}
				}

				prev_buttons = ev.buttons;
			}
		}

		/* Dispatch MPI messages */
		while (mpi_fetchMessage("views", &msg) == 0) {
			switch (msg.header) {

			case DISPLAY_QUERY: {
				struct display_query *dq =
				    (struct display_query *)msg.data;
				if (!dq->reply[0]) break;
				mpi_message_t info;
				struct display_info *di =
				    (struct display_info *)info.data;
				info.header  = DISPLAY_INFO;
				di->screen_w = fb.width;
				di->screen_h = fb.height;
				di->bpp      = fb.bpp;
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

				int dh = creq->no_decor ? 0 : DECOR_H;

				int32_t wx = creq->x, wy = creq->y;
				int32_t ww = creq->w, wh = creq->h;
				if (wx < 0) wx = 0;
				if (wy < 0) wy = 0;
				if (wx + ww > (int32_t)fb.width)
					ww = (int32_t)fb.width - wx;
				if (wy + dh + wh > (int32_t)fb.height)
					wh = (int32_t)fb.height - wy - dh;

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

				w->id      = next_win_id++;
				w->x       = wx;  w->y  = wy;
				w->w       = ww;  w->h  = wh;
				w->pitch   = pitch;
				w->buf     = buf;
				w->active  = 1;
				w->decor_h = dh;
				strncpy(w->title, creq->title,
				    sizeof(w->title) - 1);
				w->title[sizeof(w->title) - 1] = '\0';
				strncpy(w->mbox, creq->reply,
				    sizeof(w->mbox) - 1);
				w->mbox[sizeof(w->mbox) - 1] = '\0';

				z_push(w);
				focused_win = w;

				ack.header    = DISPLAY_ACK;
				da->window_id = w->id;
				da->shm_base  = (void *)client_vaddr;
				da->pitch     = (uint16_t)pitch;
				da->x = wx;  da->y = wy + dh;
				da->w = ww;  da->h = wh;
				if (creq->reply[0])
					mpi_postMessage(creq->reply,
					    DISPLAY_ACK, &ack);

				printf("views: window %u pid %d "
				    "%dx%d+%d+%d shm=0x%X\n",
				    w->id, creq->sender_pid,
				    ww, wh, wx, wy, client_vaddr);

				composite_all();
				break;
			}

			case DISPLAY_FLIP: {
				struct display_flip *fl =
				    (struct display_flip *)msg.data;
				win_t *w = win_find(fl->window_id);
				if (!w) break;
				composite_all();
				break;
			}

			case DISPLAY_RELEASE: {
				struct display_release *rel =
				    (struct display_release *)msg.data;
				win_t *w = win_find(rel->window_id);
				if (w) {
					free(w->buf);
					w->active = 0;
					z_remove(w);
					if (focused_win == w)
						focused_win = (z_count > 0)
						    ? z_stack[z_count - 1]
						    : NULL;
					composite_all();
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
