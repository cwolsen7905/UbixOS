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

/* Decoration colour palette.  The focused title bar uses the user's accent
 * colour (g_theme_decor_*), resolved from the registry by the Compositor; the
 * rest are fixed.  Defaults match the historical blue. */
extern uint32_t g_theme_decor_bg; /* focused title bar fill (accent) */
extern uint32_t g_theme_decor_hi; /* focused title bar top highlight */
#define DECOR_BG_INACT FB_RGB(0x28, 0x28, 0x38)
#define DECOR_HI_INACT FB_RGB(0x38, 0x38, 0x50)
#define DECOR_SEP FB_RGB(0x10, 0x20, 0x30)
#define DECOR_CLOSE_BG FB_RGB(0x90, 0x22, 0x22)
#define DECOR_MIN_BG FB_RGB(0x40, 0x44, 0x50)

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
	int sender_pid = 0;                             /* client PID — needed to re-share the buffer on resize */
	int min_w = 0, min_h = 0, max_w = 0, max_h = 0; /* resize constraints (0 = fixed) */
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

	/* Resize grip: a small square at the bottom-right corner of the content. */
	static const int GRIP = 14;
	bool in_resize_grip(int cx, int cy) const
	{
		int by = y + decor_h + h;
		return resizable() && cx >= x + w - GRIP && cx < x + w && cy >= by - GRIP && cy < by;
	}

	void draw_decor(ICanvas &canvas, bool is_focused) const
	{
		uint32_t bg = is_focused ? g_theme_decor_bg : DECOR_BG_INACT;
		uint32_t hi = is_focused ? g_theme_decor_hi : DECOR_HI_INACT;
		canvas.rect(x, y, w, decor_h, bg);
		canvas.rect(x, y, w, 1, hi);
		canvas.rect(x, y + decor_h - 1, w, 1, DECOR_SEP);
		canvas.text(x + 6, y + (decor_h - FB_FONT_H) / 2, title.c_str(), FB_WHITE, bg);

		int mbx = x + w - 2 * decor_h;
		canvas.rect(mbx, y + 1, decor_h - 1, decor_h - 2, DECOR_MIN_BG);
		canvas.ch(mbx + (decor_h - FB_FONT_W) / 2, y + (decor_h - FB_FONT_H) / 2, '_', FB_WHITE, DECOR_MIN_BG);

		int cbx = x + w - decor_h;
		canvas.rect(cbx, y + 1, decor_h - 1, decor_h - 2, DECOR_CLOSE_BG);
		canvas.ch(
		    cbx + (decor_h - FB_FONT_W) / 2, y + (decor_h - FB_FONT_H) / 2, 'X', FB_WHITE, DECOR_CLOSE_BG);
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
