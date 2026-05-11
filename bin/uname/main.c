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

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

static void usage(void) {
  printf("usage: uname [-amnrsv]\n");
}

int main(int argc, char **argv) {
  struct utsname u;
  int flag_a = 0, flag_m = 0, flag_n = 0, flag_r = 0, flag_s = 0, flag_v = 0;
  int any = 0;
  int i;

  if (uname(&u) != 0) {
    printf("uname: uname() failed\n");
    return 1;
  }

  if (argc == 1) {
    /* default: just sysname */
    printf("%s\n", u.sysname);
    return 0;
  }

  for (i = 1; i < argc; i++) {
    const char *arg = argv[i];
    if (arg[0] != '-') {
      usage();
      return 1;
    }
    const char *p;
    for (p = arg + 1; *p; p++) {
      switch (*p) {
        case 'a': flag_a = 1; break;
        case 'm': flag_m = 1; break;
        case 'n': flag_n = 1; break;
        case 'r': flag_r = 1; break;
        case 's': flag_s = 1; break;
        case 'v': flag_v = 1; break;
        default:
          usage();
          return 1;
      }
    }
  }

  if (flag_a) {
    flag_s = flag_n = flag_r = flag_v = flag_m = 1;
  }

#define PRINT(val) \
  do { if (any) printf(" "); printf("%s", (val)); any = 1; } while(0)

  if (flag_s) PRINT(u.sysname);
  if (flag_n) PRINT(u.nodename);
  if (flag_r) PRINT(u.release);
  if (flag_v) PRINT(u.version);
  if (flag_m) PRINT(u.machine);

  printf("\n");
  return 0;
}
