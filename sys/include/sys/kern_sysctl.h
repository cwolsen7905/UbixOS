/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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

#ifndef _SYS_KERN_SYSCTL_H
#define _SYS_KERN_SYSCTL_H

#include <sys/sysproto.h>
#include <sys/thread.h>

#define CTL_MAXNAME     24      /* largest number of components supported */

/*
 * Top-level identifiers
 */
#define CTL_UNSPEC          0           /* unused */
#define CTL_KERN            1           /* "high kernel": proc, limits */
#define CTL_VM              2           /* virtual memory */
#define CTL_VFS             3           /* filesystem, mount type is next */
#define CTL_NET             4           /* network, see socket.h */
#define CTL_DEBUG           5           /* debugging parameters */
#define CTL_HW              6           /* generic cpu/io */
#define CTL_MACHDEP         7           /* machine dependent */
#define CTL_USER            8           /* user-level */
#define CTL_P1003_1B        9           /* POSIX 1003.1B */
#define CTL_UBIX           10           /* ubixos */

#define CTL_KERN_OPENFILES  1           /* kernel openfiles */

#define EINVAL         -1               /* */

struct sysctl_entry {
  struct sysctl_entry *prev;
  struct sysctl_entry *next;
  struct sysctl_entry *children;
  char                 name[32];
  int                  id;
  void                *value;
  int                  val_len;
  };

int kern_sysctl(int *name,u_int namelen,void *old,size_t *oldlenp,void *new,size_t newlen,size_t *retval,int flags);
int sysctl_add(int *,int,char *,void *,int);
int sysctl_init();

extern bool sysctl_enabled;
#endif /* END _SYS_KERN_SYSCTL_H */
