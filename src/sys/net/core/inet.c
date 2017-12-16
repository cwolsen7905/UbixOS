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
 * $Id: inet.c,v 1.1 2004/04/15 12:38:25 reddawg Exp $
 */

/*-----------------------------------------------------------------------------------*/
/* inet.c
 *
 * Functions common to all TCP/IP modules, such as the Internet checksum and the
 * byte order functions.
 *
 */
/*-----------------------------------------------------------------------------------*/

#include <sys/types.h>

#include "net/debug.h"

#include "net/arch.h"

#include "net/def.h"

#include "net/ipv4/inet.h"


/*-----------------------------------------------------------------------------------*/
/* chksum:
 *
 * Sums up all 16 bit words in a memory portion. Also includes any odd byte.
 * This function is used by the other checksum functions.
 *
 * For now, this is not optimized. Must be optimized for the particular processor
 * arcitecture on which it is to run. Preferebly coded in assembler.
 */
/*-----------------------------------------------------------------------------------*/
static uInt32 
chksum(void *dataptr, int len)
{
  uInt32 acc;
  uInt32 *ptr = dataptr;
    
  for(acc = 0; len > 1; len -= 2) {
    //acc += *((uInt16 *)dataptr)++;
    acc += *ptr++;
  }

  /* add up any odd byte */
  if(len == 1) {
    acc += htons((uInt16)((*(uInt8 *)dataptr) & 0xff) << 8);
    DEBUGF(INET_DEBUG, ("inet: chksum: odd byte %d\n", *(uInt8 *)dataptr));
  }

  return acc;
}
/*-----------------------------------------------------------------------------------*/
/* inet_chksum_pseudo:
 *
 * Calculates the pseudo Internet checksum used by TCP and UDP for a pbuf chain.
 */
/*-----------------------------------------------------------------------------------*/
uInt16
inet_chksum_pseudo(struct pbuf *p,
		   struct ip_addr *src, struct ip_addr *dest,
		   uInt8 proto, uInt16 proto_len)
{
  uInt32 acc;
  struct pbuf *q;
  uInt8 swapped;

  acc = 0;
  swapped = 0;
  for(q = p; q != NULL; q = q->next) {    
    acc += chksum(q->payload, q->len);
    while(acc >> 16) {
      acc = (acc & 0xffff) + (acc >> 16);
    }
    if(q->len % 2 != 0) {
      swapped = 1 - swapped;
      acc = ((acc & 0xff) << 8) | ((acc & 0xff00) >> 8);
    }
  }

  if(swapped) {
    acc = ((acc & 0xff) << 8) | ((acc & 0xff00) >> 8);
  }
  acc += (src->addr & 0xffff);
  acc += ((src->addr >> 16) & 0xffff);
  acc += (dest->addr & 0xffff);
  acc += ((dest->addr >> 16) & 0xffff);
  acc += (uInt32)htons((uInt16)proto);
  acc += (uInt32)htons(proto_len);  
  
  while(acc >> 16) {
    acc = (acc & 0xffff) + (acc >> 16);
  }    
  return ~(acc & 0xffff);
}
/*-----------------------------------------------------------------------------------*/
/* inet_chksum:
 *
 * Calculates the Internet checksum over a portion of memory. Used primarely for IP
 * and ICMP.
 */
/*-----------------------------------------------------------------------------------*/
uInt16
inet_chksum(void *dataptr, uInt16 len)
{
  uInt32 acc;

  acc = chksum(dataptr, len);
  while(acc >> 16) {
    acc = (acc & 0xffff) + (acc >> 16);
  }    
  return ~(acc & 0xffff);
}
/*-----------------------------------------------------------------------------------*/
uInt16
inet_chksum_pbuf(struct pbuf *p)
{
  uInt32 acc;
  struct pbuf *q;
  uInt8 swapped;
  
  acc = 0;
  swapped = 0;
  for(q = p; q != NULL; q = q->next) {
    acc += chksum(q->payload, q->len);
    while(acc >> 16) {
      acc = (acc & 0xffff) + (acc >> 16);
    }    
    if(q->len % 2 != 0) {
      swapped = 1 - swapped;
      acc = (acc & 0xff << 8) | (acc & 0xff00 >> 8);
    }
  }
 
  if(swapped) {
    acc = ((acc & 0xff) << 8) | ((acc & 0xff00) >> 8);
  }
  return ~(acc & 0xffff);
}


/*-----------------------------------------------------------------------------------*/
