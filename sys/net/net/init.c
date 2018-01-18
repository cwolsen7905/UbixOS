/*****************************************************************************************
 Copyright (c) 2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id: init.c 54 2016-01-11 01:29:55Z reddawg $

 *****************************************************************************************/

#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/endtask.h>
#include <ubixos/sem.h>

#include <net/sys.h>
#include <net/mem.h>
#include <net/memp.h>
#include <net/tcpip.h>
#include <net/ip_addr.h>

#include <netif/ethernet.h>
#include <netif/ethernetif.h>

#include <ubixos/exec.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>

void lnc_thread();
void shell_thread(void *);

struct netif lnc_netif;

int net_init() {
  ip_addr_t ipaddr, netmask, gw;

  tcpip_init(NULL, NULL);

  IP4_ADDR(&gw, 10, 50, 0, 1);
  IP4_ADDR(&ipaddr, 10, 50, 0, 7);
  IP4_ADDR(&netmask, 255, 255, 0, 0);

  netif_add(&lnc_netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, tcpip_input);
  netif_set_link_up(&lnc_netif);
  netif_set_up(&lnc_netif);

  netif_set_default(&lnc_netif);
  sys_thread_new("lncThread", (void *) lnc_thread, 0x0, 0x1000, 0x0);
  sys_thread_new("shellThread", (void *) shell_thread, 0x0, 0x1000, 0x0);

  return(0x0);
}
