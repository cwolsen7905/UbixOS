/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * udptest — diagnose DNS failure by exercising the UDP path directly.
 * (1) raw UDP: socket(SOCK_DGRAM) -> sendto a DNS query to 8.8.8.8:53 ->
 *     poll -> recvfrom, reporting each step + errno.
 * (2) resolver: gethostbyname("google.com") + getaddrinfo, reporting result.
 * Splits "kernel UDP broken" from "musl resolver/config broken".
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Minimal DNS A-record query for "google.com", ID 0x1234, RD set. */
static const unsigned char g_dnsq[] = {0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x06, 'g',  'o',  'o',  'g',  'l',  'e',  0x03,
                                       'c',  'o',  'm',  0x00, 0x00, 0x01, 0x00, 0x01};

/**
 * Probe the UDP datagram path to a DNS server and the libc resolver.
 * @return 0 always (diagnostic output is the point).
 */
int main(void)
{
	struct sockaddr_in sin;
	struct pollfd pfd;
	unsigned char rbuf[512];
	struct hostent *he;
	int fd, n;

	printf("== udptest ==\n");

	/* (1) raw UDP to 8.8.8.8:53 */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	printf("socket(AF_INET,SOCK_DGRAM)=%d errno=%d\n", fd, fd < 0 ? errno : 0);
	if (fd >= 0)
	{
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(53);
		sin.sin_addr.s_addr = inet_addr("8.8.8.8");

		n = (int)sendto(fd, g_dnsq, sizeof(g_dnsq), 0, (struct sockaddr *)&sin, sizeof(sin));
		printf("sendto(8.8.8.8:53,%d)=%d errno=%d\n", (int)sizeof(g_dnsq), n, n < 0 ? errno : 0);

		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		n = poll(&pfd, 1, 3000);
		printf("poll(3s)=%d revents=0x%x errno=%d\n", n, pfd.revents, n < 0 ? errno : 0);
		if (n > 0)
		{
			n = (int)recvfrom(fd, rbuf, sizeof(rbuf), 0, NULL, NULL);
			printf("recvfrom=%d errno=%d", n, n < 0 ? errno : 0);
			if (n >= 4)
				printf("  (DNS id=0x%02x%02x ancount=%d)", rbuf[0], rbuf[1], (rbuf[6] << 8) | rbuf[7]);
			printf("\n");
		}
		close(fd);
	}

	/* (2) libc resolver */
	he = gethostbyname("google.com");
	printf("gethostbyname(google.com)=%s", he ? "OK" : "NULL");
	if (he && he->h_addr_list[0])
	{
		unsigned char *a = (unsigned char *)he->h_addr_list[0];
		printf(" %d.%d.%d.%d", a[0], a[1], a[2], a[3]);
	}
	else
	{
		printf(" h_errno=%d", h_errno);
	}
	printf("\n");
	return (0);
}
