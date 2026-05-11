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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFSIZE 4096

static int cat_fd(int fd, const char *name) {
  char buf[BUFSIZE];
  int nr;

  while ((nr = read(fd, buf, BUFSIZE)) > 0) {
    int off = 0;
    while (off < nr) {
      int nw = write(1, buf + off, nr - off);
      if (nw < 0) {
        fprintf(stderr, "cat: write error\n");
        return 1;
      }
      off += nw;
    }
  }
  if (nr < 0) {
    fprintf(stderr, "cat: %s: read error\n", name);
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int ret = 0;

  if (argc == 1) {
    ret = cat_fd(0, "stdin");
  } else {
    int i;
    for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-") == 0) {
        ret |= cat_fd(0, "stdin");
      } else {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
          fprintf(stderr, "cat: %s: cannot open\n", argv[i]);
          ret = 1;
          continue;
        }
        ret |= cat_fd(fd, argv[i]);
        close(fd);
      }
    }
  }
  return ret;
}
