/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * libhttp — a small HTTP/1.0 + HTTPS client for UbixOS, built on lwIP sockets
 * and BearSSL.  Handles URL parsing, http/https, and redirect following.
 */

#ifndef _HTTP_HTTP_H
#define _HTTP_HTTP_H

#include <stddef.h>

#define HTTP_MAX_REDIRECTS 5
#define HTTP_CTYPE_MAX 128

/* Result metadata from a fetch. */
struct http_meta
{
	int status;                        /* final HTTP status code (e.g. 200) */
	long content_length;               /* from Content-Length, or -1 if absent */
	char content_type[HTTP_CTYPE_MAX]; /* from Content-Type, or "" */
	char final_url[1024];              /* URL after any redirects */
};

/**
 * Fetch a URL (http:// or https://) into a freshly malloc'd buffer.
 *
 * Follows up to @p max_redirects 3xx redirects.  HTTPS validates the server
 * certificate (chain, hostname and expiry) against the bundled root CAs, so a
 * roughly-correct system clock is required.
 *
 * @param meta      out: status, content-type/length, final URL (may be NULL).
 * @param body      out: malloc'd response body; caller frees.  NUL-terminated
 *                  for convenience (the terminator is not counted in body_len).
 * @param body_len  out: number of body bytes (may be NULL).
 * @param max_redirects  redirect cap; HTTP_MAX_REDIRECTS is a sane default.
 * @return 0 on a final 2xx response, or a negative http_err code on failure.
 */
int http_get(const char *url, struct http_meta *meta, void **body, size_t *body_len, int max_redirects);

/* Negative error codes returned by http_get. */
#define HTTP_ERR_URL (-1)      /* malformed or unsupported URL */
#define HTTP_ERR_RESOLVE (-2)  /* DNS resolution failed */
#define HTTP_ERR_CONNECT (-3)  /* TCP connect failed */
#define HTTP_ERR_TLS (-4)      /* TLS handshake / cert validation failed */
#define HTTP_ERR_IO (-5)       /* read/write error mid-transfer */
#define HTTP_ERR_PROTO (-6)    /* malformed HTTP response */
#define HTTP_ERR_REDIRECT (-7) /* too many redirects */
#define HTTP_ERR_NOMEM (-8)    /* allocation failed */
#define HTTP_ERR_STATUS (-9)   /* final response was not 2xx (see meta.status) */

/**
 * Human-readable string for an http_get return code.
 */
const char *http_strerror(int err);

#endif /* _HTTP_HTTP_H */
