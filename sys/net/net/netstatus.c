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
 * netstatus.c - net_status() native syscall (slot 70): a read-only snapshot of
 * the default lwIP interface for the taskbar's network item.  Shared by i386
 * (e1000 / ne2k) and aarch64 (virtio-net).
 */

#include <sys/types.h>
#include <string.h>
#include <net/netif.h>
#include <net/ip_addr.h>
#include <net/netcfg.h>
#include <sys/sysproto.h>

/**
 * net_status(struct net_status *st) - fill st from netif_default.
 *
 * A plain field read without the tcpip lock: the values are word-sized and
 * the caller only displays them, so a momentarily stale snapshot is harmless.
 *
 * @return 0 (td_retval[0] = 0, or -1 when there is no interface / bad pointer).
 */
int sys_net_status(struct thread *td, struct sys_net_status_args *args)
{
	struct net_status st;
	struct netif *nif = netif_default;

	if (args->st == NULL || nif == NULL)
	{
		td->td_retval[0] = -1;
		return (0);
	}
	memset(&st, 0, sizeof(st));
	st.up = netif_is_up(nif) ? 1 : 0;
	st.link = netif_is_link_up(nif) ? 1 : 0;
	u_int32_t ip = ip4_addr_get_u32(netif_ip4_addr(nif));
	u_int32_t gw = ip4_addr_get_u32(netif_ip4_gw(nif));
	memcpy(st.ip, &ip, 4); /* network byte order: a.b.c.d in memory order */
	memcpy(st.gateway, &gw, 4);
	st.name[0] = nif->name[0];
	st.name[1] = nif->name[1];
	st.name[2] = (char)('0' + nif->num % 10);
	memcpy(args->st, &st, sizeof(st));
	td->td_retval[0] = 0;
	return (0);
}
