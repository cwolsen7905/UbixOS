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

#include <sys/limits.h>

/* Magic numbers for the algorithm */
#if LONG_BIT == 32
static const unsigned long mask01 = 0x01010101;
static const unsigned long mask80 = 0x80808080;
#elif LONG_BIT == 64
static const unsigned long mask01 = 0x0101010101010101;
static const unsigned long mask80 = 0x8080808080808080;
#else
#error Unsupported word size
#endif

#define LONGPTR_MASK (sizeof(long) - 1)

/*
 * Helper macro to return string length if we caught the zero
 * byte.
 */
#define testbyte(x)                             \
        do {                                    \
                if (p[x] == '\0')               \
                    return (p - str + x);       \
        } while (0)

size_t strlen(const char *str) {
  const char *p;
  const unsigned long *lp;
  long va, vb;

  /*
   * Before trying the hard (unaligned byte-by-byte access) way
   * to figure out whether there is a nul character, try to see
   * if there is a nul character is within this accessible word
   * first.
   *
   * p and (p & ~LONGPTR_MASK) must be equally accessible since
   * they always fall in the same memory page, as long as page
   * boundaries is integral multiple of word size.
   */
  lp = (const unsigned long *) ((uintptr_t) str & ~LONGPTR_MASK);
  va = (*lp - mask01);
  vb = ((~*lp) & mask80);
  lp++;
  if (va & vb)
    /* Check if we have \0 in the first part */
    for (p = str; p < (const char *) lp; p++)
      if (*p == '\0')
        return (p - str);

  /* Scan the rest of the string using word sized operation */
  for (;; lp++) {
    va = (*lp - mask01);
    vb = ((~*lp) & mask80);
    if (va & vb) {
      p = (const char *) (lp);
      testbyte(0);
      testbyte(1);
      testbyte(2);
      testbyte(3);
#if (LONG_BIT >= 64)
      testbyte(4);
      testbyte(5);
      testbyte(6);
      testbyte(7);
#endif
    }
  }

  /* NOTREACHED */
  return (0);
}
