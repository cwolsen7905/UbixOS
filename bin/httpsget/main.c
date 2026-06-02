/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * httpsget — minimal HTTPS GET client: TLS via BearSSL over an lwIP TCP socket.
 * Proves the full path end-to-end (DNS -> TCP -> TLS handshake -> certificate
 * validation against vendored root CAs -> HTTP exchange).
 *
 * Entropy for the TLS RNG is injected from getentropy() (the kernel CSPRNG),
 * since BearSSL's br_prng_seeder_system() is not wired on UbixOS.  Certificate
 * EXPIRY is not yet checked (X.509 time left at 0); the chain and hostname ARE
 * validated against the trust anchors.  Adding gettimeofday()-based time is a
 * follow-up.
 *
 * Usage: httpsget <host> [path]
 */

#include <bearssl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Vendored Mozilla CA bundle as BearSSL trust anchors: TAs[], TAs_NUM. */
#include "trust_anchors.c"

/* TLS engine needs a bidirectional record buffer; keep it off the stack. */
static unsigned char g_iobuf[BR_SSL_BUFSIZE_BIDI];

/**
 * br_sslio low-level read callback: pull bytes from the TCP socket.
 * @param ctx  pointer to the socket fd.
 * @return bytes read (>0), or -1 on EOF/error.
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
 * br_sslio low-level write callback: push bytes to the TCP socket.
 * @return bytes written (>0), or -1 on error.
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
 * Open a TCP connection to host:port, resolving the hostname via gethostbyname.
 * @return connected socket fd, or -1 on failure.
 */
static int tcp_connect(const char *host, int port)
{
	struct hostent *he;
	struct sockaddr_in sin;
	int fd;

	he = gethostbyname(host);
	if (he == NULL || he->h_addr_list[0] == NULL)
	{
		fprintf(stderr, "httpsget: cannot resolve %s\n", host);
		return (-1);
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		perror("socket");
		return (-1);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((unsigned short)port);
	memcpy(&sin.sin_addr, he->h_addr_list[0], sizeof(sin.sin_addr));

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("connect");
		close(fd);
		return (-1);
	}
	return (fd);
}

/**
 * Fetch https://<host>/<path> and print the response, then report the TLS
 * engine status.
 *
 * @return 0 if the handshake completed and the cert validated, 1 otherwise.
 */
int main(int argc, char **argv)
{
	br_ssl_client_context sc;
	br_x509_minimal_context xc;
	br_sslio_context ioc;
	const char *host, *path;
	char req[512];
	int fd, err, rlen, got_body = 0;

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <host> [path]\n", argv[0]);
		return (1);
	}
	host = argv[1];
	path = (argc >= 3) ? argv[2] : "/";

	fd = tcp_connect(host, 443);
	if (fd < 0)
		return (1);
	printf("httpsget: connected to %s:443\n", host);

	/* Set up the TLS client with the vendored trust anchors. */
	br_ssl_client_init_full(&sc, &xc, TAs, TAs_NUM);

	/* Seed the TLS RNG from the kernel CSPRNG (no system seeder on UbixOS). */
	{
		unsigned char seed[32];
		if (getentropy(seed, sizeof(seed)) != 0)
		{
			fprintf(stderr, "httpsget: getentropy failed\n");
			close(fd);
			return (1);
		}
		br_ssl_engine_inject_entropy(&sc.eng, seed, sizeof(seed));
	}

	br_ssl_engine_set_buffer(&sc.eng, g_iobuf, sizeof(g_iobuf), 1);
	br_ssl_client_reset(&sc, host, 0); /* host drives SNI + cert name check */

	br_sslio_init(&ioc, &sc.eng, sock_read, &fd, sock_write, &fd);

	rlen = snprintf(req,
	                sizeof(req),
	                "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\nUser-Agent: UbixOS-httpsget\r\n\r\n",
	                path,
	                host);

	if (br_sslio_write_all(&ioc, req, (size_t)rlen) < 0 || br_sslio_flush(&ioc) < 0)
	{
		fprintf(stderr, "httpsget: TLS write failed (err %d)\n", br_ssl_engine_last_error(&sc.eng));
		close(fd);
		return (1);
	}

	/* Drain the response to stdout. */
	for (;;)
	{
		unsigned char buf[512];
		int rc = br_sslio_read(&ioc, buf, sizeof(buf));
		if (rc < 0)
			break;
		got_body += rc;
		write(1, buf, (size_t)rc);
	}

	err = br_ssl_engine_last_error(&sc.eng);
	close(fd);

	/* BR_ERR_OK (0) means the session closed cleanly after the exchange. */
	printf("\nhttpsget: %d body bytes, TLS last_error=%d (%s)\n",
	       got_body,
	       err,
	       err == 0 ? "OK" : "see bearssl_ssl.h BR_ERR_*");
	return (err == 0 ? 0 : 1);
}
