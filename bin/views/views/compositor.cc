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

#include <objgfx/objgfx.h>
#include <objgfx/ogImage.h>
#include <ubistry/ubistry.h>
#include "compositor.hh"

static const uint8_t g_cursor_mask[CUR_H][CUR_W] = {
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {1, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2},
    {1, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2},
    {1, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2},
    {1, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2},
    {1, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2},
    {1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
};

static const uint32_t g_jailbar_colors[4] = {
    FB_RGB(0x1A, 0x1A, 0x2E),
    FB_RGB(0x1A, 0x2E, 0x00),
    FB_RGB(0x2E, 0x00, 0x1A),
    FB_RGB(0x00, 0x1A, 0x1A),
};

Compositor::Compositor(WindowRegistry &reg)
    : reg_(reg), cur_x_(0), cur_y_(0), cur_drawn_(false), damage_{0, 0, 0, 0, false}
{
}

int Compositor::init()
{
	int r = fb_.open();
	if (r != 0)
		return r;
	return fb_.init_shadow();
}

void Compositor::startup()
{
	set_wallpaper_from_registry();
	draw_desktop();
	cur_x_ = (int)fb_.width / 2;
	cur_y_ = (int)fb_.height / 2;
	cursor_save(cur_x_, cur_y_);
	cursor_draw(cur_x_, cur_y_);
	cur_drawn_ = true;
	fb_.flush_to_lf(0, 0, (int)fb_.width, (int)fb_.height);
}

void Compositor::desktop_fill_rect(int x, int y, int w, int h)
{
	int x2 = x + w, y2 = y + h;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x2 > (int)fb_.width)
		x2 = (int)fb_.width;
	if (y2 > (int)fb_.height)
		y2 = (int)fb_.height;

	/* With a wallpaper loaded, sample it (centered) and fill any margin with a
	 * flat colour; otherwise fall back to the default jailbar pattern. */
	if (wp_w_ > 0 && wp_h_ > 0)
	{
		int ox = ((int)fb_.width - wp_w_) / 2;
		int oy = ((int)fb_.height - wp_h_) / 2;
		for (int py = y; py < y2; py++)
		{
			int sy = py - oy;
			for (int px = x; px < x2; px++)
			{
				int sx = px - ox;
				if (sx >= 0 && sx < wp_w_ && sy >= 0 && sy < wp_h_)
					fb_.pixel(px, py, wp_[(size_t)sy * wp_w_ + sx]);
				else
					fb_.pixel(px, py, FB_RGB(0x10, 0x10, 0x18));
			}
		}
		return;
	}

	for (int py = y; py < y2; py++)
		for (int px = x; px < x2; px++)
			fb_.pixel(px, py, g_jailbar_colors[px & 3]);
}

void Compositor::draw_desktop()
{
	desktop_fill_rect(0, 0, (int)fb_.width, (int)fb_.height);
}

/**
 * Decode a 24bpp BMP wallpaper into the 32bpp cache.  An empty/failed path
 * clears the cache (desktop falls back to the solid pattern).
 */
void Compositor::load_wallpaper(const char *path)
{
	wp_.clear();
	wp_w_ = wp_h_ = 0;
	if (path == nullptr || path[0] == '\0')
		return;

	ogImage img;
	ogSurface s;
	if (!img.Load(path, s))
		return;

	int w = (int)s.ogGetMaxX() + 1;
	int h = (int)s.ogGetMaxY() + 1;
	if (w <= 0 || h <= 0)
		return;

	wp_.resize((size_t)w * (size_t)h);
	for (int yy = 0; yy < h; yy++)
	{
		const uint8_t *row = (const uint8_t *)s.ogGetPtr(0, (uInt32)yy);
		if (row == nullptr)
		{
			wp_.clear();
			return;
		}
		for (int xx = 0; xx < w; xx++)
		{
			const uint8_t *p = row + (size_t)xx * 3; /* BMP 24bpp: B,G,R */
			wp_[(size_t)yy * w + xx] = FB_RGB(p[2], p[1], p[0]);
		}
	}
	wp_w_ = w;
	wp_h_ = h;
}

void Compositor::set_wallpaper_from_registry()
{
	char path[256];
	if (ubistry_get_str("/views/desktop/wallpaper", path, sizeof(path)) == 0)
		load_wallpaper(path);
	else
		load_wallpaper(nullptr);
}

bool Compositor::rect_covered(int rx, int ry, int rw, int rh)
{
	for (auto *w : reg_.z_stack())
	{
		int cy = w->y + w->decor_h;
		if (w->x <= rx && w->y <= ry && w->x + w->w >= rx + rw && cy + w->h >= ry + rh)
			return true;
	}
	return false;
}

void Compositor::reblit_rect(int rx, int ry, int rw, int rh)
{
	for (auto *w : reg_.z_stack())
	{
		int cy = w->y + w->decor_h;

		int x1 = (rx > w->x) ? rx : w->x;
		int y1 = (ry > cy) ? ry : cy;
		int x2 = (rx + rw < w->x + w->w) ? rx + rw : w->x + w->w;
		int y2 = (ry + rh < cy + w->h) ? ry + rh : cy + w->h;
		if (x2 > x1 && y2 > y1)
		{
			int bx = x1 - w->x;
			int by = y1 - cy;
			fb_.blit(x1,
			         y1,
			         x2 - x1,
			         y2 - y1,
			         (const uint32_t *)((const uint8_t *)w->buf + (uint32_t)by * w->pitch +
			                            (uint32_t)bx * WIN_BPP),
			         (int)(w->pitch / WIN_BPP));
		}

		if (w->decor_h > 0 && rx < w->x + w->w && rx + rw > w->x && ry < w->y + w->decor_h && ry + rh > w->y)
			w->draw_decor(fb_, w == reg_.focused());
	}
}

void Compositor::cursor_save(int x, int y)
{
	for (int row = 0; row < CUR_H; row++)
	{
		if (y + row < 0 || y + row >= (int)fb_.height)
			continue;
		for (int col = 0; col < CUR_W; col++)
			cur_saved_[row * CUR_W + col] = fb_.read(x + col, y + row);
	}
}

void Compositor::cursor_erase(int x, int y)
{
	for (int row = 0; row < CUR_H; row++)
		for (int col = 0; col < CUR_W; col++)
			fb_.pixel(x + col, y + row, cur_saved_[row * CUR_W + col]);
}

void Compositor::cursor_draw(int x, int y)
{
	for (int row = 0; row < CUR_H; row++)
	{
		for (int col = 0; col < CUR_W; col++)
		{
			uint8_t m = g_cursor_mask[row][col];
			if (m == 2)
				continue;
			uint32_t color = (m == 1) ? FB_WHITE : FB_RGB(0x40, 0x40, 0x40);
			fb_.pixel(x + col, y + row, color);
		}
	}
}

void Compositor::composite_all()
{
	draw_desktop();
	for (auto *w : reg_.z_stack())
	{
		w->blit_to(fb_);
		if (w->decor_h > 0)
			w->draw_decor(fb_, w == reg_.focused());
	}
	cursor_save(cur_x_, cur_y_);
	cursor_draw(cur_x_, cur_y_);
	cur_drawn_ = true;
}

void Compositor::partial_composite(int sx, int sy, int sw, int sh)
{
	if (!rect_covered(sx, sy, sw, sh))
		desktop_fill_rect(sx, sy, sw, sh);
	reblit_rect(sx, sy, sw, sh);
	if (cur_x_ < sx + sw && cur_x_ + CUR_W > sx && cur_y_ < sy + sh && cur_y_ + CUR_H > sy)
	{
		cursor_save(cur_x_, cur_y_);
		cursor_draw(cur_x_, cur_y_);
		cur_drawn_ = true;
	}
}

void Compositor::invalidate(int x, int y, int w, int h)
{
	if (!damage_.valid)
	{
		damage_ = {x, y, w, h, true};
		return;
	}
	int x2 = (damage_.x + damage_.w > x + w) ? damage_.x + damage_.w : x + w;
	int y2 = (damage_.y + damage_.h > y + h) ? damage_.y + damage_.h : y + h;
	damage_.x = (damage_.x < x) ? damage_.x : x;
	damage_.y = (damage_.y < y) ? damage_.y : y;
	damage_.w = x2 - damage_.x;
	damage_.h = y2 - damage_.y;
}

void Compositor::invalidate_all()
{
	damage_ = {0, 0, (int)fb_.width, (int)fb_.height, true};
}

void Compositor::flush()
{
	if (!damage_.valid)
		return;

	int x = damage_.x, y = damage_.y;
	int w = damage_.w, h = damage_.h;
	damage_.valid = false;

	/* Clamp to screen bounds */
	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > (int)fb_.width)
		w = (int)fb_.width - x;
	if (y + h > (int)fb_.height)
		h = (int)fb_.height - y;
	if (w <= 0 || h <= 0)
		return;

	if (x == 0 && y == 0 && w >= (int)fb_.width && h >= (int)fb_.height)
	{
		composite_all();
		fb_.flush_to_lf(0, 0, (int)fb_.width, (int)fb_.height);
	}
	else
	{
		partial_composite(x, y, w, h);
		fb_.flush_to_lf(x, y, w, h);
	}
}

void Compositor::cursor_move(int dx, int dy)
{
	int old_x = cur_x_, old_y = cur_y_;

	if (cur_drawn_)
		cursor_erase(old_x, old_y);

	cur_x_ += dx;
	cur_y_ += dy;
	if (cur_x_ < 0)
		cur_x_ = 0;
	if (cur_y_ < 0)
		cur_y_ = 0;
	if (cur_x_ >= (int)fb_.width - CUR_W)
		cur_x_ = (int)fb_.width - CUR_W;
	if (cur_y_ >= (int)fb_.height - CUR_H)
		cur_y_ = (int)fb_.height - CUR_H;

	if (!rect_covered(old_x, old_y, CUR_W, CUR_H))
		desktop_fill_rect(old_x, old_y, CUR_W, CUR_H);
	reblit_rect(old_x, old_y, CUR_W, CUR_H);

	cursor_save(cur_x_, cur_y_);
	cursor_draw(cur_x_, cur_y_);
	cur_drawn_ = true;

	/* Flush bounding box of old + new cursor position to LFB immediately. */
	int fx = (old_x < cur_x_) ? old_x : cur_x_;
	int fy = (old_y < cur_y_) ? old_y : cur_y_;
	int fx2 = (old_x + CUR_W > cur_x_ + CUR_W) ? old_x + CUR_W : cur_x_ + CUR_W;
	int fy2 = (old_y + CUR_H > cur_y_ + CUR_H) ? old_y + CUR_H : cur_y_ + CUR_H;
	fb_.flush_to_lf(fx, fy, fx2 - fx, fy2 - fy);
}
