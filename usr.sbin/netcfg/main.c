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

/*
 * netcfg - apply the machine's network configuration from the ubistry registry
 * to the kernel lwIP stack.  Reads /net/mode (dhcp|static) and, for static, the
 * /net/ip, /net/netmask, /net/gateway and /net/dns octet strings, then pushes
 * them through the net_configure() syscall.  Run once at boot (after ubistry,
 * /etc/init.d/16-netcfg) and again by Settings whenever the config changes.
 */

#include <stdio.h>
#include <string.h>

#include <ubistry/ubistry.h>
#include <api/netcfg.h>
#include <api/ubix.h>

/**
 * Parse an "a.b.c.d" dotted-quad into four octets.
 * @return 0 on success, -1 if the string is malformed.
 */
static int parse_ip(const char *s, uint8_t out[4])
{
	int a, b, c, d;

	if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
		return (-1);
	if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255)
		return (-1);
	out[0] = (uint8_t)a;
	out[1] = (uint8_t)b;
	out[2] = (uint8_t)c;
	out[3] = (uint8_t)d;
	return (0);
}

int main(void)
{
	struct net_config cfg;
	char buf[64];

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = NET_DHCP;

	if (ubistry_get_str("/net/mode", buf, sizeof(buf)) == 0 && strcmp(buf, "static") == 0)
	{
		cfg.mode = NET_STATIC;
		if (ubistry_get_str("/net/ip", buf, sizeof(buf)) != 0 || parse_ip(buf, cfg.ip) != 0)
		{
			ulog(ULOG_ERR, "netcfg: static mode but /net/ip is missing/invalid");
			return (1);
		}
		if (ubistry_get_str("/net/netmask", buf, sizeof(buf)) != 0 || parse_ip(buf, cfg.netmask) != 0)
		{
			ulog(ULOG_ERR, "netcfg: static mode but /net/netmask is missing/invalid");
			return (1);
		}
		if (ubistry_get_str("/net/gateway", buf, sizeof(buf)) == 0)
			parse_ip(buf, cfg.gateway);
		if (ubistry_get_str("/net/dns", buf, sizeof(buf)) == 0)
			parse_ip(buf, cfg.dns);
	}

	if (ubix_net_configure(&cfg) != 0)
	{
		ulog(ULOG_ERR, "netcfg: net_configure syscall failed (no NIC?)");
		return (1);
	}

	if (cfg.mode == NET_STATIC)
		ulogf(ULOG_INFO, "netcfg: applied static %d.%d.%d.%d", cfg.ip[0], cfg.ip[1], cfg.ip[2], cfg.ip[3]);
	else
		ulog(ULOG_INFO, "netcfg: applied DHCP");
	return (0);
}
