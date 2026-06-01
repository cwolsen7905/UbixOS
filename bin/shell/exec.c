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
#include <sys/sys.h>
#include <sys/sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "shell.h"

static char argv_init_buf[1024];

static char *envp_init_old[12] = {
    "HOST=MrOlsen.uBixOS.com",
    "TERM=vt100",
    "SHELL=/bin/sh",
    "HOME=/",
    "PWD=/",
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
    "USER=root",
    "LOGNAME=root",
    "GROUP=admin",
    "LD_DEBUG=all",
    "LD_LIBRARY_PATH=/lib:/usr/lib",
    NULL, }; //ENVP For Initial Proccess

static char *envp_init[11] = {
    "HOST=MrOlsen.uBixOS.com",
    "TERM=xterm",
    "SHELL=/bin/sh",
    "HOME=/",
    "PWD=/",
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
    "USER=root",
    "LOGNAME=root",
    "GROUP=admin",
    "LD_LIBRARY_PATH=/lib:/usr/lib",
    NULL, }; //ENVP For Initial Proccess

static const char *path_dirs[] = { "/bin", "/sbin", "/usr/bin", "/usr/sbin", NULL };

void execProgram(inputBuffer *data) {
  char file[1024];
  int cPid = 0x0;
  int i;

  cPid = fork();

  if (!cPid) {
    int rfd;

    /* stdout redirect: open target file and dup2 onto fd 1 */
    if (data->redirect_out != NULL) {
      int flags = O_WRONLY | O_CREAT | (data->redirect_append ? O_APPEND : O_TRUNC);
      rfd = open(data->redirect_out, flags, 0644);
      if (rfd < 0) {
        printf("shell: cannot open %s\n", data->redirect_out);
        exit(1);
      }
      dup2(rfd, 1);
      close(rfd);
    }

    char **argv = malloc(sizeof(char *) * (data->argc + 1));
    sprintf(argv_init_buf, "%s", data->argv[1]);
    argv[0] = argv_init_buf;
    for (i = 1; i < data->argc; i++)
      argv[i] = data->argv[i + 1];
    argv[data->argc] = NULL;

    if (data->argv[1][0] == '/' || strchr(data->argv[1], ':')) {
      /* absolute or mountpoint-qualified path (e.g. /bin/hello) */
      execve(data->argv[1], argv, envp_init_old);
    } else if (strchr(data->argv[1], '/')) {
      /* relative path with slash — relative to cwd */
      sprintf(file, "%s%s", cwd, data->argv[1]);
      execve(file, argv, envp_init_old);
    } else {
      /* bare name — search PATH */
      for (i = 0; path_dirs[i] != NULL; i++) {
        sprintf(file, "%s/%s", path_dirs[i], data->argv[1]);
        execve(file, argv, envp_init_old);
        /* execve only returns on failure; try next dir */
      }
    }
    free(argv);
    printf("%s: Command Not Found.\n", data->argv[1]);
    exit(-1);
  }
  else {
    if (data->bg == 0x0) {
      while (pidStatus(cPid) > 0)
        sched_yield();
    }
  }
}
