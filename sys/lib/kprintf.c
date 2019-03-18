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

#include <lib/kprintf.h>
#include <string.h>
#include <sys/video.h>
#include <ubixos/kpanic.h>

static char *ksprintn(char *nbuf, uintmax_t num, int base, int *lenp, int upper);

static __inline int imax(int a, int b) {
  return (a > b ? a : b);
}

union uu {
  quad_t q; /* as a (signed) quad */
  quad_t uq; /* as an unsigned quad */
  long sl[2]; /* as two signed longs */
  u_long ul[2]; /* as two unsigned longs */
};

static void __shl(register digit *p, register int len, register int sh) {
  register int i;

  for (i = 0; i < len; i++)
    p[i] = LHALF( p[i] << sh ) | (p[i + 1] >> (HALF_BITS - sh));

  p[i] = LHALF(p[i] << sh);
}

u_quad_t __qdivrem(u_quad_t uq, u_quad_t vq, u_quad_t *arq) {
  union uu tmp;
  digit *u, *v, *q;
  register digit v1, v2;
  u_long qhat, rhat, t;
  int m, n, d, j, i;
  digit uspace[5], vspace[5], qspace[5];

  /*
   * Take care of special cases: divide by zero, and u < v.
   */
  if (vq == 0) {
    /* divide by zero. */
    static const volatile unsigned int zero = 0;

    tmp.ul[H] = tmp.ul[L] = 1 / zero;
    if (arq)
      *arq = uq;
    return (tmp.q);
  }
  if (uq < vq) {
    if (arq)
      *arq = uq;
    return (0);
  }
  u = &uspace[0];
  v = &vspace[0];
  q = &qspace[0];

  /*
   * Break dividend and divisor into digits in base B, then
   * count leading zeros to determine m and n.  When done, we
   * will have:
   *      u = (u[1]u[2]...u[m+n]) sub B
   *      v = (v[1]v[2]...v[n]) sub B
   *      v[1] != 0
   *      1 < n <= 4 (if n = 1, we use a different division algorithm)
   *      m >= 0 (otherwise u < v, which we already checked)
   *      m + n = 4
   * and thus
   *      m = 4 - n <= 2
   */
  tmp.uq = uq;
  u[0] = 0;
  u[1] = HHALF(tmp.ul[H]);
  u[2] = LHALF(tmp.ul[H]);
  u[3] = HHALF(tmp.ul[L]);
  u[4] = LHALF(tmp.ul[L]);
  tmp.uq = vq;
  v[1] = HHALF(tmp.ul[H]);
  v[2] = LHALF(tmp.ul[H]);
  v[3] = HHALF(tmp.ul[L]);
  v[4] = LHALF(tmp.ul[L]);
  for (n = 4; v[1] == 0; v++) {
    if (--n == 1) {
      u_long rbj; /* r*B+u[j] (not root boy jim) */
      digit q1, q2, q3, q4;

      /*
       * Change of plan, per exercise 16.
       *      r = 0;
       *      for j = 1..4:
       *              q[j] = floor((r*B + u[j]) / v),
       *              r = (r*B + u[j]) % v;
       * We unroll this completely here.
       */
      t = v[2]; /* nonzero, by definition */
      q1 = u[1] / t;
      rbj = COMBINE(u[1] % t, u[2]);
      q2 = rbj / t;
      rbj = COMBINE(rbj % t, u[3]);
      q3 = rbj / t;
      rbj = COMBINE(rbj % t, u[4]);
      q4 = rbj / t;
      if (arq)
        *arq = rbj % t;
      tmp.ul[H] = COMBINE(q1, q2);
      tmp.ul[L] = COMBINE(q3, q4);
      return (tmp.q);
    }
  }
  /*
   * By adjusting q once we determine m, we can guarantee that
   * there is a complete four-digit quotient at &qspace[1] when
   * we finally stop.
   */
  for (m = 4 - n; u[1] == 0; u++)
    m--;
  for (i = 4 - m; --i >= 0;)
    q[i] = 0;
  q += 4 - m;

  /*
   * Here we run Program D, translated from MIX to C and acquiring
   * a few minor changes.
   *
   * D1: choose multiplier 1 << d to ensure v[1] >= B/2.
   */
  d = 0;
  for (t = v[1]; t < B / 2; t <<= 1)
    d++;
  if (d > 0) {
    __shl(&u[0], m + n, d); /* u <<= d */
    __shl(&v[1], n - 1, d); /* v <<= d */
  }
  /*
   * D2: j = 0.
   */
  j = 0;
  v1 = v[1]; /* for D3 -- note that v[1..n] are constant */
  v2 = v[2]; /* for D3 */
  do {
    register digit uj0, uj1, uj2;

    /*
     * D3: Calculate qhat (\^q, in TeX notation).
     * Let qhat = min((u[j]*B + u[j+1])/v[1], B-1), and
     * let rhat = (u[j]*B + u[j+1]) mod v[1].
     * While rhat < B and v[2]*qhat > rhat*B+u[j+2],
     * decrement qhat and increase rhat correspondingly.
     * Note that if rhat >= B, v[2]*qhat < rhat*B.
     */
    uj0 = u[j + 0]; /* for D3 only -- note that u[j+...] change */
    uj1 = u[j + 1]; /* for D3 only */
    uj2 = u[j + 2]; /* for D3 only */
    if (uj0 == v1) {
      qhat = B;
      rhat = uj1;
      goto qhat_too_big;
    }
    else {
      u_long nn = COMBINE(uj0, uj1);
      qhat = nn / v1;
      rhat = nn % v1;
    }
    while (v2 * qhat > COMBINE(rhat, uj2)) {
      qhat_too_big: qhat--;
      if ((rhat += v1) >= B)
        break;
    }
    /*
     * D4: Multiply and subtract.
     * The variable `t' holds any borrows across the loop.
     * We split this up so that we do not require v[0] = 0,
     * and to eliminate a final special case.
     */
    for (t = 0, i = n; i > 0; i--) {
      t = u[i + j] - v[i] * qhat - t;
      u[i + j] = LHALF(t);
      t = (B - HHALF(t)) & (B - 1);
    }
    t = u[j] - t;
    u[j] = LHALF(t);
    /*
     * D5: test remainder.
     * There is a borrow if and only if HHALF(t) is nonzero;
     * in that (rare) case, qhat was too large (by exactly 1).
     * Fix it by adding v[1..n] to u[j..j+n].
     */
    if (HHALF(t)) {
      qhat--;
      for (t = 0, i = n; i > 0; i--) { /* D6: add back. */
        t += u[i + j] + v[i];
        u[i + j] = LHALF(t);
        t = HHALF(t);
      }
      u[j] = LHALF(u[j] + t);
    }
    q[j] = qhat;
  } while (++j <= m); /* D7: loop on j. */

  /*
   * If caller wants the remainder, we have to calculate it as
   * u[m..m+n] >> d (this is at most n digits and thus fits in
   * u[m+1..m+n], but we may need more source digits).
   */
  if (arq) {
    if (d) {
      for (i = m + n; i > m; --i)
        u[i] = (u[i] >> d) | LHALF(u[i - 1] << (HALF_BITS - d));
      u[i] = 0;
    }
    tmp.ul[H] = COMBINE(uspace[1], uspace[2]);
    tmp.ul[L] = COMBINE(uspace[3], uspace[4]);
    *arq = tmp.q;
  }

  tmp.ul[H] = COMBINE(qspace[1], qspace[2]);
  tmp.ul[L] = COMBINE(qspace[3], qspace[4]);
  return (tmp.q);
}

u_quad_t __umoddi3(a, b)
  u_quad_t a, b; {
  u_quad_t r;

  (void) __qdivrem(a, b, &r);
  return (r);
}

int printOff = 0x0;
int ogprintOff = 0x1;

int kprintf(const char *fmt, ...) {
  va_list ap;
  int retval;
  char buf[512];

  va_start(ap, fmt);

  retval = kvprintf(fmt, NULL, &buf, 10, ap);
  buf[retval] = '\0';
  va_end(ap);

  if (printOff == 0x0)
    kprint(buf);
  if (ogprintOff == 0x0)
    ogPrintf(buf);

  return (retval);
}

int sprintf(char *buf, const char *fmt, ...) {
  va_list args;
  int i;
  va_start(args, fmt);
  /* i = vsprintf( buf, fmt, args ); */
  i = kvprintf(fmt, NULL, buf, 10, args);
  va_end(args);
  return (i);
}

/*
 * Scaled down version of printf(3).
 *
 * Two additional formats:
 *
 * The format %b is supported to decode error registers.
 * Its usage is:
 *
 *      printf("reg=%b\n", regval, "<base><arg>*");
 *
 * where <base> is the output base expressed as a control character, e.g.
 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
 * the first of which gives the bit number to be inspected (origin 1), and
 * the next characters (up to a control character, i.e. a character <= 32),
 * give the name of the register.  Thus:
 *
 *      kvprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 *
 * would produce output:
 *
 *      reg=3<BITTWO,BITONE>
 *
 * XXX:  %D  -- Hexdump, takes pointer and separator string:
 *              ("%6D", ptr, ":")   -> XX:XX:XX:XX:XX:XX
 *              ("%*D", len, ptr, " " -> XX XX XX XX ...
 */

int kvprintf(const char *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap) {
#define PCHAR(c) {int cc=(c); if (func) (*func)(cc,arg); else *d++ = cc; retval++; }
  char nbuf[MAXNBUF];
  char *d;
  const char *p, *percent, *q;
  u_char *up;
  int ch, n;
  uintmax_t num;
  int base, lflag, qflag, tmp, width, ladjust, sharpflag, neg, sign, dot;
  int cflag, hflag, jflag, tflag, zflag;
  int dwidth, upper;
  char padc;
  int stop = 0, retval = 0;

  num = 0;
  if (!func)
    d = (char *) arg;
  else
    d = NULL;

  if (fmt == NULL)
    fmt = "(fmt null)\n";

  if (radix < 2 || radix > 36)
    radix = 10;

  for (;;) {
    padc = ' ';
    width = 0;
    while ((ch = (u_char) *fmt++) != '%' || stop) {
      if (ch == '\0')
        return (retval);
      PCHAR(ch);
    }
    percent = fmt - 1;
    qflag = 0;
    lflag = 0;
    ladjust = 0;
    sharpflag = 0;
    neg = 0;
    sign = 0;
    dot = 0;
    dwidth = 0;
    upper = 0;
    cflag = 0;
    hflag = 0;
    jflag = 0;
    tflag = 0;
    zflag = 0;
    reswitch: switch (ch = (u_char) *fmt++) {
      case '.':
        dot = 1;
        goto reswitch;
      case '#':
        sharpflag = 1;
        goto reswitch;
      case '+':
        sign = 1;
        goto reswitch;
      case '-':
        ladjust = 1;
        goto reswitch;
      case '%':
        PCHAR(ch)
        ;
      break;
      case '*':
        if (!dot) {
          width = va_arg(ap, int);
          if (width < 0) {
            ladjust = !ladjust;
            width = -width;
          }
        }
        else {
          dwidth = va_arg(ap, int);
        }
        goto reswitch;
      case '0':
        if (!dot) {
          padc = '0';
          goto reswitch;
        }
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        for (n = 0;; ++fmt) {
          n = n * 10 + ch - '0';
          ch = *fmt;
          if (ch < '0' || ch > '9')
            break;
        }
        if (dot)
          dwidth = n;
        else
          width = n;
        goto reswitch;
      case 'b':
        num = (u_int) va_arg(ap, int);
        p = va_arg(ap, char *);
        for (q = ksprintn(nbuf, num, *p++, NULL, 0); *q;)
          PCHAR(*q--)
        ;

        if (num == 0)
          break;

        for (tmp = 0; *p;) {
          n = *p++;
          if (num & (1 << (n - 1))) {
            PCHAR(tmp ? ',' : '<');
            for (; (n = *p) > ' '; ++p)
              PCHAR(n);
            tmp = 1;
          }
          else
            for (; *p > ' '; ++p)
              continue;
        }
        if (tmp)
          PCHAR('>')
        ;
      break;
      case 'c':
        PCHAR(va_arg(ap, int))
        ;
      break;
      case 'D':
        up = va_arg(ap, u_char *);
        p = va_arg(ap, char *);
        if (!width)
          width = 16;
        while (width--) {
          PCHAR(hex2ascii( *up >> 4 ));
          PCHAR(hex2ascii( *up & 0x0f ));
          up++;
          if (width)
            for (q = p; *q; q++)
              PCHAR(*q);
        }
      break;
      case 'd':
      case 'i':
        base = 10;
        sign = 1;
        goto handle_sign;
      case 'h':
        if (hflag) {
          hflag = 0;
          cflag = 1;
        }
        else
          hflag = 1;
        goto reswitch;
      case 'j':
        jflag = 1;
        goto reswitch;
      case 'l':
        if (lflag) {
          lflag = 0;
          qflag = 1;
        }
        else
          lflag = 1;
        goto reswitch;
      case 'n':
        if (jflag)
          *(va_arg(ap, intmax_t *)) = retval;
        else if (qflag)
          *(va_arg(ap, quad_t *)) = retval;
        else if (lflag)
          *(va_arg(ap, long *)) = retval;
        else if (zflag)
          *(va_arg(ap, size_t *)) = retval;
        else if (hflag)
          *(va_arg(ap, short *)) = retval;
        else if (cflag)
          *(va_arg(ap, char *)) = retval;
        else
          *(va_arg(ap, int *)) = retval;
      break;
      case 'o':
        base = 8;
        goto handle_nosign;
      case 'p':
        base = 16;
        sharpflag = (width == 0);
        sign = 0;
        num = (uintptr_t) va_arg(ap, void *);
        goto number;
      case 'q':
        qflag = 1;
        goto reswitch;
      case 'r':
        base = radix;
        if (sign)
          goto handle_sign;
        goto handle_nosign;
      case 's':
        p = va_arg(ap, char *);
        if (p == NULL)
          p = "(null)";
        if (!dot)
          n = strlen(p);
        else
          for (n = 0; n < dwidth && p[n]; n++)
            continue;

        width -= n;

        if (!ladjust && width > 0)
          while (width--)
            PCHAR(padc)
        ;
        while (n--)
          PCHAR(*p++)
        ;
        if (ladjust && width > 0)
          while (width--)
            PCHAR(padc)
        ;
      break;
      case 't':
        tflag = 1;
        goto reswitch;
      case 'u':
        base = 10;
        goto handle_nosign;
      case 'X':
        upper = 1;
      case 'x':
        base = 16;
        goto handle_nosign;
      case 'y':
        base = 16;
        sign = 1;
        goto handle_sign;
      case 'z':
        zflag = 1;
        goto reswitch;
        handle_nosign: sign = 0;
        if (jflag)
          num = va_arg(ap, uintmax_t);
        else if (qflag)
          num = va_arg(ap, u_quad_t);
        else if (tflag)
          num = va_arg(ap, ptrdiff_t);
        else if (lflag)
          num = va_arg(ap, u_long);
        else if (zflag)
          num = va_arg(ap, size_t);
        else if (hflag)
          num = (u_short) va_arg(ap, int);
        else if (cflag)
          num = (u_char) va_arg(ap, int);
        else
          num = va_arg(ap, u_int);
        goto number;
        handle_sign: if (jflag)
          num = va_arg(ap, intmax_t);
        else if (qflag)
          num = va_arg(ap, quad_t);
        else if (tflag)
          num = va_arg(ap, ptrdiff_t);
        else if (lflag)
          num = va_arg(ap, long);
        else if (zflag)
          num = va_arg(ap, ssize_t);
        else if (hflag)
          num = (short) va_arg(ap, int);
        else if (cflag)
          num = (char) va_arg(ap, int);
        else
          num = va_arg(ap, int);
        number: if (sign && (intmax_t) num < 0) {
          neg = 1;
          num = -(intmax_t) num;
        }
        p = ksprintn(nbuf, num, base, &n, upper);
        tmp = 0;
        if (sharpflag && num != 0) {
          if (base == 8)
            tmp++;
          else if (base == 16)
            tmp += 2;
        }
        if (neg)
          tmp++;

        if (!ladjust && padc == '0')
          dwidth = width - tmp;
        width -= tmp + imax(dwidth, n);
        dwidth -= n;
        if (!ladjust)
          while (width-- > 0)
            PCHAR(' ')
        ;
        if (neg)
          PCHAR('-')
        ;
        if (sharpflag && num != 0) {
          if (base == 8) {
            PCHAR('0');
          }
          else if (base == 16) {
            PCHAR('0');
            PCHAR('x');
          }
        }
        while (dwidth-- > 0)
          PCHAR('0')
        ;

        while (*p)
          PCHAR(*p--)
        ;

        if (ladjust)
          while (width-- > 0)
            PCHAR(' ')
        ;

      break;
      default:
        while (percent < fmt)
          PCHAR(*percent++)
        ;
        /*
         * Since we ignore a formatting argument it is no
         * longer safe to obey the remaining formatting
         * arguments as the arguments will no longer match
         * the format specs.
         */
        stop = 1;
      break;
    }
  }
#undef PCHAR
  return (0);
}

static char *ksprintn(char *nbuf, uintmax_t num, int base, int *lenp, int upper) {
  char *p, c;

  p = nbuf;
  *p = '\0';

  do {
    c = hex2ascii(num % base);
    *++p = upper ? toupper(c) : c;
  } while (num /= base);

  if (lenp)
    *lenp = p - nbuf;

  return (p);
}

/***
 END
 ***/

