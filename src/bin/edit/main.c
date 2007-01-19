/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id$

*****************************************************************************************/

#include <stdio.h>
#include <stdlib.h>

int main(int argc,char **argv) {
  extern char **environ;
  FILE *out;
  int offset = 0xFF;
  printf("UbixOS Text Editor\n");
  printf("V1.0\n");
#ifdef DEBUG
  printf("ARGC: [%i]\n",argc);
  printf("ARGV[0]: [%s]\n",argv[0]);
  return(0x0);
#endif

  out = fopen("/test.txt","r");

  while (!feof(out)) {
    printf("%c",fgetc(out));
    }

  //printf("[%S]",getenv("TEST"));
  printf("\nENV TEST: [0x%X:0x%X]\n",&environ,offset);
  __findenv("TEST",&offset);
  printf("\nENV TEST: [0x%X:0x%X]\n",environ,offset);
  setenv("BLAH","WOOT",1);
  printf("[%s]",getenv("BLAH"));
  __findenv("TEST",&offset);

  return(0);
  }

/***
 END
 ***/

