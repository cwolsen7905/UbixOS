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
 *
 * Replaces the fixed 8x16 internal bitmap font with real proportional, AA
 * TrueType rendering.  Faces are loaded from disk once; rasterised glyphs are
 * kept in a small direct-mapped cache (rasterising on every repaint would be
 * far too slow).
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "utils/log.h"
#include "utils/utf8.h"
#include "netsurf/layout.h"
#include "netsurf/utf8.h"
#include "netsurf/plot_style.h"

#include "framebuffer/font.h"

/* Font files, installed by mkimage alongside the other NetSurf resources. */
#define FONT_DIR "/usr/local/share/netsurf/"

enum fb_face_id {
	FB_FACE_SANS = 0, /* sans-serif and serif both map here for v1 */
	FB_FACE_MONO,
	FB_FACE_COUNT
};

struct fb_face {
	unsigned char *data; /* mmap-free: whole TTF read into memory */
	stbtt_fontinfo info;
	bool loaded;
};

static struct fb_face g_faces[FB_FACE_COUNT];

/* 96 DPI is NetSurf's nominal screen density; pt -> px is size * dpi/72. */
#define FB_DPI 96

/* Direct-mapped rasterised-glyph cache. */
#define GLYPH_CACHE_SIZE 1024

struct glyph_cache_entry {
	bool valid;
	uint32_t ucs4;
	int px;        /* pixel height the glyph was rasterised at */
	int face;
	struct fb_glyph glyph;
	uint8_t *owned_bitmap; /* freed on eviction */
};

static struct glyph_cache_entry g_cache[GLYPH_CACHE_SIZE];

/**
 * Read a whole font file into a face slot.
 *
 * @return true if the face loaded and parsed.
 */
static bool load_face(enum fb_face_id id, const char *path)
{
	FILE *f;
	long len;
	unsigned char *buf;

	f = fopen(path, "rb");
	if (f == NULL) {
		NSLOG(netsurf, INFO, "stbtt: cannot open font %s", path);
		return false;
	}
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len <= 0) {
		fclose(f);
		return false;
	}
	buf = malloc((size_t)len);
	if (buf == NULL) {
		fclose(f);
		return false;
	}
	if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
		free(buf);
		fclose(f);
		return false;
	}
	fclose(f);

	if (stbtt_InitFont(&g_faces[id].info, buf, stbtt_GetFontOffsetForIndex(buf, 0)) == 0) {
		NSLOG(netsurf, INFO, "stbtt: failed to parse font %s", path);
		free(buf);
		return false;
	}
	g_faces[id].data = buf;
	g_faces[id].loaded = true;
	NSLOG(netsurf, INFO, "stbtt: loaded %s", path);
	return true;
}

/**
 * Pick the face for a style.  v1 has no bold/italic faces: sans and serif
 * share DejaVuSans, monospace uses DejaVuSansMono.
 */
static enum fb_face_id face_for_style(const plot_font_style_t *fstyle)
{
	if (fstyle->family == PLOT_FONT_FAMILY_MONOSPACE)
		return FB_FACE_MONO;
	return FB_FACE_SANS;
}

/** Convert a NetSurf plot font size (pt * PLOT_STYLE_SCALE) to pixels. */
static int px_for_style(const plot_font_style_t *fstyle)
{
	int px = (int)(((double)fstyle->size / PLOT_STYLE_SCALE) * FB_DPI / 72.0 + 0.5);
	if (px < 6)
		px = 6; /* floor so tiny text stays legible */
	return px;
}

/**
 * Fetch a glyph, rasterising and caching on demand.
 */
const struct fb_glyph *fb_get_glyph(const plot_font_style_t *fstyle, uint32_t ucs4)
{
	enum fb_face_id face = face_for_style(fstyle);
	int px = px_for_style(fstyle);
	struct fb_face *fc = &g_faces[face];
	struct glyph_cache_entry *e;
	unsigned int h;
	float scale;
	int adv, lsb, w = 0, ht = 0, xoff = 0, yoff = 0;
	unsigned char *bm;

	if (!fc->loaded)
		return NULL;

	h = (ucs4 * 2654435761u + (unsigned)px * 97u + (unsigned)face) % GLYPH_CACHE_SIZE;
	e = &g_cache[h];
	if (e->valid && e->ucs4 == ucs4 && e->px == px && e->face == (int)face)
		return &e->glyph;

	scale = stbtt_ScaleForPixelHeight(&fc->info, (float)px);
	stbtt_GetCodepointHMetrics(&fc->info, (int)ucs4, &adv, &lsb);
	bm = stbtt_GetCodepointBitmap(&fc->info, scale, scale, (int)ucs4, &w, &ht, &xoff, &yoff);

	/* Replace whatever occupied this slot. */
	if (e->valid && e->owned_bitmap != NULL)
		free(e->owned_bitmap);

	e->valid = true;
	e->ucs4 = ucs4;
	e->px = px;
	e->face = (int)face;
	e->owned_bitmap = bm; /* may be NULL for blank glyphs (e.g. space) */
	e->glyph.advance = (int)(adv * scale + 0.5f);
	e->glyph.left = xoff;
	e->glyph.top = yoff;
	e->glyph.width = w;
	e->glyph.height = ht;
	e->glyph.bitmap = bm;

	return &e->glyph;
}

/* ---- gui_layout_table: measurement -------------------------------------- */

static nserror
fb_font_width_internal(const plot_font_style_t *fstyle,
		       const char *string, size_t length, int *width)
{
	size_t nxtchr = 0;
	int total = 0;

	while (nxtchr < length) {
		uint32_t ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
		const struct fb_glyph *g = fb_get_glyph(fstyle, ucs4);
		if (g != NULL)
			total += g->advance;
		nxtchr = utf8_next(string, length, nxtchr);
	}

	*width = total;
	return NSERROR_OK;
}

nserror
fb_font_width(const plot_font_style_t *fstyle,
	      const char *string, size_t length, int *width)
{
	return fb_font_width_internal(fstyle, string, length, width);
}

nserror
fb_font_position(const plot_font_style_t *fstyle,
		 const char *string, size_t length, int x,
		 size_t *char_offset, int *actual_x)
{
	size_t nxtchr = 0;
	int x_pos = 0;

	while (nxtchr < length) {
		uint32_t ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
		const struct fb_glyph *g = fb_get_glyph(fstyle, ucs4);
		int adv = (g != NULL) ? g->advance : 0;

		if (x_pos + adv / 2 >= x)
			break;
		x_pos += adv;
		nxtchr = utf8_next(string, length, nxtchr);
	}

	*actual_x = x_pos;
	*char_offset = nxtchr;
	return NSERROR_OK;
}

static nserror
fb_font_split(const plot_font_style_t *fstyle,
	      const char *string, size_t length, int x,
	      size_t *char_offset, int *actual_x)
{
	size_t nxtchr = 0;
	int x_pos = 0;
	int last_space_x = 0;
	size_t last_space_idx = 0;

	while (nxtchr < length) {
		uint32_t ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
		const struct fb_glyph *g = fb_get_glyph(fstyle, ucs4);

		if (string[nxtchr] == ' ') {
			last_space_x = x_pos;
			last_space_idx = nxtchr;
		}

		if (g != NULL)
			x_pos += g->advance;

		if (x_pos > x && last_space_idx != 0) {
			*actual_x = last_space_x;
			*char_offset = last_space_idx;
			return NSERROR_OK;
		}

		nxtchr = utf8_next(string, length, nxtchr);
	}

	*actual_x = x_pos;
	*char_offset = length;
	return NSERROR_OK;
}

static struct gui_layout_table layout_table = {
	.width = fb_font_width,
	.position = fb_font_position,
	.split = fb_font_split,
};

struct gui_layout_table *framebuffer_layout_table = &layout_table;

/* ---- gui_utf8_table ----------------------------------------------------- */

static nserror utf8_to_local(const char *string, size_t len, char **result)
{
	*result = malloc(len + 1);
	if (*result == NULL)
		return NSERROR_NOMEM;
	memcpy(*result, string, len);
	(*result)[len] = '\0';
	return NSERROR_OK;
}

static nserror utf8_from_local(const char *string, size_t len, char **result)
{
	*result = malloc(len + 1);
	if (*result == NULL)
		return NSERROR_NOMEM;
	memcpy(*result, string, len);
	(*result)[len] = '\0';
	return NSERROR_OK;
}

static struct gui_utf8_table utf8_table = {
	.utf8_to_local = utf8_to_local,
	.local_to_utf8 = utf8_from_local,
};

struct gui_utf8_table *framebuffer_utf8_table = &utf8_table;

/* ---- init / finalise ---------------------------------------------------- */

bool fb_font_init(void)
{
	bool sans = load_face(FB_FACE_SANS, FONT_DIR "DejaVuSans.ttf");
	/* Monospace is optional; fall back to sans if absent. */
	if (!load_face(FB_FACE_MONO, FONT_DIR "DejaVuSansMono.ttf"))
		g_faces[FB_FACE_MONO] = g_faces[FB_FACE_SANS];
	return sans;
}

bool fb_font_finalise(void)
{
	int i;
	for (i = 0; i < GLYPH_CACHE_SIZE; i++) {
		if (g_cache[i].owned_bitmap != NULL)
			free(g_cache[i].owned_bitmap);
		memset(&g_cache[i], 0, sizeof(g_cache[i]));
	}
	if (g_faces[FB_FACE_SANS].data != NULL)
		free(g_faces[FB_FACE_SANS].data);
	if (g_faces[FB_FACE_MONO].data != NULL &&
	    g_faces[FB_FACE_MONO].data != g_faces[FB_FACE_SANS].data)
		free(g_faces[FB_FACE_MONO].data);
	return true;
}
