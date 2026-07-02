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
#include <vector>
#include "framebuffer.hh"
#include "window_registry.hh"

#define CUR_W 12
#define CUR_H 12

/* ------------------------------------------------------------------ */
/* Compositor — framebuffer ownership, desktop, cursor, blitting       */
/* ------------------------------------------------------------------ */

class Compositor
{
	/* Axis-aligned bounding box of all pending damage this tick.
	 * Events call invalidate*(); flush() renders once at tick end. */
	struct DamageRect
	{
		int x, y, w, h;
		bool valid;
	};

	Framebuffer fb_;
	WindowRegistry &reg_;

	uint32_t cur_saved_[CUR_H * CUR_W];
	int cur_x_;
	int cur_y_;
	bool cur_drawn_;
	DamageRect damage_;

	/* Desktop background, resolved from the registry on each refresh. */
	enum
	{
		DESK_IMAGE = 0,
		DESK_SOLID = 1,
		DESK_BARS = 2
	};
	std::vector<uint32_t> wp_; /* decoded image, 32bpp (DESK_IMAGE) */
	int wp_w_ = 0;
	int wp_h_ = 0;
	int desk_mode_ = DESK_BARS;
	uint32_t solid_color_ = 0; /* DESK_SOLID fill */
	uint32_t bar_base_ = 0;    /* DESK_BARS base shade */

	/* Fully-rendered desktop background, screen-sized, rebuilt only when the
	 * desktop changes; desktop_fill_rect() blits from it instead of re-stretching
	 * the wallpaper per pixel on every composite. */
	std::vector<uint32_t> desk_cache_;
	int dc_w_ = 0, dc_h_ = 0;
	void render_desktop_cache();

	/* Active session user; desktop settings resolve user-first then system
	 * default.  Empty = no user logged in (system layer only). */
	std::string active_user_;

	/* Rubber-band outline shown while a window is being resized by its grip. */
	bool resize_preview_ = false;
	int rp_x_ = 0, rp_y_ = 0, rp_w_ = 0, rp_h_ = 0;

	/* Live window thumbnail shown while the cursor hovers a taskbar button
	 * (Windows-11 style).  The panel geometry is computed once when the preview
	 * is set so damage/redraw tests are cheap; pv_tw_/pv_th_ are the scaled
	 * content dimensions inside the panel. */
	bool win_preview_ = false;
	uint32_t pv_id_ = 0;
	int pv_px_ = 0, pv_py_ = 0, pv_pw_ = 0, pv_ph_ = 0, pv_tw_ = 0, pv_th_ = 0;
	void draw_window_preview();
	bool preview_hit(int x, int y, int w, int h) const;

	void desktop_fill_rect(int x, int y, int w, int h);
	void draw_desktop();
	void load_wallpaper(const char *path);
	bool rect_covered(int rx, int ry, int rw, int rh);
	void reblit_rect(int rx, int ry, int rw, int rh);

	/* Modern depth: soft drop shadow under each window and anti-aliased
	 * rounded outer corners, both clipped to [clipx,clipy,clipw,cliph]. */
	uint32_t desk_pixel(int x, int y) const;
	void draw_window_shadow(const Window *w, int clipx, int clipy, int clipw, int cliph);
	void round_window_corners(const Window *w, int clipx, int clipy, int clipw, int cliph);

	void cursor_save(int x, int y);
	void cursor_erase(int x, int y);
	void cursor_draw(int x, int y);

      public:
	explicit Compositor(WindowRegistry &reg);

	int init();
	void startup();

	/* Re-read /views/desktop/* and apply the background (image/solid/bars). */
	void set_desktop_from_registry();

	/* Set the active session user (per-user desktop) and re-resolve the
	 * background.  Pass nullptr/empty to revert to the system default. */
	void set_active_user(const char *user);

	/* Re-map the framebuffer after a live resolution change and re-resolve the
	 * desktop for the new geometry (clamps the cursor into the new bounds). */
	void reopen_fb();

	/* Show/update or hide the resize rubber-band outline (full-window coords). */
	void set_resize_preview(bool active, int x, int y, int w, int h);

	/* Show/update (active=true) or hide (active=false) a floating live thumbnail
	 * of window @window_id, centred on screen-x @anchor_x just above the taskbar.
	 * Called in response to the taskbar's DISPLAY_PREVIEW hover signal. */
	void set_window_preview(bool active, uint32_t window_id, int anchor_x);

	/* Deferred rendering: accumulate damage, render once per tick. */
	void invalidate(int x, int y, int w, int h);
	void invalidate_all();
	bool flush();

	/* Immediate rendering: used by cursor_move so cursor stays snappy. */
	void composite_all();
	void partial_composite(int sx, int sy, int sw, int sh);
	void cursor_move(int dx, int dy);

	uint32_t screen_w() const
	{
		return fb_.width;
	}
	uint32_t screen_h() const
	{
		return fb_.height;
	}
	int cur_x() const
	{
		return cur_x_;
	}
	int cur_y() const
	{
		return cur_y_;
	}
};
