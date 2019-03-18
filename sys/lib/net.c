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

#include <sys/types.h>
#include <net/inet.h>
#include <net/sockets.h>
#include <string.h>

#include "lib/kprintf.h"

#ifndef _IN_ADDR_T_DECLARED
typedef uInt32 in_addr_t;
#define _IN_ADDR_T_DECLARED
#endif

/*
 uInt32 htonl(uInt32 n) {
 uInt32 retVal = 0x0;
 retVal += ((n & 0xff) << 24);
 retVal += ((n & 0xff00) << 8);
 retVal += ((n & 0xff0000) >> 8);
 retVal += ((n & 0xff000000) >> 24);
 return(retVal);
 }
 */

/*
 uInt32 htons(uInt32 n) {
 uInt32 retVal = 0x0;
 retVal = (((n & 0xff) << 8) | ((n & 0xff00) >> 8));
 return(retVal);
 }
 */


#ifdef _INET_ATON
int inet_aton(cp, addr)
const char *cp;
struct in_addr *addr;
{
  uInt32 parts[4];
  in_addr_t val;
  char *c;
  char *endptr;
  int gotend, n;

  c = (char *)cp;
  n = 0;
  /*
   * Run through the string, grabbing numbers until
   * the end of the string, or some error
   */
  gotend = 0;
  while (!gotend) {
    //errno = 0;
    val = strtol(c, &endptr, 0);
    kprintf("VAL: [%x]",val);

    //if (errno == ERANGE)    /* Fail completely if it overflowed. */
    //        return (0);

    /*
     * If the whole string is invalid, endptr will equal
     * c.. this way we can make sure someone hasn't
     * gone '.12' or something which would get past
     * the next check.
     */
    if (endptr == c)
    return (0);
    parts[n] = val;
    c = endptr;

    /* Check the next character past the previous number's end */
    switch (*c) {
      case '.' :
      /* Make sure we only do 3 dots .. */
      if (n == 3) /* Whoops. Quit. */
      return (0);
      n++;
      c++;
      break;

      case '\0':
      gotend = 1;
      break;

      default:
      /*
       if (isspace((unsigned char)*c)) {
       gotend = 1;
       break;
       } else
       */
      return (0); /* Invalid character, so fail */
    }

  }

  /*
   * Concoct the address according to
   * the number of parts specified.
   */

  switch (n) {
    case 0: /* a -- 32 bits */
    /*
     * Nothing is necessary here.  Overflow checking was
     * already done in strtoul().
     */
    break;
    case 1: /* a.b -- 8.24 bits */
    if (val > 0xffffff || parts[0] > 0xff)
    return (0);
    val |= parts[0] << 24;
    break;

    case 2: /* a.b.c -- 8.8.16 bits */
    if (val > 0xffff || parts[0] > 0xff || parts[1] > 0xff)
    return (0);
    val |= (parts[0] << 24) | (parts[1] << 16);
    break;

    case 3: /* a.b.c.d -- 8.8.8.8 bits */
    if (val > 0xff || parts[0] > 0xff || parts[1] > 0xff ||
      parts[2] > 0xff)
    return (0);
    val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
    break;
  }

  if (addr != NULL)
  addr->s_addr = htonl(val);
  return (1);
}
#endif

/***
 END
 ***/

