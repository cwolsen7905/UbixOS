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
#include "icanvas.hh"
#include "framebuffer.hh"

/* Decoration colour palette (Windows 11 "flat" chrome).  The focused title bar
 * is filled with the user's accent colour (g_theme_decor_bg), resolved from the
 * registry by the Compositor; unfocused bars use a neutral dim fill.  Buttons
 * are hand-rasterised glyphs drawn straight over the bar (no per-button cell
 * fill) — the close glyph tints red so it stays the obvious target. */
extern uint32_t g_theme_decor_bg;                /* focused title bar fill (accent) */
extern uint32_t g_theme_decor_hi;                /* (legacy) kept for the Compositor's setter */
#define DECOR_BG_INACT FB_RGB(0x2A, 0x2A, 0x30)  /* unfocused title bar fill */
#define DECOR_CLOSE_RED FB_RGB(0xFF, 0x6E, 0x6E) /* close-glyph accent (bright for contrast) */

/**
 * Linearly blend two packed 0x00RRGGBB colours: t=0 → a, t=256 → b.
 */
static inline uint32_t decor_blend(uint32_t a, uint32_t b, int t)
{
	int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
	int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
	int r = ar + ((br - ar) * t >> 8);
	int g = ag + ((bg - ag) * t >> 8);
	int bl = ab + ((bb - ab) * t >> 8);
	return FB_RGB(r, g, bl);
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

	bool in_close_btn(int cx, int cy) const
	{
		return in_decor(cx, cy) && cx >= x + w - decor_h;
	}

	/* Minimize button: the title-bar square immediately left of the close box. */
	bool in_min_btn(int cx, int cy) const
	{
		return in_decor(cx, cy) && cx >= x + w - 2 * decor_h && cx < x + w - decor_h;
	}

	/* Maximize button: left of minimize; only resizable windows show it. */
	bool in_max_btn(int cx, int cy) const
	{
		return resizable() && in_decor(cx, cy) && cx >= x + w - 3 * decor_h && cx < x + w - 2 * decor_h;
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

	void draw_decor(ICanvas &canvas, bool is_focused) const
	{
		/* Flat fill; focus dimming blends the accent toward neutral grey. */
		uint32_t bg = is_focused ? g_theme_decor_bg : DECOR_BG_INACT;
		uint32_t glyph = is_focused ? FB_RGB(0xF0, 0xF0, 0xF4) : FB_RGB(0x80, 0x80, 0x90);
		uint32_t txt = is_focused ? FB_WHITE : FB_RGB(0x9A, 0x9A, 0xAA);

		canvas.rect(x, y, w, decor_h, bg);
		/* Hairline at the content boundary — subtle, just darker than the bar. */
		canvas.rect(x, y + decor_h - 1, w, 1, decor_blend(bg, 0, 70));

		canvas.text(x + 10, y + (decor_h - FB_FONT_H) / 2, title.c_str(), txt, bg);

		int cy = y + decor_h / 2;

		/* Maximize / restore — only resizable windows. */
		if (resizable())
		{
			int mcx = x + w - 3 * decor_h + decor_h / 2;
			if (maximized)
			{
				/* Restore: two overlapped squares. */
				glyph_square(canvas, mcx + 2, cy - 2, 8, glyph);
				canvas.rect(mcx - 4, cy + 1, 8, 1, glyph);
				canvas.rect(mcx - 4, cy + 1, 1, 6, glyph);
				canvas.rect(mcx - 4, cy + 6, 8, 1, glyph);
				canvas.rect(mcx + 3, cy + 1, 1, 6, glyph);
			}
			else
			{
				glyph_square(canvas, mcx, cy, 11, glyph);
			}
		}

		/* Minimize — a centred horizontal line. */
		int icx = x + w - 2 * decor_h + decor_h / 2;
		canvas.rect(icx - 5, cy, 11, 2, glyph);

		/* Close — bright red X, far right. */
		int ccx = x + w - decor_h + decor_h / 2;
		glyph_close(canvas, ccx, cy, 5, is_focused ? DECOR_CLOSE_RED : glyph);
	}

	void blit_to(ICanvas &canvas) const
	{
		canvas.blit(x, y + decor_h, w, h, (const uint32_t *)buf, (int)(pitch / WIN_BPP));
	}

	/* Draw the resize grip (three diagonal dots) at the bottom-right corner. */
	void draw_grip(ICanvas &canvas) const
	{
		if (!resizable())
			return;
		uint32_t gc = FB_RGB(0xC0, 0xC0, 0xD0);
		int bx = x + w - 3, by = y + decor_h + h - 3;
		for (int i = 0; i < 3; i++)
			canvas.rect(bx - i * 4, by - i * 4, 2, 2, gc);
	}
};
