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
 * netcfg.c - the kernel side of the net_configure() native syscall (slot 59).
 * The userland configurator (bin/netcfg) hands us a struct net_config; we copy
 * it onto the kernel heap and hand it to the tcpip thread (via tcpip_callback)
 * where all lwIP core calls must run.  The callback applies a static address
 * (dhcp_stop + netif_set_addr + dns_setserver) or restarts DHCP, then frees the
 * copy.
 */

#include <sys/types.h>
#include <string.h>

#include <net/sys.h>
#include <net/tcpip.h>
#include <net/ip_addr.h>
#include <net/dhcp.h>
#include <net/dns.h>
#include <net/netcfg.h>

#include <netif/e1000netif.h>

#include <lib/kmalloc.h>
#include <sys/klog.h>
#include <sys/sysproto.h>
#include <pci/e1000.h>

/**
 * Apply a network configuration.  Runs in tcpip_thread context (enqueued by
 * sys_net_configure), so it may freely call lwIP core functions.  Frees the
 * heap-allocated config when done.
 */
static void net_apply_cb(void *ctx)
{
	struct net_config *c = (struct net_config *)ctx;

	if (c->mode == NET_STATIC)
	{
		ip4_addr_t ip, mask, gw;

		IP4_ADDR(&ip, c->ip[0], c->ip[1], c->ip[2], c->ip[3]);
		IP4_ADDR(&mask, c->netmask[0], c->netmask[1], c->netmask[2], c->netmask[3]);
		IP4_ADDR(&gw, c->gateway[0], c->gateway[1], c->gateway[2], c->gateway[3]);

		dhcp_stop(&e1000_netif);
		netif_set_addr(&e1000_netif, &ip, &mask, &gw);

		if (c->dns[0] || c->dns[1] || c->dns[2] || c->dns[3])
		{
			ip_addr_t dns;
			IP4_ADDR(&dns, c->dns[0], c->dns[1], c->dns[2], c->dns[3]);
			dns_setserver(0, &dns);
		}

		klog(KLOG_NOTICE,
		     "net: static IP %d.%d.%d.%d/%d.%d.%d.%d gw %d.%d.%d.%d",
		     c->ip[0],
		     c->ip[1],
		     c->ip[2],
		     c->ip[3],
		     c->netmask[0],
		     c->netmask[1],
		     c->netmask[2],
		     c->netmask[3],
		     c->gateway[0],
		     c->gateway[1],
		     c->gateway[2],
		     c->gateway[3]);
	}
	else
	{
		klog(KLOG_INFO, "net: switching to DHCP");
		dhcp_start(&e1000_netif);
	}

	kfree(c);
}

/**
 * net_configure(const struct net_config *cfg) - native syscall slot 59.
 *
 * Copies the user's config to the kernel heap and schedules it onto the tcpip
 * thread.  Best-effort: returns 0 if the request was queued, -1 otherwise.
 */
int sys_net_configure(struct thread *td, struct sys_net_configure_args *args)
{
	struct net_config *c;

	if (!e1000_ready || args->cfg == NULL)
	{
		td->td_retval[0] = -1;
		return (0);
	}

	c = (struct net_config *)kmalloc(sizeof(*c));
	if (c == NULL)
	{
		td->td_retval[0] = -1;
		return (0);
	}
	memcpy(c, args->cfg, sizeof(*c));

	if (tcpip_callback(net_apply_cb, c) != ERR_OK)
	{
		kfree(c);
		td->td_retval[0] = -1;
		return (0);
	}

	td->td_retval[0] = 0;
	return (0);
}
