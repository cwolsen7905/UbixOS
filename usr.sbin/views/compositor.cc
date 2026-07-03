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

/* Focused title-bar colours; driven by the per-user accent (views/theme/accent),
 * resolved in set_desktop_from_registry().  Default to a calm desaturated slate. */
uint32_t g_theme_decor_bg = FB_RGB(0x33, 0x3C, 0x4C);
uint32_t g_theme_decor_hi = FB_RGB(0x4A, 0x55, 0x68);

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

	/* Accent colour for the focused window title bar — a calm, desaturated
	 * slate-blue (modern) rather than the old saturated primary blue. */
	g_theme_decor_bg = FB_RGB(0x33, 0x3C, 0x4C);
	g_theme_decor_hi = FB_RGB(0x4A, 0x55, 0x68);
	/* Per-user accent first, then the seeded system value; reject a zero (pure black)
	 * result as "unset" so a missing/zeroed per-user override can't black out the title
	 * bar (it would just keep the slate default set above). */
	if ((ubistry_get_for_int(u, "views/theme/accent", &ival) == 0 && ((uint32_t)ival & 0x00FFFFFFu) != 0) ||
	    (ubistry_get_int("/views/theme/accent", &ival) == 0 && ((uint32_t)ival & 0x00FFFFFFu) != 0))
	{
		uint32_t a = (uint32_t)ival & 0x00FFFFFFu;
		g_theme_decor_bg = a;
		g_theme_decor_hi = scale_color(a, 14, 10); /* brighter highlight line */
	}

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

/* Preview panel geometry constants (Windows-11 hover thumbnail). */
#define PV_MAXW 208              /* max thumbnail content width  */
#define PV_MAXH 132              /* max thumbnail content height */
#define PV_PAD 6                 /* panel inner padding around the content */
#define PV_TITLE (FB_FONT_H + 6) /* title strip height above the content */
#define PV_RESERVE 40            /* clearance above the bottom taskbar */

void Compositor::set_window_preview(bool active, uint32_t window_id, int anchor_x)
{
	/* Repaint the area the old preview occupied before changing state. */
	bool was = win_preview_;
	win_preview_ = false;
	if (was)
		invalidate(pv_px_, pv_py_, pv_pw_, pv_ph_);

	if (!active)
		return;

	Window *w = reg_.find(window_id);
	if (w == nullptr || w->buf == nullptr || w->w <= 0 || w->h <= 0)
		return;

	/* Fit the window's aspect ratio into the PV_MAXW x PV_MAXH content box. */
	int tw = PV_MAXW;
	int th = (int)((int64_t)tw * w->h / w->w);
	if (th > PV_MAXH)
	{
		th = PV_MAXH;
		tw = (int)((int64_t)th * w->w / w->h);
	}
	if (tw < 1)
		tw = 1;
	if (th < 1)
		th = 1;

	pv_tw_ = tw;
	pv_th_ = th;
	pv_pw_ = tw + 2 * PV_PAD;
	pv_ph_ = th + 2 * PV_PAD + PV_TITLE;

	pv_px_ = anchor_x - pv_pw_ / 2;
	if (pv_px_ < 4)
		pv_px_ = 4;
	if (pv_px_ + pv_pw_ > (int)fb_.width - 4)
		pv_px_ = (int)fb_.width - 4 - pv_pw_;
	pv_py_ = (int)fb_.height - PV_RESERVE - pv_ph_;
	if (pv_py_ < 4)
		pv_py_ = 4;

	pv_id_ = window_id;
	win_preview_ = true;
	invalidate(pv_px_, pv_py_, pv_pw_, pv_ph_);
}

bool Compositor::preview_hit(int x, int y, int w, int h) const
{
	return win_preview_ && x < pv_px_ + pv_pw_ && x + w > pv_px_ && y < pv_py_ + pv_ph_ && y + h > pv_py_;
}

/* Draw the floating live thumbnail: a flat panel + border, the window title, and
 * a nearest-neighbour downscale of the window's shared buffer.  Composited last
 * (over everything), so it must be redrawn whenever its area is repainted. */
void Compositor::draw_window_preview()
{
	if (!win_preview_)
		return;
	Window *w = reg_.find(pv_id_);
	if (w == nullptr || w->buf == nullptr || w->w <= 0 || w->h <= 0)
		return;

	uint32_t panel = FB_RGB(0x22, 0x24, 0x2C);
	uint32_t border = FB_RGB(0x3C, 0x40, 0x4A);

	fb_.rect(pv_px_, pv_py_, pv_pw_, pv_ph_, panel);
	fb_.rect(pv_px_, pv_py_, pv_pw_, 1, border);
	fb_.rect(pv_px_, pv_py_ + pv_ph_ - 1, pv_pw_, 1, border);
	fb_.rect(pv_px_, pv_py_, 1, pv_ph_, border);
	fb_.rect(pv_px_ + pv_pw_ - 1, pv_py_, 1, pv_ph_, border);

	/* Title — truncated by character count to the content width. */
	int maxc = (pv_pw_ - 2 * PV_PAD) / FB_FONT_W;
	std::string t = w->title;
	if (maxc > 0 && (int)t.size() > maxc)
		t.resize((size_t)maxc);
	fb_.text(pv_px_ + PV_PAD, pv_py_ + 3, t.c_str(), FB_WHITE, panel);

	/* Scaled content. */
	blit_scaled(w, pv_px_ + PV_PAD, pv_py_ + PV_TITLE, pv_tw_, pv_th_);
}

/* Nearest-neighbour downscale of a window's live shared buffer into (dx,dy,dw,dh). */
void Compositor::blit_scaled(const Window *w, int dx, int dy, int dw, int dh)
{
	if (w == nullptr || w->buf == nullptr || w->w <= 0 || w->h <= 0 || dw <= 0 || dh <= 0)
		return;
	const uint32_t *buf = (const uint32_t *)w->buf;
	int stride = (int)(w->pitch / WIN_BPP);
	for (int yy = 0; yy < dh; yy++)
	{
		const uint32_t *srow = buf + (size_t)(yy * w->h / dh) * stride;
		for (int xx = 0; xx < dw; xx++)
			fb_.pixel(dx + xx, dy + yy, srow[xx * w->w / dw]);
	}
}

/* One separable box-blur pass along rows (horizontal); (dx,dy) index a running
 * sum of the 2r+1-wide window with edge clamping.  Integer-only (no SSE/float). */
static void box_blur_h(const uint32_t *src, uint32_t *dst, int w, int h, int r)
{
	int win = 2 * r + 1;
	for (int j = 0; j < h; j++)
	{
		const uint32_t *s = src + (size_t)j * w;
		uint32_t *d = dst + (size_t)j * w;
		int rs = 0, gs = 0, bs = 0;
		for (int k = -r; k <= r; k++)
		{
			int idx = k < 0 ? 0 : (k >= w ? w - 1 : k);
			rs += (s[idx] >> 16) & 0xFF;
			gs += (s[idx] >> 8) & 0xFF;
			bs += s[idx] & 0xFF;
		}
		for (int i = 0; i < w; i++)
		{
			d[i] = (uint32_t)((rs / win) << 16 | (gs / win) << 8 | (bs / win));
			int add = i + r + 1;
			add = add >= w ? w - 1 : add;
			int sub = i - r;
			sub = sub < 0 ? 0 : sub;
			rs += (int)((s[add] >> 16) & 0xFF) - (int)((s[sub] >> 16) & 0xFF);
			gs += (int)((s[add] >> 8) & 0xFF) - (int)((s[sub] >> 8) & 0xFF);
			bs += (int)(s[add] & 0xFF) - (int)(s[sub] & 0xFF);
		}
	}
}

/* Separable box-blur pass along columns (vertical); mirror of box_blur_h. */
static void box_blur_v(const uint32_t *src, uint32_t *dst, int w, int h, int r)
{
	int win = 2 * r + 1;
	for (int i = 0; i < w; i++)
	{
		int rs = 0, gs = 0, bs = 0;
		for (int k = -r; k <= r; k++)
		{
			int idx = k < 0 ? 0 : (k >= h ? h - 1 : k);
			const uint32_t p = src[(size_t)idx * w + i];
			rs += (p >> 16) & 0xFF;
			gs += (p >> 8) & 0xFF;
			bs += p & 0xFF;
		}
		for (int j = 0; j < h; j++)
		{
			dst[(size_t)j * w + i] = (uint32_t)((rs / win) << 16 | (gs / win) << 8 | (bs / win));
			int add = j + r + 1;
			add = add >= h ? h - 1 : add;
			int sub = j - r;
			sub = sub < 0 ? 0 : sub;
			const uint32_t pa = src[(size_t)add * w + i], ps = src[(size_t)sub * w + i];
			rs += (int)((pa >> 16) & 0xFF) - (int)((ps >> 16) & 0xFF);
			gs += (int)((pa >> 8) & 0xFF) - (int)((ps >> 8) & 0xFF);
			bs += (int)(pa & 0xFF) - (int)(ps & 0xFF);
		}
	}
}

void Compositor::blur_region(int x, int y, int w, int h, int radius)
{
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
	if (w <= 0 || h <= 0 || radius <= 0)
		return;

	size_t n = (size_t)w * h;
	blur_a_.assign(n, 0);
	blur_b_.assign(n, 0);
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++)
			blur_a_[(size_t)j * w + i] = fb_.read(x + i, y + j);

	/* Two passes for a smoother, more gaussian-like falloff than a single box. */
	for (int pass = 0; pass < 2; pass++)
	{
		box_blur_h(blur_a_.data(), blur_b_.data(), w, h, radius);
		box_blur_v(blur_b_.data(), blur_a_.data(), w, h, radius);
	}

	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++)
			fb_.pixel(x + i, y + j, blur_a_[(size_t)j * w + i]);
}

bool Compositor::over_blur_window(int x, int y, int w, int h) const
{
	for (auto *win : reg_.z_stack())
	{
		if (win->minimized || win->opacity >= 255 || win->blur_radius == 0)
			continue;
		int cy = win->y + win->decor_h;
		if (x < win->x + win->w && x + w > win->x && y < cy + win->h && y + h > win->y)
			return true;
	}
	return false;
}

int Compositor::switcher_begin()
{
	sw_ids_.clear();
	/* MRU order: z-stack back is topmost/most-recent, so iterate in reverse.
	 * Include minimized decorated windows (still in the z-stack) so Alt-Tab can
	 * restore them; skip closing windows and undecorated surfaces (taskbar, etc). */
	for (auto it = reg_.z_stack().rbegin(); it != reg_.z_stack().rend(); ++it)
	{
		Window *w = *it;
		if (w->decor_h > 0 && !w->closing)
			sw_ids_.push_back(w->id);
	}
	sw_sel_ = 0;
	switcher_ = true;
	invalidate_all();
	return (int)sw_ids_.size();
}

void Compositor::switcher_move(int dir)
{
	int n = (int)sw_ids_.size();
	if (n <= 0)
		return;
	sw_sel_ = ((sw_sel_ + dir) % n + n) % n;
	invalidate_all();
}

uint32_t Compositor::switcher_selected() const
{
	if (sw_sel_ < 0 || sw_sel_ >= (int)sw_ids_.size())
		return 0;
	return sw_ids_[sw_sel_];
}

void Compositor::switcher_end()
{
	switcher_ = false;
	sw_ids_.clear();
	invalidate_all();
}

/* Centred overlay: a flat panel holding a row of live window thumbnails, the
 * highlighted one framed in the accent colour with its title brightened. */
void Compositor::draw_switcher()
{
	int n = (int)sw_ids_.size();
	if (!switcher_ || n <= 0)
		return;

	const int gap = 16, pad = 20, labelh = FB_FONT_H + 4;
	int tile_w = 180, tile_h = 120;
	int maxpw = (int)fb_.width - 80;
	if (n * tile_w + (n - 1) * gap + 2 * pad > maxpw)
	{
		tile_w = (maxpw - 2 * pad - (n - 1) * gap) / n;
		if (tile_w < 60)
			tile_w = 60;
		tile_h = tile_w * 2 / 3;
	}
	int cw = n * tile_w + (n - 1) * gap;
	int panel_w = cw + 2 * pad, panel_h = tile_h + labelh + 2 * pad;
	int px = ((int)fb_.width - panel_w) / 2, py = ((int)fb_.height - panel_h) / 2;

	uint32_t panel = FB_RGB(0x1E, 0x20, 0x28), border = FB_RGB(0x3C, 0x40, 0x4A);
	fb_.rect(px, py, panel_w, panel_h, panel);
	fb_.rect(px, py, panel_w, 1, border);
	fb_.rect(px, py + panel_h - 1, panel_w, 1, border);
	fb_.rect(px, py, 1, panel_h, border);
	fb_.rect(px + panel_w - 1, py, 1, panel_h, border);

	int tx = px + pad, ty = py + pad;
	for (int i = 0; i < n; i++)
	{
		Window *w = reg_.find(sw_ids_[i]);
		int cx = tx + i * (tile_w + gap);
		bool sel = (i == sw_sel_);
		uint32_t tilebg = sel ? FB_RGB(0x2E, 0x34, 0x42) : panel;

		fb_.rect(cx - 4, ty - 4, tile_w + 8, tile_h + labelh + 4, tilebg);
		if (w != nullptr)
			blit_scaled(w, cx, ty, tile_w, tile_h);
		else
			fb_.rect(cx, ty, tile_w, tile_h, FB_RGB(0x30, 0x30, 0x38));

		if (sel)
		{
			uint32_t ac = g_theme_decor_bg;
			fb_.rect(cx - 4, ty - 4, tile_w + 8, 2, ac);
			fb_.rect(cx - 4, ty + tile_h + labelh - 2, tile_w + 8, 2, ac);
			fb_.rect(cx - 4, ty - 4, 2, tile_h + labelh + 4, ac);
			fb_.rect(cx + tile_w + 2, ty - 4, 2, tile_h + labelh + 4, ac);
		}

		if (w != nullptr)
		{
			int maxc = tile_w / FB_FONT_W;
			std::string t = w->title;
			if (maxc > 0 && (int)t.size() > maxc)
				t.resize((size_t)maxc);
			fb_.text(cx, ty + tile_h + 2, t.c_str(), sel ? FB_WHITE : FB_RGB(0xB0, 0xB4, 0xC0), tilebg);
		}
	}
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
		if (w->minimized)
			continue;
		/* A translucent window does not hide what's behind it, so it must not
		 * suppress the desktop refill — the blend needs a fresh background. */
		if (w->opacity < 255)
			continue;
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

		draw_window_shadow(w, rx, ry, rw, rh);

		/* Translucent+blur: blur the freshly-drawn backdrop over the window's full
		 * content region before the tint blends (flush() promoted the damage to the
		 * window bounds, so the whole backdrop is present — no seams, no accumulation). */
		if (w->opacity < 255 && w->blur_radius > 0)
			blur_region(w->x, cy, w->w, w->h, w->blur_radius);

		int x1 = (rx > w->x) ? rx : w->x;
		int y1 = (ry > cy) ? ry : cy;
		int x2 = (rx + rw < w->x + w->w) ? rx + rw : w->x + w->w;
		int y2 = (ry + rh < cy + w->h) ? ry + rh : cy + w->h;
		if (x2 > x1 && y2 > y1)
		{
			if (w->opacity < 255)
			{
				w->blend_rect(fb_, x1, y1, x2, y2);
			}
			else
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
		}

		if (w->decor_h > 0 && rx < w->x + w->w && rx + rw > w->x && ry < w->y + w->decor_h && ry + rh > w->y)
			w->draw_decor(fb_, w == reg_.focused());
		w->draw_grip(fb_);
		round_window_corners(w, rx, ry, rw, rh);
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

uint32_t Compositor::desk_pixel(int x, int y) const
{
	if (x >= 0 && y >= 0 && x < dc_w_ && y < dc_h_ && (int)desk_cache_.size() >= dc_w_ * dc_h_ &&
	    dc_w_ == (int)fb_.width)
		return desk_cache_[(size_t)y * dc_w_ + x];
	return fb_.read(x, y);
}

/* Soft drop shadow: darken the framebuffer in a band around the window, offset
 * downward, with a quadratic distance falloff (no sqrt — world is -mno-sse).
 * Reads-then-darkens so it is idempotent within a single repaint pass. */
void Compositor::draw_window_shadow(const Window *w, int clipx, int clipy, int clipw, int cliph)
{
	const int reach = 10;      /* shadow reach in px */
	const int y_off = 4;       /* downward offset (light from above) */
	const int alpha_max = 120; /* peak darkening alpha (/256) */

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
			/* The window itself is opaque — never shadow under it. */
			if (px >= wx0 && px < wx1 && py >= wy0 && py < wy1)
				continue;
			int dx = (px < rx0) ? (rx0 - px) : ((px >= rx1) ? (px - rx1 + 1) : 0);
			int dy = (py < ry0) ? (ry0 - py) : ((py >= ry1) ? (py - ry1 + 1) : 0);
			int d2 = dx * dx + dy * dy;
			if (d2 >= reach * reach)
				continue;
			int a = alpha_max * (reach * reach - d2) / (reach * reach);
			if (a <= 0)
				continue;
			fb_.pixel(px, py, ogSurface::ogBlendColor(fb_.read(px, py), 0, (uInt32)a));
		}
	}
}

/* Round the four outer window corners with a 1px anti-aliased ring, revealing
 * the cached desktop background outside the arc. */
void Compositor::round_window_corners(const Window *w, int clipx, int clipy, int clipw, int cliph)
{
	const int radius = 6;
	int wx0 = w->x, wy0 = w->y;
	int wx1 = w->x + w->w, wy1 = w->y + w->decor_h + w->h;

	/* (region-x0, region-y0, arc-center-x, arc-center-y) for each corner. */
	int corners[4][4] = {
	    {wx0, wy0, wx0 + radius, wy0 + radius},                           /* top-left */
	    {wx1 - radius, wy0, wx1 - 1 - radius, wy0 + radius},              /* top-right */
	    {wx0, wy1 - radius, wx0 + radius, wy1 - 1 - radius},              /* bottom-left */
	    {wx1 - radius, wy1 - radius, wx1 - 1 - radius, wy1 - 1 - radius}, /* bottom-right */
	};

	for (auto &c : corners)
	{
		int rx = c[0], ry = c[1], cxc = c[2], cyc = c[3];
		for (int py = ry; py < ry + radius; py++)
		{
			if (py < clipy || py >= clipy + cliph || py < 0 || py >= (int)fb_.height)
				continue;
			for (int px = rx; px < rx + radius; px++)
			{
				if (px < clipx || px >= clipx + clipw || px < 0 || px >= (int)fb_.width)
					continue;
				int dx = px - cxc, dy = py - cyc;
				int d2 = dx * dx + dy * dy;
				if (d2 <= (radius - 1) * (radius - 1))
					continue; /* inside — leave the window pixel */
				uint32_t bg = desk_pixel(px, py);
				if (d2 >= radius * radius)
					fb_.pixel(px, py, bg); /* outside — background */
				else
					fb_.pixel(
					    px, py, ogSurface::ogBlendColor(bg, fb_.read(px, py), 128)); /* AA edge */
			}
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
		draw_window_shadow(w, 0, 0, (int)fb_.width, (int)fb_.height);
		if (w->opacity < 255 && w->blur_radius > 0)
			blur_region(w->x, w->y + w->decor_h, w->w, w->h, w->blur_radius);
		w->blit_to(fb_);
		if (w->decor_h > 0)
			w->draw_decor(fb_, w == reg_.focused());
		w->draw_grip(fb_);
		round_window_corners(w, 0, 0, (int)fb_.width, (int)fb_.height);
	}
	if (resize_preview_)
	{
		uint32_t c = FB_WHITE;
		fb_.rect(rp_x_, rp_y_, rp_w_, 1, c);
		fb_.rect(rp_x_, rp_y_ + rp_h_ - 1, rp_w_, 1, c);
		fb_.rect(rp_x_, rp_y_, 1, rp_h_, c);
		fb_.rect(rp_x_ + rp_w_ - 1, rp_y_, 1, rp_h_, c);
	}
	draw_window_preview();
	draw_switcher();
	cursor_save(cur_x_, cur_y_);
	cursor_draw(cur_x_, cur_y_);
	cur_drawn_ = true;
}

void Compositor::partial_composite(int sx, int sy, int sw, int sh)
{
	if (!rect_covered(sx, sy, sw, sh))
		desktop_fill_rect(sx, sy, sw, sh);
	reblit_rect(sx, sy, sw, sh);
	if (preview_hit(sx, sy, sw, sh))
		draw_window_preview();
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

	/* Grow the damage to the full bounds of any translucent+blur window it touches,
	 * so the backdrop blur runs over a single freshly-composited region (no seams
	 * across partial-repaint boundaries, no blur-of-blur accumulation). */
	for (auto *win : reg_.z_stack())
	{
		if (win->minimized || win->opacity >= 255 || win->blur_radius == 0)
			continue;
		int fwx = win->x, fwy = win->y, fww = win->w, fwh = win->decor_h + win->h;
		if (fwx >= x + w || fwx + fww <= x || fwy >= y + h || fwy + fwh <= y)
			continue; /* no intersection */
		int x2 = (x + w > fwx + fww) ? x + w : fwx + fww;
		int y2 = (y + h > fwy + fwh) ? y + h : fwy + fwh;
		x = (x < fwx) ? x : fwx;
		y = (y < fwy) ? y : fwy;
		w = x2 - x;
		h = y2 - y;
	}
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

	/* Over a translucent+blur panel, the local desktop-fill + reblit would repaint
	 * a cursor-sized patch of unblurred backdrop (the blur is a region-wide op),
	 * leaving a smear.  cursor_erase() above already restored the correct blended
	 * pixels there, so skip the local reblit; a real content change comes through
	 * flush(), which repaints (and blurs) the whole panel and re-saves the cursor. */
	if (!over_blur_window(old_x, old_y, CUR_W, CUR_H))
	{
		if (!rect_covered(old_x, old_y, CUR_W, CUR_H))
			desktop_fill_rect(old_x, old_y, CUR_W, CUR_H);
		reblit_rect(old_x, old_y, CUR_W, CUR_H);
		/* If the cursor was sitting over the hover preview, the reblit just erased
		 * that slice of it — redraw the panel so it survives cursor motion. */
		if (preview_hit(old_x, old_y, CUR_W, CUR_H))
			draw_window_preview();
	}

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
