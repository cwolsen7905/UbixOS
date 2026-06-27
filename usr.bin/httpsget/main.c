/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * httpsget — fetch an http:// or https:// URL via libhttp and print the
 * response.  A thin client over libhttp (which owns the TLS, trust anchors,
 * redirects and HTTP parsing).
 *
 * Usage: httpsget <url|host> [path]
 *   "example.com" is treated as "https://example.com/".
 */

#include <http/http.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @return 0 on a 2xx response, 1 on any error.
 */
int main(int argc, char **argv)
{
	struct http_meta meta;
	char url[1024];
	void *body = NULL;
	size_t len = 0;
	int rc;

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <url|host> [path]\n", argv[0]);
		return (1);
	}

	/* Accept a bare host (default https) or a full URL; optional path arg. */
	if (strncmp(argv[1], "http://", 7) == 0 || strncmp(argv[1], "https://", 8) == 0)
		snprintf(url, sizeof(url), "%s%s", argv[1], (argc >= 3) ? argv[2] : "");
	else
		snprintf(url, sizeof(url), "https://%s%s", argv[1], (argc >= 3) ? argv[2] : "/");

	memset(&meta, 0, sizeof(meta));
	rc = http_get(url, &meta, &body, &len, HTTP_MAX_REDIRECTS);
	if (rc != 0)
	{
		fprintf(stderr, "httpsget: %s", http_strerror(rc));
		if (rc == HTTP_ERR_STATUS)
			fprintf(stderr, " (HTTP %d)", meta.status);
		fprintf(stderr, "\n");
		free(body);
		return (1);
	}

	printf("httpsget: HTTP %d  %ld bytes  type=%s\n",
	       meta.status,
	       (long)len,
	       meta.content_type[0] ? meta.content_type : "(none)");
	printf("final URL: %s\n----\n", meta.final_url);
	write(1, body, len);
	free(body);
	return (0);
}
