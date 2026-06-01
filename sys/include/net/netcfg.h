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
 * netcfg.h - the wire layout for the UbixOS-native net_configure() syscall
 * (int $0x81, slot 59).  A userland configurator (bin/netcfg) reads the desired
 * network settings from the ubistry registry and pushes them to the kernel lwIP
 * stack with this struct.  The userland copy lives at include/api/netcfg.h and
 * MUST stay byte-for-byte identical with this one.
 */

#ifndef _NET_NETCFG_H
#define _NET_NETCFG_H

#include <sys/types.h>

#define NET_DHCP 0   /* obtain an address via DHCP */
#define NET_STATIC 1 /* use the static ip/netmask/gateway/dns below */

/* IPv4 addresses are stored as 4 octets (a.b.c.d), byte-order independent. */
struct net_config
{
	u_int32_t mode; /* NET_DHCP or NET_STATIC */
	u_int8_t ip[4];
	u_int8_t netmask[4];
	u_int8_t gateway[4];
	u_int8_t dns[4]; /* all-zero = leave DNS unchanged */
};

#endif /* _NET_NETCFG_H */
