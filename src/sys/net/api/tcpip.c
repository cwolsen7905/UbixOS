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
 * $Id: tcpip.c 54 2016-01-11 01:29:55Z reddawg $
 */

#include <ubixos/exec.h>
#include <lib/kmalloc.h>

#include "net/debug.h"

#include "net/opt.h"

#include "net/sys.h"

#include "net/memp.h"
#include "net/pbuf.h"

#include "net/ipv4/ip.h"
#include "net/udp.h"
#include "net/tcp.h"

#include "net/tcpip.h"

static void (*tcpip_init_done)(void *arg) = NULL;
static void *tcpip_init_done_arg;
static sys_mbox_t mbox;

/*-----------------------------------------------------------------------------------*/
static void tcpip_tcp_timer(void *arg) {
  tcp_tmr();
  sys_timeout(TCP_TMR_INTERVAL, (sys_timeout_handler) tcpip_tcp_timer, NULL);
}
/*-----------------------------------------------------------------------------------*/

static void tcpip_thread(void *arg) {
  struct tcpip_msg *msg;

  kprintf("Started TCP THread");
  ip_init();
  udp_init();
  tcp_init();
  kprintf("Started TCP THread");

  sys_timeout(TCP_TMR_INTERVAL, (sys_timeout_handler) tcpip_tcp_timer, NULL);

  if (tcpip_init_done != NULL) {
    tcpip_init_done(tcpip_init_done_arg);
  }

  while (1) { /* MAIN Loop */
    kprintf("MBF1");
    sys_mbox_fetch(mbox, (void *) &msg);
    kprintf("MBF1");
    switch (msg->type) {
      case TCPIP_MSG_API:
        //kprintf("tcpip_thread: API message %p\n", msg);
        api_msg_input(msg->msg.apimsg);
      break;
      case TCPIP_MSG_INPUT:
        //kprintf("tcpip_thread: IP packet %p\n", msg);
        ip_input(msg->msg.inp.p, msg->msg.inp.netif);
      break;
      default:
      break;
    }
    memp_freep(MEMP_TCPIP_MSG, msg);
  }
}
/*-----------------------------------------------------------------------------------*/
err_t tcpip_input(struct pbuf *p, struct netif *inp) {
  struct tcpip_msg *msg;

  msg = memp_mallocp(MEMP_TCPIP_MSG);
  if (msg == NULL) {
    kprintf("BAD MESSAGE!!!\n");
    while (1)
      sched_yield();
    pbuf_free(p);
    return ERR_MEM;
  }
  kprintf("GOOD MESSAGE\n");

  msg->type = TCPIP_MSG_INPUT;
  msg->msg.inp.p = p;
  msg->msg.inp.netif = inp;
  sys_mbox_post(mbox, msg);
  return ERR_OK;
}
/*-----------------------------------------------------------------------------------*/
void tcpip_apimsg(struct api_msg *apimsg) {
  struct tcpip_msg *msg;
  msg = memp_mallocp(MEMP_TCPIP_MSG);
  if (msg == NULL) {
    memp_free(MEMP_TCPIP_MSG, apimsg);
    return;
  }
  msg->type = TCPIP_MSG_API;
  msg->msg.apimsg = apimsg;
  sys_mbox_post(mbox, msg);
}
/*-----------------------------------------------------------------------------------*/
void tcpip_init(void (*initfunc)(void *), void *arg) {
  tcpip_init_done = initfunc;
  tcpip_init_done_arg = arg;
  mbox = sys_mbox_new();
  sys_thread_new((void *) tcpip_thread, NULL);
}
/*-----------------------------------------------------------------------------------*/

