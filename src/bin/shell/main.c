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
#include <stdlib.h>
#include <unistd.h>
#include <sys/sys.h>
#include <string.h>
#include <api/ubix.h>
#include "shell.h"

char *machine = 0x0;
char *cwd     = 0x0;
char *cwc     = 0x0;

int main() {

  char *buffer = (char *)malloc(512);
  inputBuffer *inBuf = (inputBuffer *)malloc(sizeof(inputBuffer));

  machine = (char *)malloc(32);
  cwd     = (char *)malloc(1024);

  sprintf(machine,"uBixCube");
  getcwd(cwd,1024);

  printf("[0x%X]\n",ubix_test());

  while (1) {
   aGain:
    printf("%s@%s> ",machine,cwd);
    gets((char *)buffer);

    if (buffer[0] == 0x0)
      goto aGain;

    inBuf->argc = 0x0;
    inBuf->args = 0x0;
    inBuf->bg   = 0x0;

    parseInput(inBuf,buffer);

    if (inBuf->args->arg != 0x0) {
      if (!commands(inBuf))
        execProgram(inBuf);
      }

    freeArgs(inBuf);
    }
  return(0x0);
  }
