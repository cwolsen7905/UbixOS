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
#include <unistd.h>
#include "shell.h"

static char *argv_init[2] = {
    "/bin/shell",
    NULL, }; // ARGV For Initial Proccess

static char *envp_init_old[12] = {
    "HOST=MrOlsen.uBixOS.com",
    "TERM=xterm",
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

void execProgram(inputBuffer *data) {
  char file[1024];
  int cPid = 0x0;

  cPid = fork();

  if (!cPid) {
    sprintf(file, "%s%s", cwd, data->argv[1]);
    execve(file, argv_init, envp_init_old);
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
