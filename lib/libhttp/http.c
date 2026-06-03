/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * libhttp implementation: URL parse -> TCP/TLS connect -> GET -> parse
 * status/headers -> follow redirects -> return body.  HTTP/1.0 with
 * Connection: close (server closes the stream to signal end-of-body, so no
 * chunked-transfer decoding is needed).
 */

#include <http/http.h>
#include <bearssl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Bundled Mozilla root CAs: TAs[], TAs_NUM. */
#include "trust_anchors.c"

#define BR_DAY_OFFSET_1970 719528u /* day index of 1970-01-01 in BearSSL's calendar */

/* A connection that is either a plain TCP socket or a TLS session over one. */
struct conn
{
	int fd;
	int tls;
	br_ssl_client_context sc;
	br_x509_minimal_context xc;
	br_sslio_context ioc;
	unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
};

/* see http/http.h */
const char *http_strerror(int err)
{
	switch (err)
	{
		case 0:
			return ("ok");
		case HTTP_ERR_URL:
			return ("bad URL");
		case HTTP_ERR_RESOLVE:
			return ("DNS resolution failed");
		case HTTP_ERR_CONNECT:
			return ("connect failed");
		case HTTP_ERR_TLS:
			return ("TLS handshake/validation failed");
		case HTTP_ERR_IO:
			return ("I/O error");
		case HTTP_ERR_PROTO:
			return ("malformed HTTP response");
		case HTTP_ERR_REDIRECT:
			return ("too many redirects");
		case HTTP_ERR_NOMEM:
			return ("out of memory");
		case HTTP_ERR_STATUS:
			return ("non-2xx status");
		default:
			return ("unknown error");
	}
}

/**
 * Parse a URL into scheme/host/port/path.  Recognises http and https; the path
 * defaults to "/".  Returns 0 on success, HTTP_ERR_URL otherwise.
 */
static int parse_url(const char *url, int *https, char *host, size_t hostsz, int *port, char *path, size_t pathsz)
{
	const char *p, *slash, *colon, *hstart;

	if (strncmp(url, "https://", 8) == 0)
	{
		*https = 1;
		*port = 443;
		hstart = url + 8;
	}
	else if (strncmp(url, "http://", 7) == 0)
	{
		*https = 0;
		*port = 80;
		hstart = url + 7;
	}
	else
	{
		return (HTTP_ERR_URL);
	}

	/* host[:port] runs until the first '/' (or end of string). */
	slash = strchr(hstart, '/');
	if (slash)
	{
		if (strlen(slash) >= pathsz)
			return (HTTP_ERR_URL);
		strcpy(path, slash);
	}
	else
	{
		strcpy(path, "/");
	}

	colon = strchr(hstart, ':');
	if (colon && (!slash || colon < slash))
	{
		*port = atoi(colon + 1);
		p = colon;
	}
	else
	{
		p = slash ? slash : (hstart + strlen(hstart));
	}
	if ((size_t)(p - hstart) >= hostsz || p == hstart)
		return (HTTP_ERR_URL);
	memcpy(host, hstart, (size_t)(p - hstart));
	host[p - hstart] = '\0';
	return (0);
}

/**
 * br_sslio read callback over the socket fd.
 */
static int sock_read(void *ctx, unsigned char *buf, size_t len)
{
	int fd = *(int *)ctx;
	for (;;)
	{
		int n = (int)read(fd, buf, len);
		if (n <= 0)
		{
			if (n < 0 && errno == EINTR)
				continue;
			return (-1);
		}
		return (n);
	}
}

/**
 * br_sslio write callback over the socket fd.
 */
static int sock_write(void *ctx, const unsigned char *buf, size_t len)
{
	int fd = *(int *)ctx;
	for (;;)
	{
		int n = (int)write(fd, buf, len);
		if (n <= 0)
		{
			if (n < 0 && errno == EINTR)
				continue;
			return (-1);
		}
		return (n);
	}
}

/**
 * Open a TCP connection to host:port (resolving via DNS).
 * @return fd, or a negative http_err code.
 */
static int tcp_connect(const char *host, int port)
{
	struct hostent *he;
	struct sockaddr_in sin;
	int fd;

	he = gethostbyname(host);
	if (he == NULL || he->h_addr_list[0] == NULL)
		return (HTTP_ERR_RESOLVE);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return (HTTP_ERR_CONNECT);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((unsigned short)port);
	memcpy(&sin.sin_addr, he->h_addr_list[0], sizeof(sin.sin_addr));

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		close(fd);
		return (HTTP_ERR_CONNECT);
	}
	return (fd);
}

/**
 * Establish a connection (TCP, or TLS for https) to host:port.
 * @return 0 on success, or a negative http_err code.
 */
static int conn_open(struct conn *c, int https, const char *host, int port)
{
	memset(c, 0, sizeof(*c));
	c->tls = https;
	c->fd = tcp_connect(host, port);
	if (c->fd < 0)
		return (c->fd);

	if (!https)
		return (0);

	br_ssl_client_init_full(&c->sc, &c->xc, TAs, TAs_NUM);
	{
		unsigned char seed[32];
		if (getentropy(seed, sizeof(seed)) != 0)
		{
			close(c->fd);
			return (HTTP_ERR_TLS);
		}
		br_ssl_engine_inject_entropy(&c->sc.eng, seed, sizeof(seed));
	}
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		br_x509_minimal_set_time(
		    &c->xc, (uint32_t)(tv.tv_sec / 86400) + BR_DAY_OFFSET_1970, (uint32_t)(tv.tv_sec % 86400));
	}
	br_ssl_engine_set_buffer(&c->sc.eng, c->iobuf, sizeof(c->iobuf), 1);
	br_ssl_client_reset(&c->sc, host, 0);
	br_sslio_init(&c->ioc, &c->sc.eng, sock_read, &c->fd, sock_write, &c->fd);
	return (0);
}

/**
 * Write len bytes to the connection (through TLS if https).
 * @return 0 on success, -1 on error.
 */
static int conn_write_all(struct conn *c, const void *buf, size_t len)
{
	if (c->tls)
	{
		if (br_sslio_write_all(&c->ioc, buf, len) < 0 || br_sslio_flush(&c->ioc) < 0)
			return (-1);
		return (0);
	}
	{
		const unsigned char *p = buf;
		while (len > 0)
		{
			int n = (int)write(c->fd, p, len);
			if (n <= 0)
			{
				if (n < 0 && errno == EINTR)
					continue;
				return (-1);
			}
			p += n;
			len -= (size_t)n;
		}
	}
	return (0);
}

/**
 * Read up to len bytes from the connection.
 * @return bytes read (>0), 0 on clean EOF, -1 on error.
 */
static int conn_read(struct conn *c, void *buf, size_t len)
{
	if (c->tls)
	{
		int n = br_sslio_read(&c->ioc, buf, len);
		if (n < 0)
		{
			/* Clean close after the body counts as EOF, not an error. */
			return (br_ssl_engine_last_error(&c->sc.eng) == 0 ? 0 : -1);
		}
		return (n);
	}
	for (;;)
	{
		int n = (int)read(c->fd, buf, len);
		if (n < 0 && errno == EINTR)
			continue;
		return (n);
	}
}

/**
 * Close a connection and its socket.
 */
static void conn_close(struct conn *c)
{
	if (c->fd >= 0)
		close(c->fd);
	c->fd = -1;
}

/**
 * Case-insensitive search for a header line and copy its value (trimmed) into
 * out.  Header block is the NUL-terminated text up to the body.
 */
static void header_value(const char *headers, const char *name, char *out, size_t outsz)
{
	size_t namelen = strlen(name);
	const char *line = headers;

	out[0] = '\0';
	while (line && *line)
	{
		const char *eol = strstr(line, "\r\n");
		size_t linelen = eol ? (size_t)(eol - line) : strlen(line);
		if (linelen > namelen && strncasecmp(line, name, namelen) == 0 && line[namelen] == ':')
		{
			const char *v = line + namelen + 1;
			while (*v == ' ' || *v == '\t')
				v++;
			size_t vlen = (size_t)((line + linelen) - v);
			if (vlen >= outsz)
				vlen = outsz - 1;
			memcpy(out, v, vlen);
			out[vlen] = '\0';
			return;
		}
		line = eol ? eol + 2 : NULL;
	}
}

/**
 * Perform a single GET (no redirect handling) and capture the raw response.
 * On success fills raw and rawlen with the entire response (headers+body).
 * @return 0 on success, or a negative http_err code.
 */
static int do_get(int https, const char *host, int port, const char *path, unsigned char **raw, size_t *rawlen)
{
	struct conn c;
	char req[1100];
	unsigned char *buf = NULL;
	size_t cap = 0, len = 0;
	int rc, reqlen;

	rc = conn_open(&c, https, host, port);
	if (rc != 0)
		return (rc);

	reqlen = snprintf(req,
	                  sizeof(req),
	                  "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\nUser-Agent: UbixOS-libhttp\r\n"
	                  "Accept: */*\r\n\r\n",
	                  path,
	                  host);
	if (conn_write_all(&c, req, (size_t)reqlen) != 0)
	{
		conn_close(&c);
		return (HTTP_ERR_IO);
	}

	for (;;)
	{
		int n;
		if (len + 4096 + 1 > cap)
		{
			size_t ncap = cap ? cap * 2 : 8192;
			unsigned char *nb = realloc(buf, ncap);
			if (nb == NULL)
			{
				free(buf);
				conn_close(&c);
				return (HTTP_ERR_NOMEM);
			}
			buf = nb;
			cap = ncap;
		}
		n = conn_read(&c, buf + len, 4096);
		if (n == 0)
			break;
		if (n < 0)
		{
			free(buf);
			conn_close(&c);
			return (HTTP_ERR_IO);
		}
		len += (size_t)n;
	}
	conn_close(&c);

	if (buf == NULL || len == 0)
		return (HTTP_ERR_PROTO);
	buf[len] = '\0';
	*raw = buf;
	*rawlen = len;
	return (0);
}

/* see http/http.h */
int http_get(const char *url, struct http_meta *meta, void **body, size_t *body_len, int max_redirects)
{
	char cur[1024];
	int redirects = 0;

	if (strlen(url) >= sizeof(cur))
		return (HTTP_ERR_URL);
	strcpy(cur, url);

	for (;;)
	{
		int https, port, status, rc;
		char host[256], path[1024];
		unsigned char *raw;
		size_t rawlen;
		const char *sp, *body_start;
		char loc[1024], ctype[HTTP_CTYPE_MAX], clen[32];

		rc = parse_url(cur, &https, host, sizeof(host), &port, path, sizeof(path));
		if (rc != 0)
			return (rc);

		rc = do_get(https, host, port, path, &raw, &rawlen);
		if (rc != 0)
			return (rc);

		/* Status line: "HTTP/1.x NNN ..." */
		if (strncmp((char *)raw, "HTTP/1.", 7) != 0 || (sp = strchr((char *)raw, ' ')) == NULL)
		{
			free(raw);
			return (HTTP_ERR_PROTO);
		}
		status = atoi(sp + 1);

		body_start = strstr((char *)raw, "\r\n\r\n");
		if (body_start == NULL)
		{
			free(raw);
			return (HTTP_ERR_PROTO);
		}
		body_start += 4;

		/* 3xx with a Location -> follow. */
		header_value((char *)raw, "Location", loc, sizeof(loc));
		if (status >= 300 && status < 400 && loc[0] != '\0' && redirects < max_redirects)
		{
			free(raw);
			if (strncmp(loc, "http", 4) != 0) /* only absolute redirects for now */
				return (HTTP_ERR_REDIRECT);
			if (strlen(loc) >= sizeof(cur))
				return (HTTP_ERR_URL);
			strcpy(cur, loc);
			redirects++;
			continue;
		}
		if (status >= 300 && status < 400 && loc[0] != '\0')
			return (HTTP_ERR_REDIRECT);

		/* Final response: fill meta + body. */
		header_value((char *)raw, "Content-Type", ctype, sizeof(ctype));
		header_value((char *)raw, "Content-Length", clen, sizeof(clen));
		if (meta)
		{
			meta->status = status;
			meta->content_length = clen[0] ? atol(clen) : -1;
			strncpy(meta->content_type, ctype, sizeof(meta->content_type) - 1);
			meta->content_type[sizeof(meta->content_type) - 1] = '\0';
			strncpy(meta->final_url, cur, sizeof(meta->final_url) - 1);
			meta->final_url[sizeof(meta->final_url) - 1] = '\0';
		}

		{
			size_t blen = rawlen - (size_t)((unsigned char *)body_start - raw);
			if (body)
			{
				unsigned char *b = malloc(blen + 1);
				if (b == NULL)
				{
					free(raw);
					return (HTTP_ERR_NOMEM);
				}
				memcpy(b, body_start, blen);
				b[blen] = '\0';
				*body = b;
			}
			if (body_len)
				*body_len = blen;
		}
		free(raw);
		return ((status >= 200 && status < 300) ? 0 : HTTP_ERR_STATUS);
	}
}
