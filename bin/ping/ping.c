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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

#define PING_DATA_LEN	56
#define PING_PKT_LEN	(8 + PING_DATA_LEN)   /* ICMP header + data */
#define RECV_BUF_LEN	1024
#define PING_COUNT	4

static uint16_t
icmp_cksum(const void *buf, int len)
{
	const uint16_t *p = buf;
	uint32_t sum = 0;

	while (len > 1) {
		sum += *p++;
		len -= 2;
	}
	if (len == 1)
		sum += *(const uint8_t *)p;

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (uint16_t)~sum;
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in dst;
	uint8_t pkt_buf[PING_PKT_LEN];
	char recvbuf[RECV_BUF_LEN];
	int sock, i;
	uint16_t pid;

	if (argc < 2) {
		fprintf(stderr, "usage: ping <address>\n");
		return 1;
	}

	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock < 0) {
		fprintf(stderr, "ping: socket: failed\n");
		return 1;
	}

	memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	if (inet_aton(argv[1], &dst.sin_addr) == 0) {
		fprintf(stderr, "ping: bad address: %s\n", argv[1]);
		return 1;
	}

	pid = (uint16_t)(getpid() & 0xffff);

	printf("PING %s: %d data bytes\n", argv[1], PING_DATA_LEN);

	for (i = 0; i < PING_COUNT; i++) {
		struct sockaddr_in from;
		struct ip *iph;
		struct icmp *reply;
		socklen_t fromlen;
		int recvlen, iphlen;

		struct icmp *pkt = (struct icmp *)pkt_buf;
		memset(pkt_buf, 0, PING_PKT_LEN);
		pkt->icmp_type  = ICMP_ECHO;
		pkt->icmp_code  = 0;
		pkt->icmp_id    = pid;
		pkt->icmp_seq   = (uint16_t)i;
		memset(pkt_buf + 8, 0x42, PING_DATA_LEN);
		pkt->icmp_cksum = icmp_cksum(pkt_buf, PING_PKT_LEN);

		if (sendto(sock, pkt_buf, PING_PKT_LEN, 0,
		    (struct sockaddr *)&dst, sizeof(dst)) < 0) {
			fprintf(stderr, "ping: sendto failed\n");
			continue;
		}

		fromlen = sizeof(from);
		recvlen = recvfrom(sock, recvbuf, sizeof(recvbuf), 0,
		    (struct sockaddr *)&from, &fromlen);
		if (recvlen < 0) {
			printf("Request timeout for icmp_seq %d\n", i);
			continue;
		}

		iph    = (struct ip *)recvbuf;
		iphlen = (iph->ip_hl & 0x0f) << 2;
		reply  = (struct icmp *)(recvbuf + iphlen);

		if (reply->icmp_type != ICMP_ECHOREPLY ||
		    reply->icmp_id   != pid) {
			/* Not our reply — try to recv again once */
			recvlen = recvfrom(sock, recvbuf, sizeof(recvbuf), 0,
			    (struct sockaddr *)&from, &fromlen);
			if (recvlen < 0) {
				printf("Request timeout for icmp_seq %d\n", i);
				continue;
			}
			iph    = (struct ip *)recvbuf;
			iphlen = (iph->ip_hl & 0x0f) << 2;
			reply  = (struct icmp *)(recvbuf + iphlen);
		}

		if (reply->icmp_type == ICMP_ECHOREPLY) {
			printf("%d bytes from %s: icmp_seq=%d ttl=%d\n",
			    recvlen - iphlen,
			    inet_ntoa(from.sin_addr),
			    reply->icmp_seq,
			    iph->ip_ttl);
		}

		sleep(1);
	}

	return 0;
}
