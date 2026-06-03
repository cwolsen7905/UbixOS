/*
 * Copyright 2026 The UbixOS Project
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
 * http:// and https:// scheme handler over UbixOS libhttp (BearSSL).
 *
 * Modelled on the data: fetcher (content/fetchers/data.c): each fetch is a
 * context on a ring, processed synchronously in the poll callback.  Unlike
 * curl, http_get() blocks until the whole response is in memory, so a single
 * poll iteration delivers FETCH_HEADER(s) + FETCH_DATA + FETCH_FINISHED.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <http/http.h>
#include <libwapcaplet/libwapcaplet.h>

#include "netsurf/inttypes.h"
#include "utils/nsurl.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/ring.h"

#include "content/fetch.h"
#include "content/fetchers.h"
#include "content/fetchers/libhttp.h"

struct fetch_libhttp_context {
	struct fetch *parent_fetch;
	nsurl *url;
	bool only_2xx;

	bool aborted;
	bool locked;

	struct fetch_libhttp_context *r_next, *r_prev;
};

static struct fetch_libhttp_context *ring = NULL;

/**
 * Register interest in a scheme (http or https).  Nothing to set up.
 */
static bool fetch_libhttp_initialise(lwc_string *scheme)
{
	NSLOG(netsurf, INFO, "fetch_libhttp_initialise called for %s",
	      lwc_string_data(scheme));
	return true;
}

static void fetch_libhttp_finalise(lwc_string *scheme)
{
	NSLOG(netsurf, INFO, "fetch_libhttp_finalise called for %s",
	      lwc_string_data(scheme));
}

static bool fetch_libhttp_can_fetch(const nsurl *url)
{
	return true;
}

static void *fetch_libhttp_setup(struct fetch *parent_fetch, nsurl *url,
		bool only_2xx, bool downgrade_tls, const char *post_urlenc,
		const struct fetch_multipart_data *post_multipart,
		const char **headers)
{
	struct fetch_libhttp_context *ctx = calloc(1, sizeof(*ctx));

	if (ctx == NULL)
		return NULL;

	ctx->parent_fetch = parent_fetch;
	ctx->url = nsurl_ref(url);
	ctx->only_2xx = only_2xx;

	RING_INSERT(ring, ctx);

	return ctx;
}

static bool fetch_libhttp_start(void *ctx)
{
	return true;
}

static void fetch_libhttp_free(void *ctx)
{
	struct fetch_libhttp_context *c = ctx;

	nsurl_unref(c->url);
	RING_REMOVE(ring, c);
	free(ctx);
}

static void fetch_libhttp_abort(void *ctx)
{
	struct fetch_libhttp_context *c = ctx;

	/* The poll loop performs the cleanup; just flag it here. */
	c->aborted = true;
}

static void fetch_libhttp_send_callback(const fetch_msg *msg,
		struct fetch_libhttp_context *c)
{
	c->locked = true;
	fetch_send_callback(msg, c->parent_fetch);
	c->locked = false;
}

static void fetch_libhttp_send_header(struct fetch_libhttp_context *c,
		const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void fetch_libhttp_send_header(struct fetch_libhttp_context *c,
		const char *fmt, ...)
{
	fetch_msg msg;
	char header[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(header, sizeof(header), fmt, ap);
	va_end(ap);

	msg.type = FETCH_HEADER;
	msg.data.header_or_data.buf = (const uint8_t *)header;
	msg.data.header_or_data.len = strlen(header);
	fetch_libhttp_send_callback(&msg, c);
}

/**
 * Run the blocking fetch and deliver the response.
 *
 * A non-2xx response that still carries a body (HTTP_ERR_STATUS) is delivered
 * as renderable content (error pages), matching curl-fetcher behaviour; any
 * other negative return is reported as FETCH_ERROR.
 */
static void fetch_libhttp_process(struct fetch_libhttp_context *c)
{
	fetch_msg msg;
	struct http_meta meta;
	void *body = NULL;
	size_t body_len = 0;
	int rc;

	memset(&meta, 0, sizeof(meta));
	rc = http_get(nsurl_access(c->url), &meta, &body, &body_len,
		      HTTP_MAX_REDIRECTS);

	if (rc != 0 && rc != HTTP_ERR_STATUS) {
		NSLOG(netsurf, INFO, "http_get(%.140s) failed: %s",
		      nsurl_access(c->url), http_strerror(rc));
		msg.type = FETCH_ERROR;
		msg.data.error = http_strerror(rc);
		fetch_libhttp_send_callback(&msg, c);
		free(body);
		return;
	}

	if (c->only_2xx && (meta.status < 200 || meta.status >= 300)) {
		msg.type = FETCH_ERROR;
		msg.data.error = "Server returned a non-2xx status";
		fetch_libhttp_send_callback(&msg, c);
		free(body);
		return;
	}

	fetch_set_http_code(c->parent_fetch, meta.status ? meta.status : 200);

	if (c->aborted == false && meta.content_type[0] != '\0')
		fetch_libhttp_send_header(c, "Content-Type: %s",
					  meta.content_type);

	if (c->aborted == false)
		fetch_libhttp_send_header(c, "Content-Length: %" PRIsizet,
					  body_len);

	if (c->aborted == false) {
		msg.type = FETCH_DATA;
		msg.data.header_or_data.buf = (const uint8_t *)body;
		msg.data.header_or_data.len = body_len;
		fetch_libhttp_send_callback(&msg, c);
	}

	if (c->aborted == false) {
		msg.type = FETCH_FINISHED;
		fetch_libhttp_send_callback(&msg, c);
	}

	free(body);
}

static void fetch_libhttp_poll(lwc_string *scheme)
{
	struct fetch_libhttp_context *c, *next;

	if (ring == NULL)
		return;

	c = ring;
	do {
		/* Locked fetches are mid-callback; skip for re-entrancy. */
		if (c->locked == true) {
			next = c->r_next;
			continue;
		}

		if (c->aborted == false)
			fetch_libhttp_process(c);

		next = c->r_next;

		fetch_remove_from_queues(c->parent_fetch);
		fetch_free(c->parent_fetch);
	} while ((c = next) != ring && ring != NULL);
}

nserror fetch_libhttp_register(void)
{
	const struct fetcher_operation_table fetcher_ops = {
		.initialise = fetch_libhttp_initialise,
		.acceptable = fetch_libhttp_can_fetch,
		.setup = fetch_libhttp_setup,
		.start = fetch_libhttp_start,
		.abort = fetch_libhttp_abort,
		.free = fetch_libhttp_free,
		.poll = fetch_libhttp_poll,
		.finalise = fetch_libhttp_finalise
	};
	lwc_string *http = lwc_string_ref(corestring_lwc_http);
	lwc_string *https = lwc_string_ref(corestring_lwc_https);
	nserror res;

	res = fetcher_add(http, &fetcher_ops);
	if (res != NSERROR_OK)
		return res;

	return fetcher_add(https, &fetcher_ops);
}
