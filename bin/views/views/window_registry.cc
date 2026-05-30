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

#include <algorithm>
#include "window_registry.hh"

#define CASCADE_STEP  24
#define CASCADE_START 20

WindowRegistry::WindowRegistry()
    : next_id_(1)
    , cascade_x_(CASCADE_START)
    , cascade_y_(CASCADE_START)
    , focused_(nullptr)
{}

Window *
WindowRegistry::alloc()
{
	auto w   = std::make_unique<Window>();
	Window *p = w.get();
	windows_.push_back(std::move(w));
	return p;
}

void
WindowRegistry::destroy(Window *w)
{
	auto it = std::find_if(windows_.begin(), windows_.end(),
	    [w](const std::unique_ptr<Window> &p) { return p.get() == w; });
	if (it != windows_.end())
		windows_.erase(it);
}

Window *
WindowRegistry::find(uint32_t id) const
{
	for (auto &w : windows_)
		if (w->id == id)
			return w.get();
	return nullptr;
}

void
WindowRegistry::z_push(Window *w)
{
	z_stack_.push_back(w);
}

void
WindowRegistry::z_remove(Window *w)
{
	auto it = std::find(z_stack_.begin(), z_stack_.end(), w);
	if (it != z_stack_.end())
		z_stack_.erase(it);
}

void
WindowRegistry::z_raise(Window *w)
{
	z_remove(w);
	z_push(w);
}

void
WindowRegistry::next_cascade(int32_t *out_x, int32_t *out_y,
    int32_t ww, int32_t dh, int32_t wh,
    uint32_t fb_w, uint32_t fb_h)
{
	*out_x = (int32_t)cascade_x_;
	*out_y = (int32_t)cascade_y_;
	cascade_x_ += CASCADE_STEP;
	cascade_y_ += CASCADE_STEP;
	if (cascade_x_ + ww > (int32_t)fb_w ||
	    cascade_y_ + dh + wh > (int32_t)fb_h) {
		cascade_x_ = CASCADE_START;
		cascade_y_ = CASCADE_START;
	}
}
