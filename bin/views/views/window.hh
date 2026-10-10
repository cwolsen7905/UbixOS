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

#pragma once

#include <string>
#include <objgfx/objgfx.h>
#include <objgfx/ogImage.h>
#include <views/theme.h>
#include <views/appicons.h>
#include "icanvas.hh"
#include "framebuffer.hh"

/* System palette (include/views/theme.h), resolved per session user by the
 * Compositor from views/theme/{mode,accent}; window chrome reads it on every
 * paint so a DISPLAY_THEME refresh restyles all windows at once. */
extern struct ui_theme g_ui;

/* Window-control glyphs: 16px monochrome PNGs from the ubixos-v2 icon set,
 * used as alpha masks and tinted with the theme glyph colour.  Loaded once;
 * a missing icon falls back to the hand-rasterised glyph. */
enum win_glyph
{
	WG_MINIMIZE,
	WG_MAXIMIZE,
	WG_RESTORE,
	WG_CLOSE,
	WG_PIN,
	WG_COUNT
};

struct win_glyph_mask
{
	uint32_t *px = nullptr; /* 0xAARRGGBB; only alpha is used */
	int w = 0, h = 0;
};

/**
 * Return the cached mask for a window-control glyph (px == nullptr if absent).
 */
static inline const win_glyph_mask &win_glyph_get(win_glyph g)
{
	static win_glyph_mask masks[WG_COUNT];
	static bool loaded = false;
	if (!loaded)
	{
		static const char *const names[WG_COUNT] = {
		    "win-minimize", "win-maximize", "win-restore", "win-close", "win-pin"};
		loaded = true;
		for (int i = 0; i < WG_COUNT; i++)
		{
			std::string path = std::string("/usr/share/icons/16/") + names[i] + ".png";
			masks[i].px = ogImage::LoadRGBA(path.c_str(), &masks[i].w, &masks[i].h);
		}
	}
	return masks[g];
}

/* ------------------------------------------------------------------ */
/* Window — client window state and rendering helpers                  */
/* ------------------------------------------------------------------ */

class Window
{
      public:
	uint32_t id;
	int32_t x, y, w, h;
	uint32_t pitch;
	void *buf; /* page-aligned region shared with client */
	int decor_h;
	bool closing = false;                           /* close button clicked; awaiting DISPLAY_RELEASE */
	bool minimized = false;                         /* hidden to the taskbar; restored via DISPLAY_RAISE */
	bool maximized = false;                         /* filling the screen; restore returns to saved_* */
	bool wants_motion = false;                      /* client opted into pointer-motion events (DISPLAY_CLAIM) */
	bool translucent = false;                       /* buffer top byte is alpha; blended, no server shadow */
	int hover_ctl = -1;                             /* control under the cursor: CTL_* or -1 */
	bool ctl_pressed = false;                       /* button held down on hover_ctl */
	bool pinned = false;                            /* keep-on-top: stays above unpinned windows */
	int sender_pid = 0;                             /* client PID — needed to re-share the buffer on resize */
	int min_w = 0, min_h = 0, max_w = 0, max_h = 0; /* resize constraints (0 = fixed) */
	int saved_x = 0, saved_y = 0, saved_w = 0, saved_h = 0; /* geometry before maximize/snap */
	std::string title;
	std::string mbox;

	bool resizable() const
	{
		return max_w > min_w || max_h > min_h;
	}

	bool hit_test(int cx, int cy) const
	{
		return cx >= x && cx < x + w && cy >= y && cy < y + decor_h + h;
	}

	bool in_decor(int cx, int cy) const
	{
		return decor_h > 0 && cx >= x && cx < x + w && cy >= y && cy < y + decor_h;
	}

	/* Window controls: round UI_WINBTN buttons in a capsule at the right of the
	 * title bar — [keep-on-top] | [minimize][maximize][close]; maximize only
	 * when resizable.  A 1px divider (3px margins) follows the pin. */
	enum
	{
		CTL_MIN = 0,
		CTL_MAX = 1,
		CTL_CLOSE = 2,
		CTL_PIN = 3
	};
	static const int CAPS_DIV = 7; /* divider + margins after the pin */

	int ctl_count() const
	{
		return resizable() ? 4 : 3;
	}
	int caps_w() const
	{
		return 2 * 3 + ctl_count() * UI_WINBTN + (ctl_count() - 1) * 2 + CAPS_DIV;
	}
	int caps_x() const
	{
		return x + w - 4 - caps_w();
	}
	int caps_y() const
	{
		return y + (decor_h - UI_WINCAPS_H) / 2;
	}
	/* Left x of control slot i (0-based, left to right). */
	int ctl_x(int i) const
	{
		return caps_x() + 3 + i * (UI_WINBTN + 2) + (i >= 1 ? CAPS_DIV : 0);
	}
	bool in_ctl(int cx, int cy, int i) const
	{
		int by = caps_y() + (UI_WINCAPS_H - UI_WINBTN) / 2;
		return in_decor(cx, cy) && cx >= ctl_x(i) && cx < ctl_x(i) + UI_WINBTN && cy >= by &&
		       cy < by + UI_WINBTN;
	}

	/* Kind of the control in slot i. */
	int ctl_kind(int i) const
	{
		if (i == 0)
			return CTL_PIN;
		if (i == ctl_count() - 1)
			return CTL_CLOSE;
		return i == 1 ? CTL_MIN : CTL_MAX;
	}

	/* The control under (cx, cy), or -1. */
	int ctl_at(int cx, int cy) const
	{
		for (int i = 0; i < ctl_count(); i++)
			if (in_ctl(cx, cy, i))
				return ctl_kind(i);
		return -1;
	}

	bool in_close_btn(int cx, int cy) const
	{
		return ctl_at(cx, cy) == CTL_CLOSE;
	}

	bool in_min_btn(int cx, int cy) const
	{
		return ctl_at(cx, cy) == CTL_MIN;
	}

	bool in_max_btn(int cx, int cy) const
	{
		return ctl_at(cx, cy) == CTL_MAX;
	}

	bool in_pin_btn(int cx, int cy) const
	{
		return ctl_at(cx, cy) == CTL_PIN;
	}

	/* Resize edges (design: 6px edges, 14px corners; the top band is 4px so
	 * the title bar stays draggable).  Bitmask of RZ_* or 0. */
	enum
	{
		RZ_L = 1,
		RZ_R = 2,
		RZ_T = 4,
		RZ_B = 8
	};
	int resize_edges_at(int cx, int cy) const
	{
		if (!resizable() || maximized || !hit_test(cx, cy))
			return 0;
		int bottom = y + decor_h + h;
		const int E = 6, C = 14;
		bool l = cx < x + E, r = cx >= x + w - E, t = cy < y + 4, b = cy >= bottom - E;
		/* corners grab both axes over a larger area */
		bool cl = cx < x + C, cr = cx >= x + w - C, ct = cy < y + C, cb = cy >= bottom - C;
		if ((l || b) && cl && cb)
			return RZ_L | RZ_B;
		if ((r || b) && cr && cb)
			return RZ_R | RZ_B;
		if ((l || t) && cl && ct)
			return RZ_L | RZ_T;
		if ((r || t) && cr && ct)
			return RZ_R | RZ_T;
		return (l ? RZ_L : 0) | (r ? RZ_R : 0) | (t ? RZ_T : 0) | (b ? RZ_B : 0);
	}

	/* Resize grip: a small square at the bottom-right corner of the content. */
	static const int GRIP = 14;
	bool in_resize_grip(int cx, int cy) const
	{
		int by = y + decor_h + h;
		return resizable() && cx >= x + w - GRIP && cx < x + w && cy >= by - GRIP && cy < by;
	}

	/* Crisp X (close glyph): two 2px-thick diagonals of half-extent r. */
	static void glyph_close(ICanvas &c, int cx, int cy, int r, uint32_t col)
	{
		for (int i = -r; i <= r; i++)
		{
			c.pixel(cx + i, cy + i, col);
			c.pixel(cx + i + 1, cy + i, col);
			c.pixel(cx + i, cy - i, col);
			c.pixel(cx + i + 1, cy - i, col);
		}
	}

	/* Square outline (maximize glyph) of side s centred on (cx, cy). */
	static void glyph_square(ICanvas &c, int cx, int cy, int s, uint32_t col)
	{
		int lx = cx - s / 2, ty = cy - s / 2;
		c.rect(lx, ty, s, 1, col);
		c.rect(lx, ty + s - 1, s, 1, col);
		c.rect(lx, ty, 1, s, col);
		c.rect(lx + s - 1, ty, 1, s, col);
	}

	/* Fill a w x h rounded rect (radius r) with colour col, anti-aliased by
	 * blending against what is already on the canvas. */
	static void fill_round(ICanvas &c, int rx, int ry, int rw, int rh, int r, uint32_t col)
	{
		for (int py = 0; py < rh; py++)
		{
			for (int px = 0; px < rw; px++)
			{
				int dx = 0, dy = 0;
				if (px < r)
					dx = r - px;
				else if (px >= rw - r)
					dx = px - (rw - r - 1);
				if (py < r)
					dy = r - py;
				else if (py >= rh - r)
					dy = py - (rh - r - 1);
				if (dx == 0 || dy == 0)
				{
					c.pixel(rx + px, ry + py, col);
					continue;
				}
				/* coverage of this pixel by the corner disc (r - 0.5 radius) */
				int d2 = (dx * 2 - 1) * (dx * 2 - 1) + (dy * 2 - 1) * (dy * 2 - 1); /* (2d)^2 */
				int in2 = (2 * r - 2) * (2 * r - 2), out2 = (2 * r) * (2 * r);
				if (d2 <= in2)
					c.pixel(rx + px, ry + py, col);
				else if (d2 < out2)
				{
					uint32_t t = (uint32_t)(256 * (out2 - d2) / (out2 - in2));
					c.pixel(rx + px, ry + py, ui_mix(c.read(rx + px, ry + py), col, t));
				}
			}
		}
	}

	/* Draw a glyph mask centred in a cx,cy-centred 16px cell, tinted col. */
	static void draw_mask(ICanvas &c, const win_glyph_mask &m, int cx, int cy, uint32_t col)
	{
		int ox = cx - m.w / 2, oy = cy - m.h / 2;
		for (int py = 0; py < m.h; py++)
			for (int px = 0; px < m.w; px++)
			{
				uint32_t a = m.px[py * m.w + px] >> 24;
				if (a == 0)
					continue;
				int tx = ox + px, ty = oy + py;
				c.pixel(tx, ty, a == 255 ? col : ui_mix(c.read(tx, ty), col, a + (a >> 7)));
			}
	}

	/* One window-control glyph: the PNG mask when present, else hand-drawn. */
	void draw_ctl_glyph(ICanvas &c, win_glyph g, int cx, int cy, uint32_t col) const
	{
		const win_glyph_mask &m = win_glyph_get(g);
		if (m.px != nullptr)
		{
			draw_mask(c, m, cx, cy, col);
			return;
		}
		switch (g)
		{
			case WG_MINIMIZE:
				c.rect(cx - 4, cy, 9, 1, col);
				break;
			case WG_MAXIMIZE:
				glyph_square(c, cx, cy, 9, col);
				break;
			case WG_PIN:
				c.rect(cx - 1, cy - 5, 3, 7, col); /* pin head + shaft */
				c.rect(cx, cy + 2, 1, 4, col);
				break;
			case WG_RESTORE:
				glyph_square(c, cx + 1, cy - 1, 7, col);
				glyph_square(c, cx - 1, cy + 1, 7, col);
				break;
			default:
				glyph_close(c, cx, cy, 3, col);
				break;
		}
	}

	void draw_decor(ICanvas &canvas, bool is_focused) const
	{
		const ui_theme &t = g_ui;
		uint32_t bg = is_focused ? t.surface : t.ground;
		uint32_t txt = is_focused ? t.text : t.text_muted;
		/* inactive glyphs at 55% opacity (--opacity-inactive-glyph) */
		uint32_t glyph = is_focused ? t.glyph : ui_mix(bg, t.glyph, 141);

		canvas.rect(x, y, w, decor_h, bg);

		/* Focus hairline: 1px accent across the top, fading to the bar colour
		 * over 48px at each end (the design's only gradient). */
		if (is_focused)
		{
			const int fade = 48;
			for (int i = 0; i < w; i++)
			{
				int e = i < w - 1 - i ? i : w - 1 - i;
				uint32_t a = e >= fade ? 256u : (uint32_t)(e * 256 / fade);
				canvas.pixel(x + i, y + 1, ui_mix(bg, t.accent, a)); /* row 0 is the window border */
			}
		}
		/* Divider between the title bar and the client area. */
		canvas.rect(x, y + decor_h - 1, w, 1, ui_mix(bg, t.divider, t.divider_a));

		/* Title, vertically centred; clipped so it never runs under the controls. */
		std::string ttl = title;
		/* App icon (16px, views/appicons.h); 55% opacity when unfocused. */
		{
			ui_app_icon_spec sp = ui_app_icon(title, "", 16, t);
			const ui_icon &ic = ui_icon_get(sp.size, sp.name);
			int ix = x + 12 + sp.dx, iy = y + (decor_h - 16) / 2 + sp.dy;
			for (int py = 0; ic.px && py < ic.h; py++)
				for (int px = 0; px < ic.w; px++)
				{
					uint32_t p = ic.px[py * ic.w + px], a = p >> 24;
					if (a == 0)
						continue;
					a = a + (a >> 7);
					if (!is_focused)
						a = a * 141 >> 8;
					uint32_t col = sp.tint ? sp.color : (p & 0x00FFFFFFu);
					canvas.pixel(ix + px, iy + py, ui_mix(canvas.read(ix + px, iy + py), col, a));
				}
		}
		int title_x = x + 12 + 16 + 8;
		int max_chars = (caps_x() - title_x - 8) / 7; /* ~7px average advance at 12px */
		if (max_chars < 1)
			max_chars = 1;
		if ((int)ttl.size() > max_chars)
			ttl = ttl.substr(0, max_chars > 3 ? max_chars - 3 : 1) + "...";
		canvas.text(title_x, y + (decor_h - FB_FONT_H) / 2, ttl.c_str(), txt, bg);

		/* Controls capsule: ground fill + 1px edge (transparent when unfocused). */
		int cxp = caps_x(), cyp = caps_y();
		if (is_focused)
		{
			fill_round(canvas, cxp, cyp, caps_w(), UI_WINCAPS_H, UI_WINCAPS_H / 2, t.edge);
			fill_round(
			    canvas, cxp + 1, cyp + 1, caps_w() - 2, UI_WINCAPS_H - 2, UI_WINCAPS_H / 2 - 1, t.ground);
		}

		int gy = cyp + UI_WINCAPS_H / 2;
		int by = cyp + (UI_WINCAPS_H - UI_WINBTN) / 2;
		/* hover / pressed: a filled circle behind the glyph (close goes red) */
		for (int k = 0; k < ctl_count(); k++)
		{
			int kind = ctl_kind(k);
			if (kind != hover_ctl)
				continue;
			bool close = (kind == CTL_CLOSE);
			uint32_t fill = ctl_pressed ? (close ? t.danger_pressed : t.accent_800)
			                            : (close ? t.danger_hover : t.neutral_800);
			fill_round(canvas, ctl_x(k), by, UI_WINBTN, UI_WINBTN, UI_WINBTN / 2, fill);
		}
		/* pin: "on" tint when pinned, divider after it */
		if (pinned && hover_ctl != CTL_PIN)
			fill_round(canvas, ctl_x(0), by, UI_WINBTN, UI_WINBTN, UI_WINBTN / 2, t.accent_900);
		canvas.rect(ctl_x(1) - CAPS_DIV / 2 - 1,
		            gy - 6,
		            1,
		            12,
		            ui_mix(is_focused ? t.ground : bg, t.divider, t.divider_a));
		draw_ctl_glyph(canvas, WG_PIN, ctl_x(0) + UI_WINBTN / 2, gy, pinned ? t.accent_400 : glyph);
		int slot = 1;
		draw_ctl_glyph(canvas, WG_MINIMIZE, ctl_x(slot++) + UI_WINBTN / 2, gy, glyph);
		if (resizable())
			draw_ctl_glyph(
			    canvas, maximized ? WG_RESTORE : WG_MAXIMIZE, ctl_x(slot++) + UI_WINBTN / 2, gy, glyph);
		draw_ctl_glyph(canvas, WG_CLOSE, ctl_x(slot) + UI_WINBTN / 2, gy, glyph);
	}

	void blit_to(ICanvas &canvas) const
	{
		canvas.blit(x, y + decor_h, w, h, (const uint32_t *)buf, (int)(pitch / WIN_BPP));
	}

	/* Draw the resize grip (three diagonal dots) at the bottom-right corner. */
	void draw_grip(ICanvas &canvas) const
	{
		if (!resizable() || maximized)
			return;
		uint32_t gc = g_ui.text_faint;
		int bx = x + w - 4, by = y + decor_h + h - 4;
		for (int i = 0; i < 3; i++)
			canvas.rect(bx - i * 4, by - i * 4, 2, 2, gc);
	}
};
