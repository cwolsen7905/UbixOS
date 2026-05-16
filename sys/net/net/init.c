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

#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/endtask.h>
#include <ubixos/sem.h>

#include <net/sys.h>
#include <net/mem.h>
#include <net/memp.h>
#include <net/tcpip.h>
#include <net/ip_addr.h>
#include <net/dhcp.h>

#include <netif/ethernet.h>
#include <netif/e1000netif.h>

#include <ubixos/exec.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <pci/e1000.h>

static void e1000_thread_trampoline(void *arg) {
	(void)arg;
	void e1000_thread(void);
	e1000_thread();
}

/*
 * Called from inside the tcpip thread once it is running.
 * All netif and DHCP setup must happen here in threaded mode (NO_SYS=0)
 * to satisfy lwIP's core-lock assertions.
 */
static void net_status_cb(struct netif *netif) {
	if (!ip4_addr_isany(netif_ip4_addr(netif))) {
		kprintf("net: DHCP bound — IP=%d.%d.%d.%d mask=%d.%d.%d.%d gw=%d.%d.%d.%d\n",
		    ip4_addr1(netif_ip4_addr(netif)),
		    ip4_addr2(netif_ip4_addr(netif)),
		    ip4_addr3(netif_ip4_addr(netif)),
		    ip4_addr4(netif_ip4_addr(netif)),
		    ip4_addr1(netif_ip4_netmask(netif)),
		    ip4_addr2(netif_ip4_netmask(netif)),
		    ip4_addr3(netif_ip4_netmask(netif)),
		    ip4_addr4(netif_ip4_netmask(netif)),
		    ip4_addr1(netif_ip4_gw(netif)),
		    ip4_addr2(netif_ip4_gw(netif)),
		    ip4_addr3(netif_ip4_gw(netif)),
		    ip4_addr4(netif_ip4_gw(netif)));
	}
}

static void net_tcpip_init_done(void *arg) {
	ip_addr_t ipaddr, netmask, gw;
	(void)arg;

	IP4_ADDR(&ipaddr,  0, 0, 0, 0);
	IP4_ADDR(&netmask, 0, 0, 0, 0);
	IP4_ADDR(&gw,      0, 0, 0, 0);

	netif_add(&e1000_netif, &ipaddr, &netmask, &gw, NULL,
	    e1000netif_init, tcpip_input);
	netif_set_status_callback(&e1000_netif, net_status_cb);
	netif_set_link_up(&e1000_netif);
	netif_set_up(&e1000_netif);
	netif_set_default(&e1000_netif);

	dhcp_start(&e1000_netif);
	kprintf("net: DHCP requested\n");
}

int net_init(void) {
	if (!e1000_ready) {
		kprintf("net: no NIC available, skipping network init\n");
		return 0;
	}

	tcpip_init(net_tcpip_init_done, NULL);

	sys_thread_new("e1000Thread", e1000_thread_trampoline, NULL, 0x1000, 0);

	kprintf("net: lwIP started\n");
	return 0;
}
