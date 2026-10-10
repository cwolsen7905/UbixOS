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

#include <cstring>
#include <objgfx/objgfx.h>
#include <objgfx/ogImage.h>
#include <ubistry/ubistry.h>
#include <api/ubix.h>
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

/* System palette for window chrome (views/theme.h); re-resolved per session
 * user in set_desktop_from_registry() from views/theme/{mode,accent}. */
struct ui_theme g_ui = ui_theme_dark;

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
	set_desktop_from_registry();
	draw_desktop();
	cur_x_ = (int)fb_.width / 2;
	cur_y_ = (int)fb_.height / 2;
	cursor_save(cur_x_, cur_y_);
	cursor_draw(cur_x_, cur_y_);
	cur_drawn_ = true;
	fb_.flush_to_lf(0, 0, (int)fb_.width, (int)fb_.height);
}

/**
 * Scale a packed colour's brightness by num/den (clamped); used to derive the
 * four jailbar shades from a single base colour.
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
	return FB_RGB(r, g, b);
}

/*
 * Render the entire desktop background into desk_cache_ once.  Called only when
 * the desktop changes (mode/colour/wallpaper/resolution) — never per composite.
 */
void Compositor::render_desktop_cache()
{
	int sw = (int)fb_.width, sh = (int)fb_.height;
	if (sw <= 0 || sh <= 0)
		return;
	dc_w_ = sw;
	dc_h_ = sh;
	desk_cache_.assign((size_t)sw * sh, 0);
	uint32_t *d = desk_cache_.data();

	if (desk_mode_ == DESK_IMAGE && wp_w_ > 0 && wp_h_ > 0)
	{
		for (int py = 0; py < sh; py++)
		{
			int sy = py * wp_h_ / sh;
			if (sy >= wp_h_)
				sy = wp_h_ - 1;
			const uint32_t *srow = &wp_[(size_t)sy * wp_w_];
			uint32_t *drow = d + (size_t)py * sw;
			for (int px = 0; px < sw; px++)
			{
				int sx = px * wp_w_ / sw;
				if (sx >= wp_w_)
					sx = wp_w_ - 1;
				drow[px] = srow[sx];
			}
		}
		return;
	}

	if (desk_mode_ == DESK_SOLID)
	{
		for (size_t i = 0, n = (size_t)sw * sh; i < n; i++)
			d[i] = solid_color_;
		return;
	}

	/* Jailbars: four brightness shades of the base colour as vertical stripes. */
	static const int shade[4] = {10, 7, 13, 5};
	uint32_t bar[4];
	for (int i = 0; i < 4; i++)
		bar[i] = bar_base_ ? scale_color(bar_base_, shade[i], 10) : g_jailbar_colors[i];
	for (int py = 0; py < sh; py++)
	{
		uint32_t *drow = d + (size_t)py * sw;
		for (int px = 0; px < sw; px++)
			drow[px] = bar[px & 3];
	}
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
	if (x2 <= x || y2 <= y)
		return;

	/* Blit the pre-rendered background from the cache (one memcpy-style copy per
	 * row) instead of re-stretching the wallpaper per pixel. */
	if ((int)desk_cache_.size() >= dc_w_ * dc_h_ && dc_w_ == (int)fb_.width)
		fb_.blit(x, y, x2 - x, y2 - y, desk_cache_.data() + (size_t)y * dc_w_ + x, dc_w_);
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
	{
		ulogf(ULOG_ERR, "views: wallpaper load FAILED: %s", path);
		return;
	}

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
	ulogf(ULOG_INFO, "views: wallpaper %s loaded %dx%d", path, w, h);
}

void Compositor::set_desktop_from_registry()
{
	/* Resolve user-first (per-user override) then the bare system default. */
	const char *u = active_user_.empty() ? nullptr : active_user_.c_str();
	char mode[32];
	int ival;

	desk_mode_ = DESK_BARS;
	if (ubistry_get_for(u, "views/desktop/mode", mode, sizeof(mode)) == 0)
	{
		if (strcmp(mode, "image") == 0)
			desk_mode_ = DESK_IMAGE;
		else if (strcmp(mode, "solid") == 0)
			desk_mode_ = DESK_SOLID;
	}

	if (ubistry_get_for_int(u, "views/desktop/color", &ival) == 0)
		solid_color_ = (uint32_t)ival & 0x00FFFFFFu;
	if (ubistry_get_for_int(u, "views/desktop/barcolor", &ival) == 0)
		bar_base_ = (uint32_t)ival & 0x00FFFFFFu;

	/* Window chrome palette: light/dark mode + the user's accent override. */
	g_ui = ui_theme_load(u);

	wp_.clear();
	wp_w_ = wp_h_ = 0;
	if (desk_mode_ == DESK_IMAGE)
	{
		char path[256];
		if (ubistry_get_for(u, "views/desktop/image", path, sizeof(path)) == 0)
			load_wallpaper(path);
	}

	render_desktop_cache(); /* rebuild the cached background for the new settings */
}

void Compositor::set_active_user(const char *user)
{
	active_user_ = (user != nullptr) ? user : "";
	set_desktop_from_registry();
}

void Compositor::set_resize_preview(bool active, int x, int y, int w, int h)
{
	resize_preview_ = active;
	rp_x_ = x;
	rp_y_ = y;
	rp_w_ = w;
	rp_h_ = h;
	invalidate_all();
}

void Compositor::reopen_fb()
{
	if (fb_.reopen() != 0)
		return;

	/* Clamp the cursor into the new screen bounds. */
	if (cur_x_ > (int)fb_.width - CUR_W)
		cur_x_ = (int)fb_.width - CUR_W;
	if (cur_y_ > (int)fb_.height - CUR_H)
		cur_y_ = (int)fb_.height - CUR_H;
	if (cur_x_ < 0)
		cur_x_ = 0;
	if (cur_y_ < 0)
		cur_y_ = 0;
	cur_drawn_ = false;

	set_desktop_from_registry(); /* re-stretch the wallpaper to the new size */
}

bool Compositor::rect_covered(int rx, int ry, int rw, int rh)
{
	for (auto *w : reg_.z_stack())
	{
		if (w->minimized || w->translucent)
			continue; /* a translucent window never hides what is beneath it */
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
		if (w->minimized)
			continue;
		int cy = w->y + w->decor_h;

		if (!w->translucent)
		{
			draw_window_shadow(w, rx, ry, rw, rh);
			save_corners(w);
		}

		int x1 = (rx > w->x) ? rx : w->x;
		int y1 = (ry > cy) ? ry : cy;
		int x2 = (rx + rw < w->x + w->w) ? rx + rw : w->x + w->w;
		int y2 = (ry + rh < cy + w->h) ? ry + rh : cy + w->h;
		if (x2 > x1 && y2 > y1)
		{
			int bx = x1 - w->x;
			int by = y1 - cy;
			if (w->translucent)
				blend_blit(x1,
				           y1,
				           x2 - x1,
				           y2 - y1,
				           (const uint32_t *)((const uint8_t *)w->buf + (uint32_t)by * w->pitch +
				                              (uint32_t)bx * WIN_BPP),
				           (int)(w->pitch / WIN_BPP));
			else
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
		w->draw_grip(fb_);
		if (!w->translucent)
			frame_window(w, rx, ry, rw, rh);
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

/* Blend a straight-alpha 0xAARRGGBB buffer over the framebuffer (translucent
 * windows).  Fully clear pixels are skipped and opaque ones copied, so the
 * common cases cost no more than a plain blit. */
void Compositor::blend_blit(int dx, int dy, int w, int h, const uint32_t *src, int stride)
{
	for (int row = 0; row < h; row++)
	{
		int py = dy + row;
		if (py < 0 || py >= (int)fb_.height)
			continue;
		const uint32_t *s = src + (size_t)row * stride;
		for (int col = 0; col < w; col++)
		{
			int px = dx + col;
			if (px < 0 || px >= (int)fb_.width)
				continue;
			uint32_t a = s[col] >> 24;
			if (a == 0)
				continue;
			uint32_t c = s[col] & 0x00FFFFFFu;
			fb_.pixel(px, py, a == 255 ? c : ui_mix(fb_.read(px, py), c, a + (a >> 7)));
		}
	}
}

/**
 * Integer square root (floor) for small non-negative values — the corner
 * anti-aliasing stays integer-only, like the shadow (world is -mno-sse).
 */
static int isqrt_small(int v)
{
	int r = 0;
	while ((r + 1) * (r + 1) <= v)
		r++;
	return r;
}

/* Soft drop shadow: darken the framebuffer in a band around the window, offset
 * downward, with a quadratic distance falloff (no sqrt — world is -mno-sse).
 * Reads-then-darkens so it is idempotent within a single repaint pass. */
void Compositor::draw_window_shadow(const Window *w, int clipx, int clipy, int clipw, int cliph)
{
	if (frameless(w))
		return; /* maximized / full-screen windows sit flush: no shadow */
	bool active = (w == reg_.focused());
	const int reach = active ? g_ui.shadow_win_reach : g_ui.shadow_inact_reach;
	const int y_off = active ? g_ui.shadow_win_y : g_ui.shadow_inact_y;
	const int alpha_max = active ? g_ui.shadow_win_a : g_ui.shadow_inact_a;

	int wx0 = w->x, wy0 = w->y;
	int wx1 = w->x + w->w, wy1 = w->y + w->decor_h + w->h;
	int rx0 = wx0, ry0 = wy0 + y_off, rx1 = wx1, ry1 = wy1 + y_off;

	int bx0 = wx0 - reach, by0 = wy0 - reach + y_off, bx1 = wx1 + reach, by1 = wy1 + reach + y_off;
	if (bx0 < clipx)
		bx0 = clipx;
	if (by0 < clipy)
		by0 = clipy;
	if (bx1 > clipx + clipw)
		bx1 = clipx + clipw;
	if (by1 > clipy + cliph)
		by1 = clipy + cliph;
	if (bx0 < 0)
		bx0 = 0;
	if (by0 < 0)
		by0 = 0;
	if (bx1 > (int)fb_.width)
		bx1 = (int)fb_.width;
	if (by1 > (int)fb_.height)
		by1 = (int)fb_.height;

	for (int py = by0; py < by1; py++)
	{
		for (int px = bx0; px < bx1; px++)
		{
			/* The window itself is opaque — never shadow under it, except the
			 * corner squares, whose outside-the-arc pixels show through. */
			if (px >= wx0 && px < wx1 && py >= wy0 && py < wy1)
			{
				bool cx = px < wx0 + UI_R_LG || px >= wx1 - UI_R_LG;
				bool cy = py < wy0 + UI_R_LG || py >= wy1 - UI_R_LG;
				if (!(cx && cy))
					continue;
			}
			int dx = (px < rx0) ? (rx0 - px) : ((px >= rx1) ? (px - rx1 + 1) : 0);
			int dy = (py < ry0) ? (ry0 - py) : ((py >= ry1) ? (py - ry1 + 1) : 0);
			int d2 = dx * dx + dy * dy;
			if (d2 >= reach * reach)
				continue;
			int a = alpha_max * (reach * reach - d2) / (reach * reach);
			if (a <= 0)
				continue;
			fb_.pixel(px, py, ui_mix(fb_.read(px, py), g_ui.shadow, (uint32_t)a));
		}
	}
}

/* True for windows drawn flush with no shadow, outline or rounding: maximized
 * windows and anything covering the whole screen (the login screen). */
bool Compositor::frameless(const Window *w) const
{
	return w->maximized ||
	       (w->x <= 0 && w->y <= 0 && w->x + w->w >= (int)fb_.width && w->y + w->decor_h + w->h >= (int)fb_.height);
}

/* Snapshot the four corner squares under a window (desktop, shadow and any
 * windows below) before it is drawn, so frame_window() can restore exactly
 * what belongs outside the rounded corner — not just the wallpaper. */
void Compositor::save_corners(const Window *w)
{
	const int r = UI_R_LG;
	int wx1 = w->x + w->w, wy1 = w->y + w->decor_h + w->h;
	int ox[4] = {w->x, wx1 - r, w->x, wx1 - r};
	int oy[4] = {w->y, w->y, wy1 - r, wy1 - r};
	for (int c = 0; c < 4; c++)
		for (int py = 0; py < r; py++)
			for (int px = 0; px < r; px++)
			{
				int sx = ox[c] + px, sy = oy[c] + py;
				bool on = sx >= 0 && sy >= 0 && sx < (int)fb_.width && sy < (int)fb_.height;
				corner_bg_[c][py * r + px] = on ? fb_.read(sx, sy) : 0;
			}
}

/* Draw the 1px window outline (edge-strong focused / edge unfocused) and round
 * the outer corners to UI_R_LG with anti-aliasing, restoring the pixels saved
 * by save_corners() outside the arc.  Everything is clipped to the damage
 * rectangle [clipx,clipy,clipw,cliph].  Maximized windows stay square. */
void Compositor::frame_window(const Window *w, int clipx, int clipy, int clipw, int cliph)
{
	if (frameless(w))
		return;
	const int r = UI_R_LG;
	uint32_t edge = (w == reg_.focused()) ? g_ui.edge_strong : g_ui.edge;
	int wx0 = w->x, wy0 = w->y;
	int wx1 = w->x + w->w, wy1 = w->y + w->decor_h + w->h;

	auto put = [&](int px, int py, uint32_t col)
	{
		if (px < clipx || px >= clipx + clipw || py < clipy || py >= clipy + cliph || px < 0 || py < 0 ||
		    px >= (int)fb_.width || py >= (int)fb_.height)
			return;
		fb_.pixel(px, py, col);
	};

	/* Straight edges between the corner squares. */
	for (int px = wx0 + r; px < wx1 - r; px++)
	{
		put(px, wy0, edge);
		put(px, wy1 - 1, edge);
	}
	for (int py = wy0 + r; py < wy1 - r; py++)
	{
		put(wx0, py, edge);
		put(wx1 - 1, py, edge);
	}

	/* Corners: per pixel, coverage of the window disc (radius r) and of its
	 * 1px outline ring, in half-pixel units to avoid floating point. */
	int ox[4] = {wx0, wx1 - r, wx0, wx1 - r};
	int oy[4] = {wy0, wy0, wy1 - r, wy1 - r};
	for (int c = 0; c < 4; c++)
	{
		bool left = (c == 0 || c == 2), top = (c < 2);
		for (int py = 0; py < r; py++)
			for (int px = 0; px < r; px++)
			{
				int sx = ox[c] + px, sy = oy[c] + py;
				if (sx < clipx || sx >= clipx + clipw || sy < clipy || sy >= clipy + cliph || sx < 0 ||
				    sy < 0 || sx >= (int)fb_.width || sy >= (int)fb_.height)
					continue;
				/* distance (x2) from the arc centre to this pixel's centre */
				int dx = left ? (2 * (r - px) - 1) : (2 * (px + 1) - 1);
				int dy = top ? (2 * (r - py) - 1) : (2 * (py + 1) - 1);
				int d = isqrt_small(dx * dx + dy * dy); /* 2 * distance */
				int R2 = 2 * r;
				/* outside coverage 0..256 (1 px soft edge), ring coverage 0..256 */
				int in_cov = (R2 + 1 - d) * 128;
				in_cov = in_cov < 0 ? 0 : (in_cov > 256 ? 256 : in_cov);
				int ring = (d - (R2 - 3)) * 128;
				ring = ring < 0 ? 0 : (ring > 256 ? 256 : ring);
				uint32_t win = ui_mix(fb_.read(sx, sy), edge, (uint32_t)ring);
				fb_.pixel(sx, sy, ui_mix(corner_bg_[c][py * r + px], win, (uint32_t)in_cov));
			}
	}
}

void Compositor::composite_all()
{
	draw_desktop();
	for (auto *w : reg_.z_stack())
	{
		if (w->minimized)
			continue;
		if (w->translucent)
		{
			blend_blit(
			    w->x, w->y + w->decor_h, w->w, w->h, (const uint32_t *)w->buf, (int)(w->pitch / WIN_BPP));
			continue;
		}
		draw_window_shadow(w, 0, 0, (int)fb_.width, (int)fb_.height);
		save_corners(w);
		w->blit_to(fb_);
		if (w->decor_h > 0)
			w->draw_decor(fb_, w == reg_.focused());
		w->draw_grip(fb_);
		frame_window(w, 0, 0, (int)fb_.width, (int)fb_.height);
	}
	if (resize_preview_)
	{
		uint32_t c = FB_WHITE;
		fb_.rect(rp_x_, rp_y_, rp_w_, 1, c);
		fb_.rect(rp_x_, rp_y_ + rp_h_ - 1, rp_w_, 1, c);
		fb_.rect(rp_x_, rp_y_, 1, rp_h_, c);
		fb_.rect(rp_x_ + rp_w_ - 1, rp_y_, 1, rp_h_, c);
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

bool Compositor::flush()
{
	if (!damage_.valid)
		return false;

	int x = damage_.x, y = damage_.y;
	int w = damage_.w, h = damage_.h;
	damage_.valid = false;

	/* Inflate by the shadow reach so a moved/closed window's old soft shadow
	 * (which falls outside its rect) is repainted — otherwise it leaves trails. */
	const int shadow_pad = 14;
	x -= shadow_pad;
	y -= shadow_pad;
	w += 2 * shadow_pad;
	h += 2 * shadow_pad;

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
		return false;

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
	return true;
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
