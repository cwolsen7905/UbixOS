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

#include <memory>
#include <vector>
#include "window.hh"

/* ------------------------------------------------------------------ */
/* WindowRegistry — owns window storage and Z-order                   */
/* ------------------------------------------------------------------ */

class WindowRegistry {
	std::vector<std::unique_ptr<Window>> windows_;
	std::vector<Window *>                z_stack_;
	uint32_t                             next_id_;
	int                                  cascade_x_;
	int                                  cascade_y_;
	Window                              *focused_;

public:
	WindowRegistry();

	/* Window lifecycle */
	Window  *alloc();
	void     destroy(Window *w);
	Window  *find(uint32_t id) const;

	/* Z-order */
	void     z_push(Window *w);
	void     z_remove(Window *w);
	void     z_raise(Window *w);

	/* Focus */
	Window  *focused() const { return focused_; }
	void     set_focused(Window *w) { focused_ = w; }

	/* Z-order access for compositor */
	const std::vector<Window *> &z_stack() const { return z_stack_; }

	/* Cascade placement: writes wx/wy and advances the cascade origin.
	 * Caller provides the window dimensions and framebuffer size. */
	void next_cascade(int32_t *out_x, int32_t *out_y,
	                  int32_t ww, int32_t dh, int32_t wh,
	                  uint32_t fb_w, uint32_t fb_h);

	/* Clamp every window's origin so it stays on a screen of sw x sh (used
	 * after a live resolution change). */
	void clamp_to(int sw, int sh);
};
