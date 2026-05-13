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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <string.h>

/*
 * Raw sysctl(2) — syscall 202 (0xCA).
 * Arguments are passed on the stack; the kernel reads them directly.
 */
int sysctl(int *name, unsigned int namelen,
           void *oldp, size_t *oldlenp,
           void *newp, size_t newlen) {
  int ret;
  asm volatile(
    "int $0x80"
    : "=a"(ret)
    : "a"(0xCA), "b"(name), "c"(namelen), "d"(oldp), "S"(oldlenp), "D"(newp)
    : "memory"
  );
  return ret;
}

/*
 * sysctlbyname — resolve dotted name to MIB then read value.
 * Uses the kernel's CTL_UNSPEC / name_to_mib mechanism (name[0]=0, name[1]=3).
 */
int sysctlbyname(const char *name, void *oldp, size_t *oldlenp,
                 void *newp, size_t newlen) {
  int mib[CTL_MAXNAME];
  size_t miblen = sizeof(mib);
  int lookup[2] = { 0, 3 };

  if (sysctl(lookup, 2, mib, &miblen, (void *)name, strlen(name) + 1) < 0)
    return -1;

  return sysctl(mib, (unsigned int)(miblen / sizeof(int)),
                oldp, oldlenp, newp, newlen);
}
