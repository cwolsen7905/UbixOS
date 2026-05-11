/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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

#ifndef _SYS_SYSCTL_H_
#define _SYS_SYSCTL_H_

#include <sys/types.h>

#define CTL_MAXNAME  24

/* Top-level MIB identifiers */
#define CTL_UNSPEC    0
#define CTL_KERN      1
#define CTL_VM        2
#define CTL_VFS       3
#define CTL_NET       4
#define CTL_DEBUG     5
#define CTL_HW        6
#define CTL_MACHDEP   7
#define CTL_USER      8

/* kern subtree */
#define KERN_OSTYPE      1
#define KERN_OSRELEASE   2
#define KERN_VERSION     4
#define KERN_HOSTNAME   10
#define KERN_OSRELDATE  24

/* hw subtree */
#define HW_MACHINE  1
#define HW_NCPU     3

/*
 * syscall 202 wrapper.
 * Pass name=NULL/namelen=0 and newp pointing to a NUL-terminated string to
 * resolve a name to its MIB (CTL_UNSPEC / name_to_mib mode).
 * Pass a numeric MIB array to read/write a value.
 */
int sysctl(int *name, unsigned int namelen,
           void *oldp, size_t *oldlenp,
           void *newp, size_t newlen);

/*
 * Convenience: look up a dotted-name string (e.g. "kern.osrelease"),
 * copy the string value into buf[buflen].  Returns 0 on success, -1 on error.
 */
int sysctlbyname(const char *name, void *oldp, size_t *oldlenp,
                 void *newp, size_t newlen);

#endif /* _SYS_SYSCTL_H_ */
