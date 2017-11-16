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
 * $Id: stats.h 54 2016-01-11 01:29:55Z reddawg $
 */
#ifndef __LWIP_STATS_H__
#define __LWIP_STATS_H__

#include "net/opt.h"
#include "net/arch/cc.h"

#include "net/memp.h"

#ifdef STATS

struct stats_proto {
  uInt16 xmit;    /* Transmitted packets. */
  uInt16 rexmit;  /* Retransmitted packets. */
  uInt16 recv;    /* Received packets. */
  uInt16 fw;      /* Forwarded packets. */
  uInt16 drop;    /* Dropped packets. */
  uInt16 chkerr;  /* Checksum error. */
  uInt16 lenerr;  /* Invalid length error. */
  uInt16 memerr;  /* Out of memory error. */
  uInt16 rterr;   /* Routing error. */
  uInt16 proterr; /* Protocol error. */
  uInt16 opterr;  /* Error in options. */
  uInt16 err;     /* Misc error. */
  uInt16 cachehit;
};

struct stats_mem {
  uInt16 avail;
  uInt16 used;
  uInt16 max;  
  uInt16 err;
  uInt16 reclaimed;
};

struct stats_pbuf {
  uInt16 avail;
  uInt16 used;
  uInt16 max;  
  uInt16 err;
  uInt16 reclaimed;

  uInt16 alloc_locked;
  uInt16 refresh_locked;
};

struct stats_syselem {
  uInt16 used;
  uInt16 max;
  uInt16 err;
};

struct stats_sys {
  struct stats_syselem sem;
  struct stats_syselem mbox;
};

struct stats_ {
  struct stats_proto link;
  struct stats_proto ip;
  struct stats_proto icmp;
  struct stats_proto udp;
  struct stats_proto tcp;
  struct stats_pbuf pbuf;
  struct stats_mem mem;
  struct stats_mem memp[MEMP_MAX];
  struct stats_sys sys;
};

extern struct stats_ stats;

#endif /* STATS */

void stats_init(void);
#endif /* __LWIP_STATS_H__ */




