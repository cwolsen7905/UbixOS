/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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
 * JPEG content handler, decoding via stb_image (UbixOS has no libjpeg).
 *
 * Mirrors the PNG handler.  JPEGs have no alpha, so bitmaps are created
 * BITMAP_OPAQUE; stb still emits RR GG BB AA (alpha 0xff) at req_comp==4.
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
#include "image/jpeg.h"

typedef struct nsjpeg_content {
	struct content base;
} nsjpeg_content;

static nserror
nsjpeg_create(const content_handler *handler,
	      lwc_string *imime_type,
	      const struct http_parameter *params,
	      struct llcache_handle *llcache,
	      const char *fallback_charset,
	      bool quirks,
	      struct content **c)
{
	nsjpeg_content *jpeg_c;
	nserror error;

	jpeg_c = calloc(1, sizeof(nsjpeg_content));
	if (jpeg_c == NULL)
		return NSERROR_NOMEM;

	error = content__init(&jpeg_c->base, handler, imime_type, params,
			      llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(jpeg_c);
		return error;
	}

	*c = (struct content *)jpeg_c;
	return NSERROR_OK;
}

/**
 * Decode the cached JPEG source into a NetSurf bitmap via stb_image.
 *
 * @return the created bitmap, or NULL on failure (caller-owned).
 */
static struct bitmap *jpeg_cache_convert(struct content *c)
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
		NSLOG(netsurf, INFO, "stb JPEG decode failed: %s", stbi_failure_reason());
		return NULL;
	}

	bitmap = guit->bitmap->create(width, height, BITMAP_NEW | BITMAP_OPAQUE);
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

static bool nsjpeg_convert(struct content *c)
{
	const uint8_t *data;
	size_t size;
	int width = 0, height = 0, comp = 0;
	char *title;

	data = content__get_source_data(c, &size);
	if (data == NULL || size == 0 ||
	    stbi_info_from_memory(data, (int)size, &width, &height, &comp) == 0) {
		union content_msg_data msg_data;
		msg_data.error = "Failed to decode JPEG";
		content_broadcast(c, CONTENT_MSG_ERROR, &msg_data);
		return false;
	}

	c->width = width;
	c->height = height;
	c->size += (size_t)width * height * 4;

	image_cache_add(c, NULL, jpeg_cache_convert);

	title = messages_get_buff("JPEGTitle",
				  nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
				  width, height);
	if (title != NULL) {
		content__set_title(c, title);
		free(title);
	}

	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");

	return true;
}

static nserror nsjpeg_clone(const struct content *old, struct content **newc)
{
	struct content *jpeg_c;
	nserror error;

	jpeg_c = calloc(1, sizeof(nsjpeg_content));
	if (jpeg_c == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, jpeg_c);
	if (error != NSERROR_OK) {
		content_destroy(jpeg_c);
		return error;
	}

	if ((old->status == CONTENT_STATUS_READY) ||
	    (old->status == CONTENT_STATUS_DONE)) {
		if (nsjpeg_convert(jpeg_c) == false) {
			content_destroy(jpeg_c);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = jpeg_c;
	return NSERROR_OK;
}

static const content_handler nsjpeg_content_handler = {
	.create = nsjpeg_create,
	.data_complete = nsjpeg_convert,
	.destroy = image_cache_destroy,
	.redraw = image_cache_redraw,
	.clone = nsjpeg_clone,
	.get_internal = image_cache_get_internal,
	.type = image_cache_content_type,
	.no_share = false,
};

static const char *nsjpeg_types[] = {
	"image/jpeg",
	"image/jpg",
	"image/pjpeg"
};

CONTENT_FACTORY_REGISTER_TYPES(nsjpeg, nsjpeg_types, nsjpeg_content_handler);
