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
#include "framebuffer.hh"

/* Decoration colour palette */
#define DECOR_BG       FB_RGB(0x28, 0x48, 0x70)
#define DECOR_HI       FB_RGB(0x40, 0x70, 0xA8)
#define DECOR_BG_INACT FB_RGB(0x28, 0x28, 0x38)
#define DECOR_HI_INACT FB_RGB(0x38, 0x38, 0x50)
#define DECOR_SEP      FB_RGB(0x10, 0x20, 0x30)
#define DECOR_CLOSE_BG FB_RGB(0x90, 0x22, 0x22)

/* ------------------------------------------------------------------ */
/* Window — client window state and rendering helpers                  */
/* ------------------------------------------------------------------ */

class Window {
public:
	uint32_t     id;
	int32_t      x, y, w, h;
	uint32_t     pitch;
	void        *buf;       /* page-aligned region shared with client */
	int          decor_h;
	std::string  title;
	std::string  mbox;

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

	void draw_decor(Framebuffer &fb, bool is_focused) const {
		uint32_t bg = is_focused ? DECOR_BG    : DECOR_BG_INACT;
		uint32_t hi = is_focused ? DECOR_HI    : DECOR_HI_INACT;
		fb.rect(x, y,               w, decor_h, bg);
		fb.rect(x, y,               w, 1,       hi);
		fb.rect(x, y + decor_h - 1, w, 1,       DECOR_SEP);
		fb.text(x + 6, y + (decor_h - FB_FONT_H) / 2,
		    title.c_str(), FB_WHITE, bg);
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
