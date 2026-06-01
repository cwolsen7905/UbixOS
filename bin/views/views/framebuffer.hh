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

#include <stdint.h>
#include "icanvas.hh"

/* Pixel construction and font constants */
#define FB_RGB(r,g,b) \
    (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define FB_WHITE   0x00FFFFFFu
#define FB_FONT_W  8
#define FB_FONT_H  8

/* Shared window buffers are always 32bpp BGRX */
#define WIN_BPP    4

/* sys_mapfb return struct */
struct _fb_map {
	void     *base;
	uint32_t  width;
	uint32_t  height;
	uint16_t  pitch;
	uint8_t   bpp;
};

extern "C" int _sys_mapfb(struct _fb_map *info);

/* ------------------------------------------------------------------ */
/* Framebuffer — native pixel operations directly on the VESA LFB     */
/* ------------------------------------------------------------------ */

class Framebuffer : public ICanvas {
	void     put(uint8_t *row, int x, uint32_t color) const;
	uint8_t *row_ptr(int y) const;

	uint32_t *shadow_;

public:
	uint32_t  width, height, pitch, bpp;
	void     *base;

	int      open();
	int      reopen();
	int      init_shadow();
	void     flush_to_lf(int x, int y, int w, int h);

	void     pixel(int x, int y, uint32_t color);
	void     rect(int x, int y, int w, int h, uint32_t color);
	void     blit(int dx, int dy, int w, int h,
	              const uint32_t *src, int src_stride);
	void     ch(int x, int y, char c, uint32_t fg, uint32_t bg);
	void     text(int x, int y, const char *s, uint32_t fg, uint32_t bg);
	uint32_t read(int x, int y) const;
};
