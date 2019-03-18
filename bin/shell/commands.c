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
#include <unistd.h>
#include <sys/sys.h>
#include <sys/sched.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/mpi.h>

#include "shell.h"

int commands(inputBuffer *data) {
  int cPid = 0x0,i = 0x0,x = 0x0;
  char **argv = data->argv;

  mpi_message_t cmdMsg;

  if (data == NULL) return 1;
  if (data->args->arg == NULL) return 1;
  if (*data->args->arg == '\0') return 1;

  if (0 == memcmp(data->args->arg, "uname", 5)) {
    printf("UbixOS v0.87  " __DATE__" " __TIME__ " \n");
    return(1);
    }
  else if (0 == memcmp(data->args->arg, "exit", 4)) {
    exit(1);
    }
  else if (0 == memcmp(data->args->arg, "mypid", 5)) {
    printf("My Pid: [%i]\n",getpid());
    return(1);
    }
  else if (memcmp(data->args->arg,"stress", 6) == 0) {
    //for (i=0x0;i<100;i++) {
    while (1) {
 /*     printf("Starting Clock\n"); */
      cPid = fork();
      if (cPid == 0x0) {
        execve("clock",0x0,0x0);
        exit(0x1);
        } else {
        printf("Childs Pid: [%i]\n",cPid);
        while (pidStatus(cPid) > 0)
          sched_yield();
        }
      cPid = fork();
      if (cPid == 0x0) {
        execve("ls", 0x0, 0x0);
        exit(0x1);
      } else {
        printf("Childs Pid: [%i]\n",cPid);
        while (pidStatus(cPid) > 0)
          sched_yield();
      }
    }
  }
  else if (memcmp(data->args->arg,"test2", 5) == 0) {
    for (i=0x0;i<500;i++) {
      cPid = fork();
      if (cPid == 0x0) {
        printf("Pid: [%i:%i]\n",cPid,i);
        execve("clock",0x0,0x0);
        exit(0x1);
      }
      else {
        printf("Childs Pid: [%i]\n",cPid);
      }
    }
  }
  else if (memcmp(data->args->arg,"test3", 5) == 0) {
    for (i=0x0;i<50;i++) {
      cPid = fork();
      if (cPid == 0x0) {
        printf("Pid: [%i:%i]\n",cPid,i);
        for (x=0x0;x<100;x++) sched_yield();
        printf("Exiting!!!\n");
        exit(0x1);
        }
      else {
        printf("Childs Pid: [%i]\n",cPid);
        }
      }
    }
  else if (memcmp(data->args->arg,"echo",4) == 0) {
    for (i=1;i<data->argc;i++) {
      printf("%s ",argv[i]);
      }
    printf("\n");
    }
  else if (memcmp(data->args->arg,"about",5) == 0) {
    printf("UbixOS Shell v0.99 (C) 2002\n");
    printf("Base Command Line Interface\n");
    }
  else if (memcmp(data->args->arg,"cd",2) == 0) {
    if (argv[1]) {
      chdir(argv[1]);
      getcwd(cwd,1024);
      }
    }
  else if (memcmp(data->args->arg,"unlink",6) == 0) {
    if (argv[1]) {
      unlink(argv[1]);
      }
    }
  else if (memcmp(data->args->arg,"msg",3) == 0x0) {
    printf("Posting Message: mbox(%s), header: 0x%X, data: %s\n", argv[2], atoi(argv[3]), argv[4]);
    cmdMsg.header = atoi(argv[3]);
    sprintf(cmdMsg.data,argv[4]);
    mpi_postMessage(argv[2],0x1,&cmdMsg);
    }
  else if (memcmp(data->args->arg,"mkdir",5) == 0x0) {
    if (argv[1]) {
      mkdir(argv[1],0xEAA);
      }
    }
  else if (memcmp(data->args->arg,"id",2) == 0x0) {
    printf("UID: %i, GID: %i\n",getuid(),getgid());
    }
  else if (argv[1] != 0x0 && !strcmp(argv[1],"reboot")) {
    cmdMsg.header  = 1000;
    cmdMsg.data[0] = '\0';
    mpi_postMessage("system",0x1,&cmdMsg);
    }
  else {
    return(0);
    }
  return(1);
  }
