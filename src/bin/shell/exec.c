/**************************************************************************************
 Copyright (c) 2002 The UbixOS Project
 All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions, the following disclaimer and the list of authors.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions, the following disclaimer and the list of authors
in the documentation and/or other materials provided with the distribution. Neither the name of the UbixOS Project nor the names of its
contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id$

**************************************************************************************/

#include <sys/sys.h>
#include <sys/sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "shell.h"

int boo = 0x0;

void execProgram(inputBuffer *data) {
  char file[1024];
  char **argv = data->argv;
  int cPid = 0x0;

  printf("Executing App: %s\n",argv[1]);

  cPid = fork();

  if (!cPid) {
    sprintf(file,"%s%s",cwd,argv[1]);
    printf("Forked And Ready To Roll: %s\n",file);
    if (boo == 0)
      exec(file,data->argv,data->envp);
    else
      execn(file,&data->argv);

    printf("%s: Command Not Found.\n",argv[1]);
    exit(-1);
    }
  else {
    //if (data->bg == 0x0) {
      while (pidStatus(cPid) > 0)
        sched_yield();
      //}
    }
  }
