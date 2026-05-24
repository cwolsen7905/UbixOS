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
 * lwIP netif bridge for the Intel e1000 (82540EM) driver.
 * Replaces ethernetif.c — no shared globals, reads MAC from hardware.
 */

#include <pci/e1000.h>
#include <string.h>

#include "net/opt.h"
#include "net/def.h"
#include "net/mem.h"
#include "net/pbuf.h"
#include "net/stats.h"
#include "net/snmp.h"
#include "net/ethip6.h"
#include "net/etharp.h"

#define IFNAME0 'e'
#define IFNAME1 '0'

struct netif e1000_netif;

/* Declared in e1000.c — returns pointer to the last-received packet */
extern const u_int8_t *e1000_get_rx_packet(u_int16_t *out_len);

/* -----------------------------------------------------------------------
 * low_level_init — called once by netif_add via e1000netif_init
 * --------------------------------------------------------------------- */

static void low_level_init(struct netif *netif) {
	netif->hwaddr_len = ETHARP_HWADDR_LEN;
	netif->hwaddr[0]  = e1000_mac[0];
	netif->hwaddr[1]  = e1000_mac[1];
	netif->hwaddr[2]  = e1000_mac[2];
	netif->hwaddr[3]  = e1000_mac[3];
	netif->hwaddr[4]  = e1000_mac[4];
	netif->hwaddr[5]  = e1000_mac[5];

	netif->mtu   = 1500;
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
}

/* -----------------------------------------------------------------------
 * low_level_output — transmit one lwIP pbuf chain
 * --------------------------------------------------------------------- */

static err_t low_level_output(struct netif *netif, struct pbuf *p) {
	/* Flatten the pbuf chain into a single linear buffer.
	 * The e1000 supports scatter-gather but this keeps the driver simple. */
	u_int8_t tx_scratch[1518]; /* max Ethernet frame without VLAN */
	u_int16_t       total = 0;
	struct pbuf   *q;

	(void)netif;

	for (q = p; q != NULL; q = q->next) {
		if (total + q->len > sizeof(tx_scratch)) {
			LINK_STATS_INC(link.drop);
			return ERR_BUF;
		}
		memcpy(tx_scratch + total, q->payload, q->len);
		total += q->len;
	}

	e1000_send_packet(tx_scratch, total);

	MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
	if (((u8_t *)p->payload)[0] & 1)
		MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
	else
		MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
	LINK_STATS_INC(link.xmit);
	return ERR_OK;
}

/* -----------------------------------------------------------------------
 * low_level_input — wrap the received packet in a pbuf chain
 * --------------------------------------------------------------------- */

static struct pbuf *low_level_input(struct netif *netif) {
	u_int16_t       len;
	const u_int8_t *buf = e1000_get_rx_packet(&len);
	struct pbuf   *p, *q;
	u_int16_t       copied = 0;

	(void)netif;

	if (!buf || !len)
		return NULL;

	p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
	if (!p) {
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		MIB2_STATS_NETIF_INC(netif, ifindiscards);
		return NULL;
	}

	for (q = p; q != NULL && copied < len; q = q->next) {
		u_int16_t n = (q->len < (len - copied)) ? q->len : (len - copied);
		memcpy(q->payload, buf + copied, n);
		copied += n;
	}

	MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);
	if (((u8_t *)p->payload)[0] & 1)
		MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
	else
		MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
	LINK_STATS_INC(link.recv);
	return p;
}

/* -----------------------------------------------------------------------
 * e1000netif_input — called from e1000_thread when a packet arrives
 * --------------------------------------------------------------------- */

void e1000netif_input(struct netif *netif) {
	struct pbuf *p = low_level_input(netif);
	if (!p)
		return;
	if (netif->input(p, netif) != ERR_OK) {
		LWIP_DEBUGF(NETIF_DEBUG, ("e1000netif_input: input error\n"));
		pbuf_free(p);
	}
}

/* -----------------------------------------------------------------------
 * e1000netif_init — passed to netif_add()
 * --------------------------------------------------------------------- */

err_t e1000netif_init(struct netif *netif) {
	LWIP_ASSERT("netif != NULL", netif != NULL);

	netif->state    = NULL;
	netif->name[0]  = IFNAME0;
	netif->name[1]  = IFNAME1;
	netif->output   = etharp_output;
#if LWIP_IPV6
	netif->output_ip6 = ethip6_output;
#endif
	netif->linkoutput = low_level_output;

	low_level_init(netif);
	return ERR_OK;
}
