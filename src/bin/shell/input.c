/**************************************************************************************
 Copyright (c) 2002,2008 The UbixOS Project
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"

void parseInput(inputBuffer *buffer,char *data) {
  int                 i       = 0x0;
  char               *tmpDat  = data;
  char               *arg     = 0x0;
  char              **argv    = 0x0;
  struct argsStruct  *tmpArgs = 0x0;

  while (data[0] == ' ') {
    data++;
    }

  if (*data == '\0')
    return;

  buffer->args = (struct argsStruct *)malloc(sizeof(struct argsStruct));

  tmpArgs = buffer->args;

  while(tmpDat != 0x0) {
    arg = strtok(tmpDat," ");
    tmpDat = strtok(NULL,"\n");

    //printf("sh[%s:%s]",arg,data);
    if (arg[0] == '&') {
      buffer->bg = 0x1;
      }
    else {
      buffer->argc++;
      tmpArgs->arg = arg;
      if (tmpDat != 0x0) {
        tmpArgs->next = (struct argsStruct *)malloc(sizeof(struct argsStruct));
        }
      tmpArgs = tmpArgs->next;
      }
    }

  /* Alloc memory for argv[] */
  buffer->argv = (char *)malloc(sizeof(char *) * (buffer->argc + 2));
  buffer->envp = (char *)malloc(sizeof(char *));

  buffer->envp[0] = 0x0;

  tmpArgs = buffer->args;
  argv = buffer->argv;

  for (i=0x1;i <= buffer->argc;i++) {
    argv[i] = tmpArgs->arg;
    tmpArgs = tmpArgs->next;
    }
  argv[0] = (char *)buffer->argc;
  }

void freeArgs(inputBuffer *ptr) {
   free(ptr->args);
   free(ptr->argv);
   free(ptr->envp);
  //free(tmpArgs->argv);
  }
