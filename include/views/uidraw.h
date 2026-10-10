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

/*
 * views/uidraw.h — small anti-aliased drawing kit for system UI clients
 * (taskbar, login, launcher, Settings) drawing into a 32bpp window buffer.
 *
 * Everything is integer-only (the i386 world is built -mno-sse) and works on a
 * plain 0x00RRGGBB pixel array, so it composes with objGFX text rendering on
 * the same surface.  Shapes blend against what is already drawn, giving clean
 * anti-aliased edges over any background.  Colours come from views/theme.h.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include <objgfx/ogImage.h>
#include <objgfx/ogButton.h>
#include <objgfx/ogScalableFont.h>
#include <views/theme.h>

/* A 32bpp pixel buffer: px[y * stride + x], low 24 bits = RGB. */
struct ui_canvas
{
	uint32_t *px = nullptr;
	int w = 0, h = 0, stride = 0;

	bool in(int x, int y) const
	{
		return x >= 0 && y >= 0 && x < w && y < h;
	}
	uint32_t get(int x, int y) const
	{
		return px[(size_t)y * stride + x] & 0x00FFFFFFu;
	}
	void put(int x, int y, uint32_t c)
	{
		if (in(x, y))
			px[(size_t)y * stride + x] = c & 0x00FFFFFFu;
	}
	/* Blend c over (x,y) by a in 0..256. */
	void blend(int x, int y, uint32_t c, uint32_t a)
	{
		if (in(x, y))
			px[(size_t)y * stride + x] = ui_mix(get(x, y), c, a);
	}
};

/**
 * True when clocks should use 24-hour time (/views/clock/24h, per user, default
 * 12-hour like the design).  user may be NULL for the system default.
 */
static inline bool ui_clock_24h(const char *user)
{
	int v = 0;
	const char *u = (user != nullptr && user[0] != '\0') ? user : nullptr;
	return ubistry_get_for_int(u, "views/clock/24h", &v) == 0 && v != 0;
}

/**
 * Format a clock face: "20:57" (24h) or "8:57 PM" (12h).
 */
static inline void ui_format_time(char *buf, size_t n, const struct tm *lt, bool h24)
{
	if (h24)
		snprintf(buf, n, "%02d:%02d", lt->tm_hour, lt->tm_min);
	else
		snprintf(buf, n, "%d:%02d %s", (lt->tm_hour + 11) % 12 + 1, lt->tm_min, lt->tm_hour < 12 ? "AM" : "PM");
}

/* A loaded icon: straight-alpha 0xAARRGGBB pixels (ogImage::LoadRGBA). */
struct ui_icon
{
	uint32_t *px = nullptr;
	int w = 0, h = 0;
};

/**
 * Load /usr/share/icons/<size>/<name>.png once and cache it (px is nullptr if
 * the file is missing, so callers can draw nothing or a fallback).
 */
static inline const ui_icon &ui_icon_get(int size, const char *name)
{
	static std::vector<std::pair<std::string, ui_icon>> cache;
	std::string key = std::to_string(size) + "/" + name;
	for (auto &e : cache)
		if (e.first == key)
			return e.second;
	ui_icon ic;
	ic.px = ogImage::LoadRGBA(("/usr/share/icons/" + key + ".png").c_str(), &ic.w, &ic.h);
	cache.push_back({key, ic});
	return cache.back().second;
}

/**
 * Floor integer square root (bitwise; no floating point).
 */
static inline uint32_t ui_isqrt(uint32_t v)
{
	uint32_t r = 0, bit = 1u << 30;
	while (bit > v)
		bit >>= 2;
	while (bit != 0)
	{
		if (v >= r + bit)
		{
			v -= r + bit;
			r = (r >> 1) + bit;
		}
		else
			r >>= 1;
		bit >>= 2;
	}
	return r;
}

/**
 * Signed distance, in quarter pixels, from pixel (px,py)'s centre to the edge
 * of the rounded rect (x,y,w,h,r): negative inside, positive outside.
 */
static inline int ui_rr_dist4(int px, int py, int x, int y, int w, int h, int r)
{
	int qx = std::abs(4 * px + 2 - (4 * x + 2 * w)) - (2 * w - 4 * r);
	int qy = std::abs(4 * py + 2 - (4 * y + 2 * h)) - (2 * h - 4 * r);
	int ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
	int outside = (ox && oy) ? (int)ui_isqrt((uint32_t)(ox * ox + oy * oy)) : (ox > oy ? ox : oy);
	int inside = qx > qy ? qx : qy;
	return outside + (inside < 0 ? inside : 0) - 4 * r;
}

/**
 * Coverage (0..256) of a pixel at signed quarter-pixel distance d (1px AA ramp).
 */
static inline uint32_t ui_cov(int d)
{
	int c = 128 - d * 64;
	return c <= 0 ? 0u : (c >= 256 ? 256u : (uint32_t)c);
}

static inline void ui_fill(ui_canvas &c, int x, int y, int w, int h, uint32_t col)
{
	for (int yy = y; yy < y + h; yy++)
		for (int xx = x; xx < x + w; xx++)
			c.put(xx, yy, col);
}

/* Anti-aliased rounded-rect fill (radius r), blended over what is drawn. */
static inline void ui_fill_round(ui_canvas &c, int x, int y, int w, int h, int r, uint32_t col)
{
	for (int yy = y; yy < y + h; yy++)
		for (int xx = x; xx < x + w; xx++)
		{
			bool corner = (xx < x + r || xx >= x + w - r) && (yy < y + r || yy >= y + h - r);
			if (!corner)
			{
				c.put(xx, yy, col);
				continue;
			}
			uint32_t a = ui_cov(ui_rr_dist4(xx, yy, x, y, w, h, r));
			if (a)
				c.blend(xx, yy, col, a);
		}
}

/* Rounded rect with a 1px outline (stroke) around a fill. */
static inline void ui_outlined_round(ui_canvas &c, int x, int y, int w, int h, int r, uint32_t stroke, uint32_t fillc)
{
	ui_fill_round(c, x, y, w, h, r, stroke);
	ui_fill_round(c, x + 1, y + 1, w - 2, h - 2, r > 0 ? r - 1 : 0, fillc);
}

/* A t-px stroke along the inside edge of a rounded rect (no fill), anti-aliased
 * on both edges — rings, outlined circles over busy backgrounds. */
static inline void ui_stroke_round(ui_canvas &c, int x, int y, int w, int h, int r, int t, uint32_t col)
{
	int ir = r > t ? r - t : 0;
	for (int yy = y; yy < y + h; yy++)
		for (int xx = x; xx < x + w; xx++)
		{
			uint32_t outer = ui_cov(ui_rr_dist4(xx, yy, x, y, w, h, r));
			uint32_t inner = ui_cov(ui_rr_dist4(xx, yy, x + t, y + t, w - 2 * t, h - 2 * t, ir));
			if (outer > inner)
				c.blend(xx, yy, col, outer - inner);
		}
}

/* A t-px rounded ring (stroke only) drawn `gap` px outside the rect — the focus
 * ring: radius grows with the offset so it stays concentric. */
static inline void ui_focus_ring(ui_canvas &c, int x, int y, int w, int h, int r, uint32_t col)
{
	const int gap = 2, t = 2;
	int ox = x - gap - t, oy = y - gap - t, ow = w + 2 * (gap + t), oh = h + 2 * (gap + t);
	int orad = r + gap + t, irad = r + gap;
	for (int yy = oy; yy < oy + oh; yy++)
		for (int xx = ox; xx < ox + ow; xx++)
		{
			uint32_t outer = ui_cov(ui_rr_dist4(xx, yy, ox, oy, ow, oh, orad));
			uint32_t inner = ui_cov(ui_rr_dist4(xx, yy, x - gap, y - gap, w + 2 * gap, h + 2 * gap, irad));
			if (outer > inner)
				c.blend(xx, yy, col, outer - inner);
		}
}

/* Composite an RGBA icon at (x,y) using its own colours. */
static inline void ui_draw_icon(ui_canvas &c, const ui_icon &ic, int x, int y)
{
	if (ic.px == nullptr)
		return;
	for (int yy = 0; yy < ic.h; yy++)
		for (int xx = 0; xx < ic.w; xx++)
		{
			uint32_t p = ic.px[yy * ic.w + xx], a = p >> 24;
			if (a)
				c.blend(x + xx, y + yy, p & 0x00FFFFFFu, a + (a >> 7));
		}
}

/* Draw an icon as a mask (alpha only), tinted col — for theme-coloured glyphs. */
static inline void ui_draw_icon_tint(ui_canvas &c, const ui_icon &ic, int x, int y, uint32_t col)
{
	if (ic.px == nullptr)
		return;
	for (int yy = 0; yy < ic.h; yy++)
		for (int xx = 0; xx < ic.w; xx++)
		{
			uint32_t a = ic.px[yy * ic.w + xx] >> 24;
			if (a)
				c.blend(x + xx, y + yy, col, a + (a >> 7));
		}
}

/* The focus hairline: 1px accent line fading to transparent over `fade` px at
 * each end (active title bars, the login card). */
static inline void ui_hairline(ui_canvas &c, int x, int y, int w, uint32_t col, int fade = 48)
{
	for (int i = 0; i < w; i++)
	{
		int e = i < w - 1 - i ? i : w - 1 - i;
		c.blend(x + i, y, col, e >= fade ? 256u : (uint32_t)(e * 256 / fade));
	}
}

/**
 * One soft offset shadow under a rounded rect (x,y,w,h,r): darkens toward
 * col by up to alpha /256 within `reach` px, quadratic falloff, shifted down by
 * yoff.  Pixels inside the shape are left alone (the shape is drawn on top).
 */
static inline void ui_shadow(
    ui_canvas &c, int x, int y, int w, int h, int r, int yoff, int reach, int alpha, uint32_t col)
{
	int r4 = 4 * reach;
	for (int yy = y + yoff - reach; yy < y + h + yoff + reach; yy++)
		for (int xx = x - reach; xx < x + w + reach; xx++)
		{
			if (!c.in(xx, yy))
				continue;
			int d = ui_rr_dist4(xx, yy - yoff, x, y, w, h, r);
			if (d >= r4)
				continue;
			int dd = d > 0 ? d : 0;
			uint32_t a = (uint32_t)(alpha * (r4 * r4 - dd * dd) / (r4 * r4));
			uint32_t in = ui_cov(ui_rr_dist4(xx, yy, x, y, w, h, r));
			a = a * (256 - in) >> 8;
			if (a)
				c.blend(xx, yy, col, a);
		}
}

/**
 * Give a translucent window its floating shape, as straight alpha in the top
 * byte of every pixel: a rounded rect (x,y,w,h,r) at fill_a opacity with an
 * opaque 1px edge, plus one soft offset shadow outside it; everything else is
 * fully clear.  Run after the content (RGB) is drawn.  The window must have
 * been claimed with display_claim_req.translucent = 1.
 */
static inline void ui_alpha_shape(ui_canvas &c,
                                  int x,
                                  int y,
                                  int w,
                                  int h,
                                  int r,
                                  uint8_t fill_a,
                                  uint32_t edge,
                                  int shadow_y,
                                  int shadow_reach,
                                  int shadow_a,
                                  uint32_t shadow_col)
{
	int reach4 = 4 * shadow_reach;
	for (int yy = 0; yy < c.h; yy++)
	{
		uint32_t *rp = c.px + (size_t)yy * c.stride;
		for (int xx = 0; xx < c.w; xx++)
		{
			int d = ui_rr_dist4(xx, yy, x, y, w, h, r);
			int cov = (int)ui_cov(d);
			int ring = 384 + d * 64; /* how much of the 1px edge band */
			ring = ring < 0 ? 0 : (ring > 256 ? 256 : ring);

			uint32_t rgb = rp[xx] & 0x00FFFFFFu;
			if (ring > 0 && cov > 0)
				rgb = ui_mix(rgb, edge, (uint32_t)ring);
			int a_fill = cov * (fill_a + ((255 - fill_a) * ring >> 8)) >> 8;

			int a_sh = 0;
			if (cov < 256)
			{
				int ds = ui_rr_dist4(xx, yy - shadow_y, x, y, w, h, r);
				if (ds < reach4)
				{
					int dd = ds > 0 ? ds : 0;
					a_sh = shadow_a * (reach4 * reach4 - dd * dd) / (reach4 * reach4);
					a_sh = a_sh * (256 - cov) >> 8;
				}
			}

			int a = a_fill + (a_sh * (255 - a_fill) / 255);
			if (a <= 0)
			{
				rp[xx] = 0;
				continue;
			}
			if (a_sh > 0 && a_fill < 255) /* shadow showing through the AA edge */
				rgb = ui_mix(shadow_col, rgb, (uint32_t)(a_fill * 256 / a));
			rp[xx] = ((uint32_t)(a > 255 ? 255 : a) << 24) | rgb;
		}
	}
}

/* Draw str with its left edge at x, vertically centred on row cy (transparent
 * background: glyphs blend over what is drawn). */
static inline void ui_text(ogSurface &s, ogScalableFont &f, int x, int cy, const char *str, uint32_t fg)
{
	f.SetFGColor((fg >> 16) & 0xFF, (fg >> 8) & 0xFF, fg & 0xFF, 255);
	f.SetBGColor(0, 0, 0, 0);
	f.PutString(s, x, cy - (int)f.GetHeight() / 2 + 1, str);
}

/* The same, horizontally centred on cx. */
static inline void ui_text_c(ogSurface &s, ogScalableFont &f, int cx, int cy, const char *str, uint32_t fg)
{
	ui_text(s, f, cx - (int)f.TextWidth(str) / 2, cy, str, fg);
}

/**
 * The restart / shut-down screen (ubixos-v2): near-black, the ring-and-dot
 * mark (40px ring, 4px accent-400 stroke, glowing accent-300 dot), then a
 * 16px title and a 12px muted subtitle, centred.  Fixed dark colours in both
 * themes — this is the machine going away, not an app.
 */
static inline void ui_power_screen(
    ui_canvas &c, ogSurface &s, ogScalableFont &title_f, ogScalableFont &sub_f, const char *title, const char *sub)
{
	ui_fill(c, 0, 0, c.w, c.h, 0x07080Du);
	int cx = c.w / 2, cy = c.h / 2 - 28;
	ui_stroke_round(c, cx - 20, cy - 20, 40, 40, 20, 4, 0xB5ABFCu);
	int dx = cx + 20 + 9 - 13, dy = cy - 20 - 9; /* right:-9px top:-9px, 13px dot */
	ui_shadow(c, dx, dy, 13, 13, 6, 0, 8, 140, 0x9184D9u);
	ui_fill_round(c, dx, dy, 13, 13, 6, 0xD2CEFDu);
	ui_text_c(s, title_f, cx, cy + 20 + 24 + 10, title, 0xE9E9EDu);
	ui_text_c(s, sub_f, cx, cy + 20 + 24 + 20 + 8 + 8, sub, 0x75798Cu);
}

/**
 * Style every ogButton as the design's secondary button for theme t: surface
 * fill, 1px edge, 6px radius; pressed = accent-900 fill with an accent edge.
 */
static inline void ui_style_buttons(const struct ui_theme &t)
{
	ogButton::style = {t.surface,
	                   t.accent_900,
	                   t.ground,
	                   t.edge,
	                   t.accent,
	                   ui_mix(t.ground, t.edge, 128),
	                   t.text,
	                   t.text_faint,
	                   (uInt32)UI_R_MD};
}
