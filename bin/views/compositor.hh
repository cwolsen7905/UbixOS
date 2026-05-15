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

#include "framebuffer.hh"
#include "window_registry.hh"

#define CUR_W 12
#define CUR_H 12

/* ------------------------------------------------------------------ */
/* Compositor — framebuffer ownership, desktop, cursor, blitting       */
/* ------------------------------------------------------------------ */

class Compositor {
	Framebuffer     fb_;
	WindowRegistry &reg_;

	uint32_t cur_saved_[CUR_H * CUR_W];
	int      cur_x_;
	int      cur_y_;
	bool     cur_drawn_;

	void desktop_fill_rect(int x, int y, int w, int h);
	void draw_desktop();
	void reblit_rect(int rx, int ry, int rw, int rh);
	void cursor_save(int x, int y);
	void cursor_erase(int x, int y);
	void cursor_draw(int x, int y);

public:
	explicit Compositor(WindowRegistry &reg);

	int  init();
	void startup();
	void composite_all();
	void partial_composite(int sx, int sy, int sw, int sh);
	void cursor_move(int dx, int dy);

	uint32_t screen_w() const { return fb_.width; }
	uint32_t screen_h() const { return fb_.height; }
	int      cur_x()    const { return cur_x_; }
	int      cur_y()    const { return cur_y_; }
};
