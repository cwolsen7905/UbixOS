/*
 * Copyright 2026 The UbixOS Project
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/**
 * \file
 * Scalable antialiased framebuffer font backend over stb_truetype.
 * Selected by NETSURF_FB_FONTLIB=stbtt (defines FB_USE_STBTT).
 */

#ifndef NETSURF_FB_FONT_STBTT_H
#define NETSURF_FB_FONT_STBTT_H

#include <stdint.h>
#include "netsurf/plot_style.h"

/** A rasterised glyph: 8bpp alpha coverage plus placement relative to the pen.
 *  Owned by the glyph cache; valid until evicted, so use immediately. */
struct fb_glyph {
	int advance;       /**< horizontal advance, pixels */
	int left;          /**< x offset from pen to bitmap left */
	int top;           /**< y offset from baseline to bitmap top (<=0 above) */
	int width;         /**< bitmap width, pixels */
	int height;        /**< bitmap height, pixels */
	const uint8_t *bitmap; /**< width*height 8bpp coverage, or NULL (e.g. space) */
};

/** Codepoints below U+0020 are control characters and are not rendered. */
#define codepoint_displayable(u) (((u) >= 0x20) && ((u) != 0x7f))

/**
 * Look up (rasterising and caching on demand) the glyph for a codepoint in the
 * face/size/style selected by @p fstyle.
 *
 * @return cache-owned glyph (use before the next lookup), or NULL on failure.
 */
const struct fb_glyph *fb_get_glyph(const plot_font_style_t *fstyle, uint32_t ucs4);

#endif /* NETSURF_FB_FONT_STBTT_H */
