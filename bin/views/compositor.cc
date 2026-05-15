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

#include "compositor.hh"

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

Compositor::Compositor(WindowRegistry &reg)
    : reg_(reg), cur_x_(0), cur_y_(0), cur_drawn_(false)
{}

int
Compositor::init()
{
	return fb_.open();
}

void
Compositor::startup()
{
	draw_desktop();
	cur_x_ = (int)fb_.width  / 2;
	cur_y_ = (int)fb_.height / 2;
	cursor_save(cur_x_, cur_y_);
	cursor_draw(cur_x_, cur_y_);
	cur_drawn_ = true;
}

void
Compositor::desktop_fill_rect(int x, int y, int w, int h)
{
	int x2 = x + w, y2 = y + h;
	if (x  < 0) x  = 0;
	if (y  < 0) y  = 0;
	if (x2 > (int)fb_.width)  x2 = (int)fb_.width;
	if (y2 > (int)fb_.height) y2 = (int)fb_.height;
	for (int py = y; py < y2; py++)
		for (int px = x; px < x2; px++)
			fb_.pixel(px, py, jailbar_colors[px & 3]);
}

void
Compositor::draw_desktop()
{
	desktop_fill_rect(0, 0, (int)fb_.width, (int)fb_.height);
}

void
Compositor::reblit_rect(int rx, int ry, int rw, int rh)
{
	for (auto *w : reg_.z_stack()) {
		int cy = w->y + w->decor_h;

		int x1 = (rx    > w->x)        ? rx    : w->x;
		int y1 = (ry    > cy)           ? ry    : cy;
		int x2 = (rx+rw < w->x + w->w) ? rx+rw : w->x + w->w;
		int y2 = (ry+rh < cy  + w->h)  ? ry+rh : cy   + w->h;
		if (x2 > x1 && y2 > y1) {
			int bx = x1 - w->x;
			int by = y1 - cy;
			fb_.blit(x1, y1, x2 - x1, y2 - y1,
			    (const uint32_t *)((const uint8_t *)w->buf +
			        (uint32_t)by * w->pitch +
			        (uint32_t)bx * WIN_BPP),
			    (int)(w->pitch / WIN_BPP));
		}

		if (w->decor_h > 0 &&
		    rx < w->x + w->w && rx + rw > w->x &&
		    ry < w->y + w->decor_h && ry + rh > w->y)
			w->draw_decor(fb_, w == reg_.focused());
	}
}

void
Compositor::cursor_save(int x, int y)
{
	for (int row = 0; row < CUR_H; row++) {
		if (y + row < 0 || y + row >= (int)fb_.height)
			continue;
		for (int col = 0; col < CUR_W; col++)
			cur_saved_[row * CUR_W + col] =
			    fb_.read(x + col, y + row);
	}
}

void
Compositor::cursor_erase(int x, int y)
{
	for (int row = 0; row < CUR_H; row++)
		for (int col = 0; col < CUR_W; col++)
			fb_.pixel(x + col, y + row,
			    cur_saved_[row * CUR_W + col]);
}

void
Compositor::cursor_draw(int x, int y)
{
	for (int row = 0; row < CUR_H; row++) {
		for (int col = 0; col < CUR_W; col++) {
			uint8_t m = cursor_mask[row][col];
			if (m == 2)
				continue;
			uint32_t color = (m == 1)
			    ? FB_WHITE
			    : FB_RGB(0x40, 0x40, 0x40);
			fb_.pixel(x + col, y + row, color);
		}
	}
}

void
Compositor::composite_all()
{
	draw_desktop();
	for (auto *w : reg_.z_stack()) {
		w->blit_to(fb_);
		if (w->decor_h > 0)
			w->draw_decor(fb_, w == reg_.focused());
	}
	cursor_save(cur_x_, cur_y_);
	cursor_draw(cur_x_, cur_y_);
	cur_drawn_ = true;
}

void
Compositor::partial_composite(int sx, int sy, int sw, int sh)
{
	desktop_fill_rect(sx, sy, sw, sh);
	reblit_rect(sx, sy, sw, sh);
	if (cur_x_ < sx + sw && cur_x_ + CUR_W > sx &&
	    cur_y_ < sy + sh && cur_y_ + CUR_H > sy) {
		cursor_save(cur_x_, cur_y_);
		cursor_draw(cur_x_, cur_y_);
		cur_drawn_ = true;
	}
}

void
Compositor::cursor_move(int dx, int dy)
{
	int old_x = cur_x_, old_y = cur_y_;

	if (cur_drawn_)
		cursor_erase(old_x, old_y);

	cur_x_ += dx;
	cur_y_ += dy;
	if (cur_x_ < 0) cur_x_ = 0;
	if (cur_y_ < 0) cur_y_ = 0;
	if (cur_x_ >= (int)fb_.width  - CUR_W)
		cur_x_ = (int)fb_.width  - CUR_W;
	if (cur_y_ >= (int)fb_.height - CUR_H)
		cur_y_ = (int)fb_.height - CUR_H;

	desktop_fill_rect(old_x, old_y, CUR_W, CUR_H);
	reblit_rect(old_x, old_y, CUR_W, CUR_H);

	cursor_save(cur_x_, cur_y_);
	cursor_draw(cur_x_, cur_y_);
	cur_drawn_ = true;
}
