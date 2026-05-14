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
#include <fb/fb.h>
#include <views/display_proto.h>
}

#define MAX_WINDOWS  16
#define WIN_BPP       4   /* shared buffers always 32bpp BGRX */

/* Decoration colours */
#define DECOR_BG        FB_RGB(0x28, 0x48, 0x70)
#define DECOR_HI        FB_RGB(0x40, 0x70, 0xA8)
#define DECOR_SEP       FB_RGB(0x10, 0x20, 0x30)
#define DECOR_CLOSE_BG  FB_RGB(0x90, 0x22, 0x22)

#define CUR_W  12
#define CUR_H  12

/* ------------------------------------------------------------------ */
/* Framebuffer — wraps libfb; replaced by native impl in Phase 10b     */
/* ------------------------------------------------------------------ */

class Framebuffer {
public:
	uint32_t  width, height, pitch, bpp;
	void     *base;

	int open() {
		struct fb_info info;
		if (fb_open(&info) != 0)
			return -1;
		width  = info.width;
		height = info.height;
		pitch  = info.pitch;
		bpp    = info.bpp;
		base   = info.base;
		return 0;
	}

	void pixel(int x, int y, uint32_t color) {
		fb_pixel(x, y, color);
	}

	void rect(int x, int y, int w, int h, uint32_t color) {
		fb_rect(x, y, w, h, color);
	}

	void blit(int x, int y, int w, int h,
	    const uint32_t *src, int src_stride) {
		fb_blit(x, y, w, h, src, src_stride);
	}

	void text(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
		fb_text(x, y, s, fg, bg);
	}

	void ch(int x, int y, char c, uint32_t fg, uint32_t bg) {
		fb_char(x, y, c, fg, bg);
	}

	uint32_t read(int x, int y) const {
		if (x < 0 || y < 0 ||
		    (uint32_t)x >= width || (uint32_t)y >= height)
			return 0;
		const uint32_t *row = (const uint32_t *)
		    ((const uint8_t *)base + (uint32_t)y * pitch);
		return row[x];
	}
};

/* ------------------------------------------------------------------ */
/* Window                                                               */
/* ------------------------------------------------------------------ */

class Window {
public:
	uint32_t  id;
	int32_t   x, y, w, h;
	uint32_t  pitch;
	void     *buf;
	bool      active;
	int       decor_h;
	char      title[64];
	char      mbox[64];

	bool hit_test(int cx, int cy) const {
		return cx >= x && cx < x + w &&
		       cy >= y && cy < y + decor_h + h;
	}

	bool in_decor(int cx, int cy) const {
		return decor_h > 0 &&
		       cx >= x && cx < x + w &&
		       cy >= y && cy < y + decor_h;
	}

	bool in_close_btn(int cx, int cy) const {
		return in_decor(cx, cy) && cx >= x + w - decor_h;
	}

	void draw_decor(Framebuffer &fb) const {
		fb.rect(x, y,              w, decor_h,     DECOR_BG);
		fb.rect(x, y,              w, 1,            DECOR_HI);
		fb.rect(x, y + decor_h - 1, w, 1,          DECOR_SEP);
		fb.text(x + 6, y + (decor_h - FB_FONT_H) / 2,
		    title, FB_WHITE, DECOR_BG);
		int cbx = x + w - decor_h;
		fb.rect(cbx, y + 1, decor_h - 1, decor_h - 2, DECOR_CLOSE_BG);
		fb.ch(cbx + (decor_h - FB_FONT_W) / 2,
		    y + (decor_h - FB_FONT_H) / 2,
		    'X', FB_WHITE, DECOR_CLOSE_BG);
	}

	void blit_to(Framebuffer &fb) const {
		fb.blit(x, y + decor_h, w, h,
		    (const uint32_t *)buf,
		    (int)(pitch / WIN_BPP));
	}
};

/* ------------------------------------------------------------------ */
/* WindowManager                                                        */
/* ------------------------------------------------------------------ */

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

static const uint32_t jailbar_colors[4] = {
	FB_RGB(0x1A, 0x1A, 0x2E),
	FB_RGB(0x1A, 0x2E, 0x00),
	FB_RGB(0x2E, 0x00, 0x1A),
	FB_RGB(0x00, 0x1A, 0x1A),
};

class WindowManager {
	Framebuffer  fb;
	Window       windows[MAX_WINDOWS];
	uint32_t     next_win_id;

	Window      *z_stack[MAX_WINDOWS];
	int          z_count;
	Window      *focused;

	bool         dragging;
	Window      *drag_win;
	int          drag_off_x, drag_off_y;

	int          cur_x, cur_y;
	uint32_t     cur_saved[CUR_H * CUR_W];
	bool         cur_drawn;
	uint8_t      prev_buttons;

	/* -- Z-order ---------------------------------------------------- */

	void z_push(Window *w) {
		if (z_count < MAX_WINDOWS)
			z_stack[z_count++] = w;
	}

	void z_remove(Window *w) {
		for (int i = 0; i < z_count; i++) {
			if (z_stack[i] != w)
				continue;
			for (int j = i; j < z_count - 1; j++)
				z_stack[j] = z_stack[j + 1];
			z_count--;
			return;
		}
	}

	void z_raise(Window *w) { z_remove(w); z_push(w); }

	/* -- Window table ---------------------------------------------- */

	Window *win_alloc() {
		for (int i = 0; i < MAX_WINDOWS; i++)
			if (!windows[i].active)
				return &windows[i];
		return NULL;
	}

	Window *win_find(uint32_t id) {
		for (int i = 0; i < MAX_WINDOWS; i++)
			if (windows[i].active && windows[i].id == id)
				return &windows[i];
		return NULL;
	}

	/* -- Desktop ---------------------------------------------------- */

	void desktop_fill_rect(int x, int y, int w, int h) {
		int x2 = x + w, y2 = y + h;
		if (x  < 0) x  = 0;
		if (y  < 0) y  = 0;
		if (x2 > (int)fb.width)  x2 = (int)fb.width;
		if (y2 > (int)fb.height) y2 = (int)fb.height;
		for (int py = y; py < y2; py++)
			for (int px = x; px < x2; px++)
				fb.pixel(px, py, jailbar_colors[px & 3]);
	}

	void draw_desktop() {
		desktop_fill_rect(0, 0, (int)fb.width, (int)fb.height);
	}

	/* -- Compositing ------------------------------------------------ */

	void reblit_rect(int rx, int ry, int rw, int rh) {
		for (int i = 0; i < z_count; i++) {
			Window *w = z_stack[i];
			int cy = w->y + w->decor_h;

			int x1 = (rx    > w->x)       ? rx    : w->x;
			int y1 = (ry    > cy)          ? ry    : cy;
			int x2 = (rx+rw < w->x + w->w) ? rx+rw : w->x + w->w;
			int y2 = (ry+rh < cy  + w->h)  ? ry+rh : cy   + w->h;
			if (x2 > x1 && y2 > y1) {
				int bx = x1 - w->x;
				int by = y1 - cy;
				fb.blit(x1, y1, x2 - x1, y2 - y1,
				    (const uint32_t *)((const uint8_t *)w->buf +
				        (uint32_t)by * w->pitch +
				        (uint32_t)bx * WIN_BPP),
				    (int)(w->pitch / WIN_BPP));
			}

			if (w->decor_h > 0 &&
			    rx < w->x + w->w && rx + rw > w->x &&
			    ry < w->y + w->decor_h && ry + rh > w->y)
				w->draw_decor(fb);
		}
	}

	/* -- Cursor ----------------------------------------------------- */

	void cursor_save(int x, int y) {
		for (int row = 0; row < CUR_H; row++) {
			if (y + row < 0 || y + row >= (int)fb.height)
				continue;
			for (int col = 0; col < CUR_W; col++)
				cur_saved[row * CUR_W + col] =
				    fb.read(x + col, y + row);
		}
	}

	void cursor_erase(int x, int y) {
		for (int row = 0; row < CUR_H; row++)
			for (int col = 0; col < CUR_W; col++)
				fb.pixel(x + col, y + row,
				    cur_saved[row * CUR_W + col]);
	}

	void cursor_draw(int x, int y) {
		for (int row = 0; row < CUR_H; row++) {
			for (int col = 0; col < CUR_W; col++) {
				uint8_t m = cursor_mask[row][col];
				if (m == 2)
					continue;
				uint32_t color = (m == 1)
				    ? FB_WHITE
				    : FB_RGB(0x40, 0x40, 0x40);
				fb.pixel(x + col, y + row, color);
			}
		}
	}

	/* -- Mouse helper ---------------------------------------------- */

	void send_mouse(Window *w, int cx, int cy, uint8_t buttons) {
		if (!w->mbox[0])
			return;
		mpi_message_t mev;
		struct display_mouse_ev *me =
		    (struct display_mouse_ev *)mev.data;
		mev.header    = DISPLAY_MOUSE;
		me->window_id = w->id;
		me->x         = (int16_t)(cx - w->x);
		me->y         = (int16_t)(cy - (w->y + w->decor_h));
		me->dx        = 0;
		me->dy        = 0;
		me->buttons   = buttons;
		mpi_postMessage(w->mbox, DISPLAY_MOUSE, &mev);
	}

	void close_window(Window *w) {
		if (w->mbox[0]) {
			mpi_message_t cm;
			struct display_close *dc =
			    (struct display_close *)cm.data;
			cm.header     = DISPLAY_CLOSE;
			dc->window_id = w->id;
			mpi_postMessage(w->mbox, DISPLAY_CLOSE, &cm);
		}
		free(w->buf);
		w->active = false;
		z_remove(w);
		focused = (z_count > 0) ? z_stack[z_count - 1] : NULL;
		composite_all();
	}

public:
	WindowManager()
	    : next_win_id(1), z_count(0), focused(NULL),
	      dragging(false), drag_win(NULL),
	      drag_off_x(0), drag_off_y(0),
	      cur_x(0), cur_y(0), cur_drawn(false), prev_buttons(0)
	{
		for (int i = 0; i < MAX_WINDOWS; i++) {
			windows[i].active = false;
			z_stack[i] = NULL;
		}
	}

	int init() { return fb.open(); }

	void startup() {
		draw_desktop();
		cur_x = (int)fb.width  / 2;
		cur_y = (int)fb.height / 2;
		cursor_save(cur_x, cur_y);
		cursor_draw(cur_x, cur_y);
		cur_drawn = true;
	}

	void composite_all() {
		draw_desktop();
		for (int i = 0; i < z_count; i++) {
			z_stack[i]->blit_to(fb);
			if (z_stack[i]->decor_h > 0)
				z_stack[i]->draw_decor(fb);
		}
		cursor_save(cur_x, cur_y);
		cursor_draw(cur_x, cur_y);
		cur_drawn = true;
	}

	void cursor_move(int dx, int dy) {
		int old_x = cur_x, old_y = cur_y;

		if (cur_drawn)
			cursor_erase(old_x, old_y);

		cur_x += dx;
		cur_y += dy;
		if (cur_x < 0) cur_x = 0;
		if (cur_y < 0) cur_y = 0;
		if (cur_x >= (int)fb.width  - CUR_W)
			cur_x = (int)fb.width  - CUR_W;
		if (cur_y >= (int)fb.height - CUR_H)
			cur_y = (int)fb.height - CUR_H;

		desktop_fill_rect(old_x, old_y, CUR_W, CUR_H);
		reblit_rect(old_x, old_y, CUR_W, CUR_H);

		cursor_save(cur_x, cur_y);
		cursor_draw(cur_x, cur_y);
		cur_drawn = true;
	}

	void handle_query(struct display_query *dq) {
		if (!dq->reply[0])
			return;
		mpi_message_t info;
		struct display_info *di = (struct display_info *)info.data;
		info.header  = DISPLAY_INFO;
		di->screen_w = fb.width;
		di->screen_h = fb.height;
		di->bpp      = (uint8_t)fb.bpp;
		mpi_postMessage(dq->reply, DISPLAY_INFO, &info);
	}

	void handle_claim(struct display_claim_req *creq) {
		mpi_message_t ack;
		struct display_ack *da = (struct display_ack *)ack.data;

		auto deny = [&]() {
			ack.header = DISPLAY_DENIED;
			if (creq->reply[0])
				mpi_postMessage(creq->reply,
				    DISPLAY_DENIED, &ack);
		};

		Window *w = win_alloc();
		if (!w || creq->sender_pid <= 0 ||
		    creq->w <= 0 || creq->h <= 0) {
			deny(); return;
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
		if (!buf) { deny(); return; }
		memset(buf, 0, buf_size);

		uint32_t client_vaddr = 0;
		if (fb_share_buffer(creq->sender_pid, buf,
		    buf_size, &client_vaddr) != 0) {
			free(buf); deny(); return;
		}

		w->id      = next_win_id++;
		w->x       = wx;   w->y = wy;
		w->w       = ww;   w->h = wh;
		w->pitch   = pitch;
		w->buf     = buf;
		w->active  = true;
		w->decor_h = dh;
		strncpy(w->title, creq->title, sizeof(w->title) - 1);
		w->title[sizeof(w->title) - 1] = '\0';
		strncpy(w->mbox, creq->reply,  sizeof(w->mbox)  - 1);
		w->mbox[sizeof(w->mbox) - 1]   = '\0';

		z_push(w);
		focused = w;

		ack.header    = DISPLAY_ACK;
		da->window_id = w->id;
		da->shm_base  = (void *)client_vaddr;
		da->pitch     = (uint16_t)pitch;
		da->x = wx;  da->y = wy + dh;
		da->w = ww;  da->h = wh;
		if (creq->reply[0])
			mpi_postMessage(creq->reply, DISPLAY_ACK, &ack);

		printf("views: window %u pid %d %dx%d+%d+%d shm=0x%X\n",
		    w->id, creq->sender_pid, ww, wh, wx, wy, client_vaddr);

		composite_all();
	}

	void handle_flip(struct display_flip *fl) {
		if (win_find(fl->window_id))
			composite_all();
	}

	void handle_release(struct display_release *rel) {
		Window *w = win_find(rel->window_id);
		if (!w) return;
		free(w->buf);
		w->active = false;
		z_remove(w);
		if (focused == w)
			focused = (z_count > 0) ? z_stack[z_count - 1] : NULL;
		composite_all();
	}

	void handle_mouse(mouse_event_t &ev) {
		cursor_move(ev.dx, ev.dy);

		if (dragging && drag_win && (ev.dx || ev.dy)) {
			drag_win->x = cur_x - drag_off_x;
			drag_win->y = cur_y - drag_off_y;
			if (drag_win->x < 0) drag_win->x = 0;
			if (drag_win->y < 0) drag_win->y = 0;
			composite_all();
		}

		if (ev.buttons == prev_buttons)
			return;

		bool pressed = (ev.buttons & 1) != 0;

		if (!pressed && dragging) {
			dragging = false;
			drag_win = NULL;
		}

		if (pressed) {
			Window *hit = NULL;
			for (int i = z_count - 1; i >= 0; i--) {
				if (z_stack[i]->hit_test(cur_x, cur_y)) {
					hit = z_stack[i];
					break;
				}
			}
			if (hit) {
				z_raise(hit);
				focused = hit;
				composite_all();

				if (hit->in_close_btn(cur_x, cur_y)) {
					close_window(hit);
				} else if (hit->in_decor(cur_x, cur_y)) {
					dragging   = true;
					drag_win   = hit;
					drag_off_x = cur_x - hit->x;
					drag_off_y = cur_y - hit->y;
				} else {
					send_mouse(hit, cur_x, cur_y, ev.buttons);
				}
			}
		} else {
			if (focused)
				send_mouse(focused, cur_x, cur_y, ev.buttons);
		}

		prev_buttons = ev.buttons;
	}

	void handle_kbd(kbd_event_t &kev) {
		if (!focused || !focused->mbox[0])
			return;
		mpi_message_t kmsg;
		struct display_key *dk = (struct display_key *)kmsg.data;
		kmsg.header   = DISPLAY_KEY;
		dk->window_id = focused->id;
		dk->keycode   = kev.keycode;
		dk->pressed   = kev.pressed;
		mpi_postMessage(focused->mbox, DISPLAY_KEY, &kmsg);
	}
};

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	static char mbox_views[]   = "views";
	static char mbox_system[]  = "system";

	if (mpi_createMbox(mbox_views) != 0) {
		printf("views: mpi_createMbox failed\n");
		return 1;
	}

	mpi_message_t req;
	req.header  = 0x82;
	req.data[0] = 'v'; req.data[1] = 'i'; req.data[2] = 'e';
	req.data[3] = 'w'; req.data[4] = 's'; req.data[5] = '\0';
	mpi_postMessage(mbox_system, 0x82, &req);

	mpi_message_t reply;
	while (mpi_fetchMessage(mbox_views, &reply) != 0)
		sched_yield();

	if (reply.data[0] == 0) {
		printf("views: VESA init failed\n");
		return 1;
	}

	WindowManager wm;
	if (wm.init() != 0) {
		printf("views: fb_open failed\n");
		return 1;
	}
	wm.startup();

	/* Launch taskbar */
	{
		int pid = fork();
		if (pid == 0) {
			char *argv_tb[] = { (char *)"taskbar", NULL };
			char *envp_tb[] = { NULL };
			execve("sys:/bin/taskbar", argv_tb, envp_tb);
			printf("views: failed to exec taskbar\n");
			exit(1);
		}
	}

	mpi_message_t msg;

	for (;;) {
		kbd_event_t kev;
		while (fb_poll_kbd(&kev) == 0)
			wm.handle_kbd(kev);

		mouse_event_t ev;
		while (fb_poll_mouse(&ev) == 0)
			wm.handle_mouse(ev);

		while (mpi_fetchMessage(mbox_views, &msg) == 0) {
			switch (msg.header) {
			case DISPLAY_QUERY:
				wm.handle_query(
				    (struct display_query *)msg.data);
				break;
			case DISPLAY_CLAIM:
				wm.handle_claim(
				    (struct display_claim_req *)msg.data);
				break;
			case DISPLAY_FLIP:
				wm.handle_flip(
				    (struct display_flip *)msg.data);
				break;
			case DISPLAY_RELEASE:
				wm.handle_release(
				    (struct display_release *)msg.data);
				break;
			default:
				break;
			}
		}

		sched_yield();
	}

	return 0;
}
