/*
 * Copyright (c) 2001, Swedish Institute of Computer Science.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id$
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include <ubixos/types.h>
#include <ubixos/sched.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <sys/device.old.h>
#include <isa/ne2k.h>


#include "net/debug.h"

#include "net/opt.h"
#include "net/def.h"
#include "net/mem.h"
#include "net/pbuf.h"
#include "net/sys.h"

#include "netif/arp.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'd'

struct nicBuffer *tmpBuf = 0x0;

struct ethernetif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
};

static const struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};

/* Forward declarations. */
static void  ethernetif_input(struct netif *netif);
static err_t ethernetif_output(struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr);
static void ethernetif_thread();
struct device *dev = 0x0;

/*-----------------------------------------------------------------------------------*/
static void low_level_init(struct netif *netif) {
  struct ethernetif *ethernetif;

  ethernetif = netif->state;
  dev = (struct device *)kmalloc(sizeof(struct device));
  dev->ioAddr = 0x280;
  dev->irq    = 0x10;
  
  /* Obtain MAC address from network interface. */
  ethernetif->ethaddr->addr[0] = 0x00;
  ethernetif->ethaddr->addr[1] = 0x00;
  ethernetif->ethaddr->addr[2] = 0xC0;
  ethernetif->ethaddr->addr[3] = 0x97;
  ethernetif->ethaddr->addr[4] = 0xC6;
  ethernetif->ethaddr->addr[5] = 0x93;

  /* Do whatever else is needed to initialize interface. */  
  kprintf("NETIF: [0x%X:0x%X]\n",netif,ethernetif_thread);
  sys_thread_new(ethernetif_thread, netif);
 
  }
/*-----------------------------------------------------------------------------------*/
/*
 * low_level_output():
 *
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 */
/*-----------------------------------------------------------------------------------*/

static err_t low_level_output(struct ethernetif *ethernetif, struct pbuf *p) {
  struct pbuf *q;
  char buf[1500];
  char *bufptr = 0x0;

  dev->ioAddr = 0x280;
  dev->irq    = 10;

  bufptr = &buf[0];

  for(q = p; q != NULL; q = q->next) {
    bcopy(q->payload, bufptr, q->len);
    bufptr += q->len;
    }
  PCtoNIC(dev,buf,p->tot_len);
  //kprintf("Sending Data[%i]\n",p->tot_len); 
  return ERR_OK;
  }


/*-----------------------------------------------------------------------------------*/
/*
 * low_level_input():
 *
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 */
/*-----------------------------------------------------------------------------------*/
static struct pbuf *low_level_input(struct ethernetif *ethernetif) {
  struct pbuf *p, *q;
  uInt16 len;
  char *bufptr;
  char *buf;

  len = tmpBuf->length;
  bufptr = tmpBuf->buffer;


  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_LINK, len, PBUF_POOL);

  if(p != NULL) {
    /* We iterate over the pbuf chain until we have read the entire
       packet into the pbuf. */
    //bufptr = &buf[0];
    for(q = p; q != NULL; q = q->next) {
      /* Read enough bytes to fill this pbuf in the chain. The
         avaliable data in the pbuf is given by the q->len
         variable. */
      /* read data into(q->payload, q->len); */
      bcopy(bufptr, q->payload, q->len);
      buf = q->payload;
      bufptr += q->len;
    }
    /* acknowledge that packet has been read(); */
  } else {
    /* drop packet(); */
  }
  return p;
}


/*-----------------------------------------------------------------------------------*/
/*
 * ethernetif_output():
 *
 * This function is called by the TCP/IP stack when an IP packet
 * should be sent. It calls the function called low_level_output() to
 * do the actuall transmission of the packet.
 *
 */
/*-----------------------------------------------------------------------------------*/
static err_t ethernetif_output(struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr) {
  struct ethernetif *ethernetif;
  struct pbuf *q;
  struct eth_hdr *ethhdr;
  struct eth_addr *dest, mcastaddr;
  struct ip_addr *queryaddr;
  err_t err;
  uInt8 i;
  
  ethernetif = netif->state;


  /* Make room for Ethernet header. */
  if(pbuf_header(p, sizeof(struct eth_hdr)) != 0) {
    /* The pbuf_header() call shouldn't fail, but we allocate an extra
       pbuf just in case. */
    q = pbuf_alloc(PBUF_LINK, sizeof(struct eth_hdr), PBUF_RAM);
    if(q == NULL) {
      return ERR_MEM;
    }
    pbuf_chain(q, p);
    p = q;
  }

  /* Construct Ethernet header. Start with looking up deciding which
     MAC address to use as a destination address. Broadcasts and
     multicasts are special, all other addresses are looked up in the
     ARP table. */
  queryaddr = ipaddr;
  if(ip_addr_isany(ipaddr) ||
     ip_addr_isbroadcast(ipaddr, &(netif->netmask))) {
    dest = (struct eth_addr *)&ethbroadcast;
  } else if(ip_addr_ismulticast(ipaddr)) {
    /* Hash IP multicast address to MAC address. */
    mcastaddr.addr[0] = 0x01;
    mcastaddr.addr[1] = 0x0;
    mcastaddr.addr[2] = 0x5e;
    mcastaddr.addr[3] = ip4_addr2(ipaddr) & 0x7f;
    mcastaddr.addr[4] = ip4_addr3(ipaddr);
    mcastaddr.addr[5] = ip4_addr4(ipaddr);
    dest = &mcastaddr;
  } else {
    if(ip_addr_maskcmp(ipaddr, &(netif->ip_addr), &(netif->netmask))) {
      /* Use destination IP address if the destination is on the same
         subnet as we are. */
      queryaddr = ipaddr;
    } else {
      /* Otherwise we use the default router as the address to send
         the Ethernet frame to. */
      queryaddr = &(netif->gw);
    }
    dest = arp_lookup(queryaddr);
  }


  /* If the arp_lookup() didn't find an address, we send out an ARP
     query for the IP address. */
  if(dest == NULL) {
    q = arp_query(netif, ethernetif->ethaddr, queryaddr);
    if(q != NULL) {
      err = low_level_output(ethernetif, q);
      pbuf_free(q);
      return err;
    }
    return ERR_MEM;
  }
  ethhdr = p->payload;

  for(i = 0; i < 6; i++) {
    ethhdr->dest.addr[i] = dest->addr[i];
    ethhdr->src.addr[i] = ethernetif->ethaddr->addr[i];
  }
  
  ethhdr->type = htons(ETHTYPE_IP);
  
  return low_level_output(ethernetif, p);

}
/*-----------------------------------------------------------------------------------*/
/*
 * ethernetif_input():
 *
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface.
 *
 */
/*-----------------------------------------------------------------------------------*/
static void ethernetif_input(struct netif *netif) {
  struct ethernetif *ethernetif = 0x0;
  struct eth_hdr *ethhdr = 0x0;
  struct pbuf *p = 0x0;

  ethernetif = netif->state;
 
  p = low_level_input(ethernetif);

  if(p != NULL) {

    ethhdr = p->payload;
    
    switch(htons(ethhdr->type)) {
    case ETHTYPE_IP:
      arp_ip_input(netif, p);
      pbuf_header(p, -14);
      netif->input(p, netif);
      break;
    case ETHTYPE_ARP:
      p = arp_arp_input(netif, ethernetif->ethaddr, p);
      if(p != NULL) {
	low_level_output(ethernetif, p);
	pbuf_free(p);
      }
      break;
    default:
      pbuf_free(p);
      break;
    }
  }
}
/*-----------------------------------------------------------------------------------*/
static void
arp_timer(void *arg)
{
  arp_tmr();
  sys_timeout(ARP_TMR_INTERVAL, (sys_timeout_handler)arp_timer, NULL);
}

/*-----------------------------------------------------------------------------------*/
/*
 * ethernetif_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */
/*-----------------------------------------------------------------------------------*/
void ethernetif_init(struct netif *netif) {
  struct ethernetif *ethernetif;
    
  ethernetif = mem_malloc(sizeof(struct ethernetif));
  netif->state = ethernetif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  netif->output = ethernetif_output;
  netif->linkoutput = (void *)low_level_output;
  
  ethernetif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);
  
  low_level_init(netif);
  arp_init();

  sys_timeout(ARP_TMR_INTERVAL, (sys_timeout_handler)arp_timer, NULL);
  }

/*-----------------------------------------------------------------------------------*/

void ethernetif_thread(void *arg) {
  struct netif *netif = 0x0;
 
  netif = arg;
  
  while (1) {
    tmpBuf = ne2kGetBuffer();
    if (tmpBuf && tmpBuf->length > 0x0) {
      ethernetif_input(netif);  
      }
    }
  ne2kFreeBuffer(tmpBuf);
  }
