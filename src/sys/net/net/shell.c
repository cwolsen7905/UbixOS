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
 * $Id: shell.c 54 2016-01-11 01:29:55Z reddawg $
 */

#include <lib/kmalloc.h>
#include <ubixos/fork.h>
#include <string.h>

#include "net/mem.h"
#include "net/debug.h"
#include "net/def.h"
#include <net/api.h>
#include "net/stats.h"

#include "lib/kprintf.h"
char *buffer = 0x0;

#define ESUCCESS 0
#define ESYNTAX -1
#define ETOOFEW -2
#define ETOOMANY -3
#define ECLOSED -4

#define NCONNS 10
// UBU static struct netconn *conns[NCONNS];

static void sendstr(const char *str, struct netconn *conn) {
  netconn_write(conn, (void *) str, strlen(str), NETCONN_NOCOPY);
}

static void prompt(struct netconn *conn) {
  sendstr("uBixCube:@sys> ", conn);
}

static void shell_main(struct netconn *conn) {
  struct netbuf *buf = 0x0;
  uInt32 len;
  // UBU int i;
  // UBU char bufr[1500];
  prompt(conn);
  while (1) {
    buf = netconn_recv(conn);
    if (buf != 0x0)
      netbuf_copy(buf, buffer, 1024);
    len = netbuf_len(buf);
    netbuf_delete(buf);
    buffer[len - 2] = '\0';
    kprintf("Buffer: [%s:%i]\n", buffer, len);
    if (!strcmp(buffer, "quit")) {
      netconn_close(conn);
      break;
    }
    else if (!strcmp(buffer, "ls")) {
      sendstr("no\nfiles\nhere\n", conn);
    }
    else if (!strcmp(buffer, "uname")) {
      sendstr("UbixOS v1.0 reddawg@devel.ubixos.com:/ubix.elf\n", conn);
    }
    prompt(conn);
  }
}

static void shell_thread(void *arg) {
  struct netconn *conn = 0x0, *newconn = 0x0;

  buffer = (char *) kmalloc(1024);

  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 23);
  netconn_listen(conn);

  while (1) {
    kprintf("shell1");
    newconn = netconn_accept(conn);
    kprintf("2");
    shell_main(newconn);
    kprintf("3");
    //netconn_delete(newconn);
    kprintf("4");
  }
}

void shell_init(void) {
  sys_thread_new(shell_thread, NULL);
}

/***
 END
 ***/

