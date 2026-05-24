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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFSIZE		4096
#define MAX_HOST	256
#define MAX_PATH	1024

static int
usage(void)
{
	fprintf(stderr, "usage: wget [-O output] http://host[:port]/path\n");
	return (1);
}

/*
 * Parse http://host[:port]/path into components.
 * Returns 0 on success, -1 on error.
 */
static int
parse_url(const char *url, char *host, int hostsz, int *port, char *path, int pathsz)
{
	const char *p;
	const char *slash;
	const char *colon;
	int hostlen;

	if (strncmp(url, "http://", 7) != 0)
		return (-1);
	p = url + 7;

	slash = strchr(p, '/');
	if (slash == NULL) {
		/* no path component — treat as root */
		slash = p + strlen(p);
	}

	/* isolate host[:port] */
	colon = memchr(p, ':', (size_t)(slash - p));
	if (colon != NULL) {
		hostlen = (int)(colon - p);
		*port = atoi(colon + 1);
	} else {
		hostlen = (int)(slash - p);
		*port = 80;
	}

	if (hostlen <= 0 || hostlen >= hostsz)
		return (-1);
	memcpy(host, p, (size_t)hostlen);
	host[hostlen] = '\0';

	if (*slash == '\0') {
		strncpy(path, "/", pathsz - 1);
		path[pathsz - 1] = '\0';
	} else {
		strncpy(path, slash, pathsz - 1);
		path[pathsz - 1] = '\0';
	}

	if (*port <= 0 || *port > 65535)
		return (-1);
	return (0);
}

/*
 * Derive a local filename from the URL path.
 * Uses the last path component, or "index.html" for bare /.
 */
static const char *
output_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	const char *name = (slash != NULL && *(slash + 1) != '\0') ? slash + 1 : NULL;
	return (name != NULL ? name : "index.html");
}

/*
 * Read exactly one line (up to \n) from fd into buf.
 * Returns number of bytes placed in buf (not including \0), or -1 on error / 0 on EOF.
 */
static int
readline(int fd, char *buf, int bufsz)
{
	int i = 0;
	char c;
	int r;

	while (i < bufsz - 1) {
		r = read(fd, &c, 1);
		if (r <= 0)
			return (r);
		if (c == '\n') {
			buf[i] = '\0';
			/* strip trailing \r */
			if (i > 0 && buf[i - 1] == '\r')
				buf[i - 1] = '\0';
			return (i);
		}
		buf[i++] = c;
	}
	buf[i] = '\0';
	return (i);
}

int
main(int argc, char *argv[])
{
	char host[MAX_HOST];
	char path[MAX_PATH];
	char outfile[MAX_PATH];
	char request[MAX_HOST + MAX_PATH + 64];
	char buf[BUFSIZE];
	char line[BUFSIZE];
	int port, fd, nr, nw;
	int content_length = -1;
	int received = 0;
	int header_done = 0;
	int status_code = 0;
	FILE *out = NULL;
	struct hostent *he;
	struct sockaddr_in sin;
	const char *outarg = NULL;

	/* parse options */
	int i = 1;
	while (i < argc && argv[i][0] == '-') {
		if (strcmp(argv[i], "-O") == 0 && i + 1 < argc) {
			outarg = argv[++i];
		} else {
			return (usage());
		}
		i++;
	}

	if (i >= argc)
		return (usage());

	if (parse_url(argv[i], host, sizeof(host), &port, path, sizeof(path)) < 0) {
		fprintf(stderr, "wget: invalid URL: %s\n", argv[i]);
		return (1);
	}

	/* determine output destination */
	if (outarg != NULL) {
		strncpy(outfile, outarg, sizeof(outfile) - 1);
		outfile[sizeof(outfile) - 1] = '\0';
	} else {
		strncpy(outfile, output_name(path), sizeof(outfile) - 1);
		outfile[sizeof(outfile) - 1] = '\0';
	}

	fprintf(stderr, "--  %s\n", argv[i]);
	fprintf(stderr, "Resolving %s... ", host);
	fflush(stderr);

	he = gethostbyname(host);
	if (he == NULL) {
		fprintf(stderr, "failed\n");
		fprintf(stderr, "wget: cannot resolve %s\n", host);
		return (1);
	}

	char ipstr[16];
	struct in_addr *addr = (struct in_addr *)he->h_addr_list[0];
	snprintf(ipstr, sizeof(ipstr), "%u.%u.%u.%u",
	    (unsigned char)he->h_addr[0], (unsigned char)he->h_addr[1],
	    (unsigned char)he->h_addr[2], (unsigned char)he->h_addr[3]);
	fprintf(stderr, "%s\n", ipstr);

	fprintf(stderr, "Connecting to %s (%s):%d... ", host, ipstr, port);
	fflush(stderr);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("wget: socket");
		return (1);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port   = htons((unsigned short)port);
	memcpy(&sin.sin_addr, addr, sizeof(*addr));

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("wget: connect");
		close(fd);
		return (1);
	}
	fprintf(stderr, "connected.\n");

	/* send HTTP/1.0 request */
	snprintf(request, sizeof(request),
	    "GET %s HTTP/1.0\r\n"
	    "Host: %s\r\n"
	    "User-Agent: wget/ubixos\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    path, host);
	if (write(fd, request, strlen(request)) < 0) {
		perror("wget: write");
		close(fd);
		return (1);
	}

	/* read status line */
	if (readline(fd, line, sizeof(line)) <= 0) {
		fprintf(stderr, "wget: no response\n");
		close(fd);
		return (1);
	}

	/* HTTP/1.x NNN reason */
	{
		char *sp = strchr(line, ' ');
		if (sp != NULL)
			status_code = atoi(sp + 1);
	}

	fprintf(stderr, "HTTP response: %d\n", status_code);

	if (status_code != 200) {
		fprintf(stderr, "wget: server returned %d\n", status_code);
		close(fd);
		return (1);
	}

	/* read headers */
	while (readline(fd, line, sizeof(line)) > 0) {
		if (strncasecmp(line, "Content-Length:", 15) == 0)
			content_length = atoi(line + 15);
	}
	header_done = 1;
	(void)header_done;

	/* open output */
	if (strcmp(outfile, "-") == 0) {
		out = stdout;
	} else {
		out = fopen(outfile, "w");
		if (out == NULL) {
			perror("wget: fopen");
			close(fd);
			return (1);
		}
		fprintf(stderr, "Saving to: '%s'\n", outfile);
	}

	/* read body */
	while (1) {
		nr = read(fd, buf, sizeof(buf));
		if (nr <= 0)
			break;
		nw = fwrite(buf, 1, (size_t)nr, out);
		if (nw < nr) {
			perror("wget: fwrite");
			break;
		}
		received += nr;
		if (content_length > 0 && received >= content_length)
			break;
	}

	if (out != stdout)
		fclose(out);
	close(fd);

	fprintf(stderr, "\n%d bytes received.\n", received);
	return (0);
}
