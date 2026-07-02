/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * lwIP netif bridge for the aarch64 virtio-net driver + the aarch64 network
 * bring-up (net_init).
 *
 * The i386 net_init (sys/net/net/init.c) is hard-wired to the e1000/ne2k PCI
 * drivers, so aarch64 has its own here: it brings lwIP up over the polling
 * virtio-net driver (sys/arch/aarch64/dev/virtio_net.c).  TX flattens an lwIP
 * pbuf chain and hands it to NET_SEND(); an RX kernel thread polls the
 * receive used-ring via NET_POLL_RX() and feeds each frame to lwIP.
 */

#include "bringup.h"
#include <sys/types.h>
#include <string.h>
#include <ubixos/sched.h> /* sched_yield */
#include <sys/klog.h>
#include <sys/descrip.h> /* g_socket_select (path B) */

#include "net/opt.h"
#include "net/def.h"
#include "net/mem.h"
#include "net/pbuf.h"
#include "net/stats.h"
#include "net/etharp.h"
#include "net/netif.h"
#include "net/tcpip.h"
#include "net/dhcp.h"
#include "net/sys.h"

#define IFNAME0 'v'
#define IFNAME1 'n'

/* NIC backend: the SMSC LAN9514 USB Ethernet on the Raspberry Pi, virtio-net on
 * QEMU.  Both expose the same send/poll_rx/mac/ready interface. */
#ifdef BOARD_RPI3
#define NET_MAC smsc_mac
#define NET_READY smsc_ready
#define NET_SEND smsc_send
#define NET_POLL_RX smsc_poll_rx
#else
#define NET_MAC virtio_net_mac
#define NET_READY virtio_net_ready
#define NET_SEND virtio_net_send
#define NET_POLL_RX virtio_net_poll_rx
#endif

static struct netif g_vnet_netif;

/**
 * netif init callback (run once via netif_add): copy the MAC, set MTU + flags.
 */
static void low_level_init(struct netif *netif)
{
	netif->hwaddr_len = ETHARP_HWADDR_LEN;
	memcpy(netif->hwaddr, NET_MAC, 6);
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
}

/**
 * lwIP TX: flatten the pbuf chain into one linear frame and transmit it.
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	u_int8_t tx_scratch[1518]; /* max Ethernet frame without VLAN */
	u_int16_t total = 0;
	struct pbuf *q;

	(void)netif;
	for (q = p; q != NULL; q = q->next)
	{
		if (total + q->len > sizeof(tx_scratch))
		{
			LINK_STATS_INC(link.drop);
			return ERR_BUF;
		}
		memcpy(tx_scratch + total, q->payload, q->len);
		total += q->len;
	}

	if (NET_SEND(tx_scratch, total) != 0)
	{
		LINK_STATS_INC(link.drop);
		return ERR_IF;
	}
	LINK_STATS_INC(link.xmit);
	return ERR_OK;
}

/**
 * RX delivery callback (invoked by NET_POLL_RX for each received frame):
 * wrap the frame in a pbuf and push it into lwIP via netif->input (tcpip_input,
 * which is thread-safe — it posts to the tcpip mailbox).
 */
static void vnet_deliver(const u_int8_t *frame, u_int32_t len)
{
	struct pbuf *p, *q;
	u_int32_t copied = 0;

	if (len == 0 || len > 1600)
		return;
	p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
	if (p == NULL)
	{
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		return;
	}
	for (q = p; q != NULL && copied < len; q = q->next)
	{
		u_int32_t n = (q->len < (len - copied)) ? q->len : (len - copied);
		memcpy(q->payload, frame + copied, n);
		copied += n;
	}
	LINK_STATS_INC(link.recv);
	/* The RX thread can deliver a frame before netif_add() has installed the
	 * input handler (tcpip_input) — an init-order race that, under SMP, faults
	 * by calling through a NULL g_vnet_netif.input.  Drop the frame until the
	 * netif is fully registered. */
	if (g_vnet_netif.input == NULL)
	{
		LINK_STATS_INC(link.drop);
		pbuf_free(p);
		return;
	}
	if (g_vnet_netif.input(p, &g_vnet_netif) != ERR_OK)
		pbuf_free(p);
}

/* Never-true sleep predicate: vnet_rx_thread has no RX interrupt to wake it, so
 * it just sleeps out the timeout each idle pass (see vnet_rx_thread). */
static int vnet_rx_never(void *arg)
{
	(void)arg;
	return (0);
}

static char g_vnet_rx_chan; /* sleep channel token for the idle RX wait */

/**
 * RX poll thread: drain the virtio receive ring and feed lwIP.  While frames are
 * arriving it loops at full speed; once a sweep finds nothing it sleeps ~1 tick
 * (leaving the run queue) instead of busy-yielding, so an idle link costs ~0%
 * CPU rather than pegging one.  Latency on the first packet after idle is one
 * timer tick (10 ms) — fine for a desktop; a real RX interrupt would remove even
 * that (the driver has none yet).
 */
static void vnet_rx_thread(void *arg)
{
	(void)arg;
	for (;;)
	{
		if (NET_POLL_RX(vnet_deliver) == 0)
			sched_wait_event_timeout(&g_vnet_rx_chan, vnet_rx_never, NULL, 1);
	}
}

/**
 * lwIP netif init function passed to netif_add().
 */
static err_t vnet_netif_init(struct netif *netif)
{
	netif->state = NULL;
	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;
	low_level_init(netif);
	return ERR_OK;
}

/**
 * tcpip-thread callback (NO_SYS=0): add the virtio-net interface and start DHCP.
 * All netif/DHCP setup must happen here to satisfy lwIP's core-lock assertions.
 */
static void net_status_cb(struct netif *netif)
{
	if (!ip4_addr_isany(netif_ip4_addr(netif)))
		kprintf("net: DHCP bound IP=%d.%d.%d.%d gw=%d.%d.%d.%d\n",
		        ip4_addr1(netif_ip4_addr(netif)),
		        ip4_addr2(netif_ip4_addr(netif)),
		        ip4_addr3(netif_ip4_addr(netif)),
		        ip4_addr4(netif_ip4_addr(netif)),
		        ip4_addr1(netif_ip4_gw(netif)),
		        ip4_addr2(netif_ip4_gw(netif)),
		        ip4_addr3(netif_ip4_gw(netif)),
		        ip4_addr4(netif_ip4_gw(netif)));
}

static void net_tcpip_init_done(void *arg)
{
	ip_addr_t ipaddr, netmask, gw;
	(void)arg;

	IP4_ADDR(&ipaddr, 0, 0, 0, 0);
	IP4_ADDR(&netmask, 0, 0, 0, 0);
	IP4_ADDR(&gw, 0, 0, 0, 0);

	netif_add(&g_vnet_netif, &ipaddr, &netmask, &gw, NULL, vnet_netif_init, tcpip_input);
	netif_set_status_callback(&g_vnet_netif, net_status_cb);
	netif_set_link_up(&g_vnet_netif);
	netif_set_up(&g_vnet_netif);
	netif_set_default(&g_vnet_netif);

	dhcp_start(&g_vnet_netif);
	klog(KLOG_INFO, "net: DHCP requested on vn0");
}

/**
 * Bring up the aarch64 network stack: start lwIP's tcpip thread (which runs the
 * netif/DHCP setup callback), spawn the virtio-net RX poll thread, and install
 * lwIP's select() for the generic sys_select/sys_poll path.
 *
 * @return 0 always (no NIC -> a no-op + warning).
 */
int aarch64_net_init(void)
{
	{
		extern int lwip_select(int, struct fd_set *, struct fd_set *, struct fd_set *, struct timeval *);
		g_socket_select = lwip_select;
	}

	if (!NET_READY)
	{
		klog(KLOG_WARNING, "net: no virtio-net device, skipping network init");
		return (0);
	}

	tcpip_init(net_tcpip_init_done, NULL);
	sys_thread_new("vnetRx", vnet_rx_thread, NULL, 0x1000, 0);
	klog(KLOG_INFO, "net: lwIP started over virtio-net");
	return (0);
}
