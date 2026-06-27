/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 4096

static int usage(void) {
    fprintf(stderr, "usage: nc <host> <port>\n");
    return 1;
}

static unsigned long parse_ip(const char *s) {
    unsigned a, b, c, d;
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return 0;
    return ((unsigned long)a << 24) | (b << 16) | (c << 8) | d;
}

int main(int argc, char *argv[]) {
    int fd, port;
    struct sockaddr_in sin;
    char buf[BUFSIZE];
    int nr, nw;

    if (argc != 3)
        return usage();

    port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "nc: invalid port: %s\n", argv[2]);
        return 1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("nc: socket");
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port   = htons((unsigned short)port);
    sin.sin_addr.s_addr = htonl(parse_ip(argv[1]));

    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("nc: connect");
        close(fd);
        return 1;
    }

    /* relay: stdin → socket, socket → stdout */
    while (1) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(0, &rset);
        FD_SET(fd, &rset);

        if (select(fd + 1, &rset, NULL, NULL, NULL) < 0)
            break;

        if (FD_ISSET(0, &rset)) {
            nr = read(0, buf, sizeof(buf));
            if (nr <= 0) break;
            nw = write(fd, buf, nr);
            if (nw < 0) break;
        }

        if (FD_ISSET(fd, &rset)) {
            nr = read(fd, buf, sizeof(buf));
            if (nr <= 0) break;
            nw = write(1, buf, nr);
            if (nw < 0) break;
        }
    }

    close(fd);
    return 0;
}
