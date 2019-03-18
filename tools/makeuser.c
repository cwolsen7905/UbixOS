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

#include <stdio.h>
#include <stdlib.h>

struct passwd {
  char username[32];
  char passwd[32];
  int uid;
  int gid;
  char shell[128];
  char realname[256];
  char path[256];
};

int main(int argc, char **argv) {
  int i = 0x0;
  struct passwd *password = 0x0;
  FILE *out;
  password = (struct passwd *) malloc(4096);
  out = fopen("./userdb", "wbb");
  sprintf(password[0].username, "root");
  sprintf(password[0].passwd, "user");
  sprintf(password[0].shell, "sys:/bin/shell");
  sprintf(password[0].realname, "Root User");
  sprintf(password[0].path, "root");
  password[0].uid = 0;
  password[0].gid = 0;
  sprintf(password[1].username, "guest");
  sprintf(password[1].passwd, "user");
  sprintf(password[1].shell, "sys:/bin/shell");
  sprintf(password[1].realname, "Guest User");
  sprintf(password[1].path, "guest");
  password[1].uid = 1;
  password[1].gid = 1;
  sprintf(password[2].username, "reddawg");
  sprintf(password[2].passwd, "temp123");
  sprintf(password[2].shell, "sys:/bin/sh");
  sprintf(password[2].realname, "Christopher W. Olsen");
  sprintf(password[2].path, "reddawg");
  password[2].uid = 1000;
  password[2].gid = 0;
  fwrite(password, 4096, 1, out);
  fclose(out);
  for (i = 0; i < 3; i++) {
    printf("User:  [%s]\n", password[i].username);
    printf("Pass:  [%s]\n", password[i].passwd);
    printf("UID:   [%i]\n", password[i].uid);
    printf("GID:   [%i]\n", password[i].gid);
    printf("Shell: [%s]\n", password[i].shell);
    printf("Name:  [%s]\n", password[i].realname);
    printf("Path:  [%s]\n", password[i].path);
  }
  return (0);
}
