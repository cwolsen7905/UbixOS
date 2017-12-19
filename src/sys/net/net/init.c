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

#include <net/sys.h>
#include <net/mem.h>
#include <net/memp.h>
#include <net/tcpip.h>

#include <netif/loopif.h>
#include <netif/tcpdump.h>
#include <netif/ethernetif.h>

#include <ubixos/exec.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>

void netMainThread();
static void tcpip_init_done(void *arg);

int net_init() {
  sys_init();
  mem_init();
  memp_init();
  pbuf_init();

  /*
  *thread = (void *)execThread((void *)start_routine,0x0,arg);
  */
  
  sys_thread_new((void *)(netMainThread), 0x0);

  return(0x0);
  }


void netMainThread() {
  struct ip_addr ipaddr, netmask, gw;
  sys_sem_t sem;
 
  netif_init();
  sem = sys_sem_new(0);
  tcpip_init(tcpip_init_done, &sem);
  kprintf("WAIT: %i]",_current->id);
  sys_sem_wait(sem);
  kprintf("FREE");
  sys_sem_free(sem);

  kprintf("TCP/IP initialized.\n");

  IP4_ADDR(&gw, 10,50,0,1);
  IP4_ADDR(&ipaddr, 10,50,6,65);
  IP4_ADDR(&netmask, 255,255,0,0);
  netif_set_default(netif_add(&ipaddr, &netmask, &gw, ethernetif_init, tcpip_input));

  IP4_ADDR(&gw, 127,0,0,1);
  IP4_ADDR(&ipaddr, 127,0,0,1);
  IP4_ADDR(&netmask, 255,0,0,0);
  netif_add(&ipaddr, &netmask, &gw, loopif_init, tcpip_input);

  //udpecho_init();
  shell_init();
  //bot_init();
  irqEnable(0x9);
  endTask(_current->id);
  }


static void tcpip_init_done(void *arg) {
  sys_sem_t *sem = 0x0;
  sem = arg;
  kprintf("SIG");
  sys_sem_signal(*sem);
  kprintf("NAL");
  }

/***
 END
 ***/

