/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
 * Copyright 2026 The UbixOS Project (stb_image port)
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * \file
 * PNG content handler, decoding via stb_image (UbixOS has no libpng).
 *
 * NetSurf bitmaps are 32bpp RR GG BB AA — exactly stb's req_comp==4 output, so
 * the decoded rows copy straight in.  Dimensions are read cheaply at
 * data_complete with stbi_info; the full decode is deferred to the image cache.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image.h"

#include "netsurf/inttypes.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "netsurf/bitmap.h"
#include "content/llcache.h"
#include "content/content_protected.h"
#include "desktop/gui_internal.h"

#include "image/image_cache.h"
#include "image/png.h"

typedef struct nspng_content {
	struct content base;
} nspng_content;

static nserror
nspng_create(const content_handler *handler,
	     lwc_string *imime_type,
	     const struct http_parameter *params,
	     struct llcache_handle *llcache,
	     const char *fallback_charset,
	     bool quirks,
	     struct content **c)
{
	nspng_content *png_c;
	nserror error;

	png_c = calloc(1, sizeof(nspng_content));
	if (png_c == NULL)
		return NSERROR_NOMEM;

	error = content__init(&png_c->base, handler, imime_type, params,
			      llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(png_c);
		return error;
	}

	*c = (struct content *)png_c;
	return NSERROR_OK;
}

static bool nspng_process_data(struct content *c, const char *data,
			       unsigned int size)
{
	/* Source data is accumulated by the llcache; decode happens lazily in
	 * png_cache_convert().  Nothing to do per-chunk. */
	(void)c;
	(void)data;
	(void)size;
	return true;
}

/**
 * Decode the cached PNG source into a NetSurf bitmap via stb_image.
 *
 * @return the created bitmap, or NULL on failure (caller-owned).
 */
static struct bitmap *png_cache_convert(struct content *c)
{
	const uint8_t *data;
	size_t size;
	int width = 0, height = 0, comp = 0, y;
	unsigned char *decoded;
	struct bitmap *bitmap;
	unsigned char *buffer;
	size_t rowstride;

	data = content__get_source_data(c, &size);
	if (data == NULL || size == 0)
		return NULL;

	decoded = stbi_load_from_memory(data, (int)size, &width, &height, &comp, 4);
	if (decoded == NULL) {
		NSLOG(netsurf, INFO, "stb PNG decode failed: %s", stbi_failure_reason());
		return NULL;
	}

	bitmap = guit->bitmap->create(width, height, BITMAP_NEW);
	if (bitmap == NULL) {
		stbi_image_free(decoded);
		return NULL;
	}

	buffer = guit->bitmap->get_buffer(bitmap);
	if (buffer == NULL) {
		guit->bitmap->destroy(bitmap);
		stbi_image_free(decoded);
		return NULL;
	}

	rowstride = guit->bitmap->get_rowstride(bitmap);
	for (y = 0; y < height; y++) {
		memcpy(buffer + (size_t)y * rowstride,
		       decoded + (size_t)y * (size_t)width * 4,
		       (size_t)width * 4);
	}

	stbi_image_free(decoded);
	guit->bitmap->modified(bitmap);

	return bitmap;
}

static bool nspng_convert(struct content *c)
{
	const uint8_t *data;
	size_t size;
	int width = 0, height = 0, comp = 0;
	char *title;

	data = content__get_source_data(c, &size);
	if (data == NULL || size == 0 ||
	    stbi_info_from_memory(data, (int)size, &width, &height, &comp) == 0) {
		content_broadcast_errorcode(c, NSERROR_PNG_ERROR);
		return false;
	}

	c->width = width;
	c->height = height;
	c->size += (size_t)width * height * 4;

	title = messages_get_buff("PNGTitle",
				  nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
				  width, height);
	if (title != NULL) {
		content__set_title(c, title);
		free(title);
	}

	image_cache_add(c, NULL, png_cache_convert);

	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");

	return true;
}

static nserror nspng_clone(const struct content *old_c, struct content **new_c)
{
	nspng_content *clone_png_c;
	nserror error;

	clone_png_c = calloc(1, sizeof(nspng_content));
	if (clone_png_c == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old_c, &clone_png_c->base);
	if (error != NSERROR_OK) {
		content_destroy(&clone_png_c->base);
		return error;
	}

	if ((old_c->status == CONTENT_STATUS_READY) ||
	    (old_c->status == CONTENT_STATUS_DONE)) {
		if (nspng_convert(&clone_png_c->base) == false) {
			content_destroy(&clone_png_c->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*new_c = (struct content *)clone_png_c;
	return NSERROR_OK;
}

static const content_handler nspng_content_handler = {
	.create = nspng_create,
	.process_data = nspng_process_data,
	.data_complete = nspng_convert,
	.clone = nspng_clone,
	.destroy = image_cache_destroy,
	.redraw = image_cache_redraw,
	.get_internal = image_cache_get_internal,
	.type = image_cache_content_type,
	.no_share = false,
};

static const char *nspng_types[] = {
	"image/png",
	"image/x-png"
};

CONTENT_FACTORY_REGISTER_TYPES(nspng, nspng_types, nspng_content_handler);
