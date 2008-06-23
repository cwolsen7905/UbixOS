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
  char *a,*b;
  FILE *out;
  char buf[8192];
  int fd;
  int len = -1;
  printf("UbixOS Text Editor\n");
  printf("V1.0\n");

  //out = fopen("/test.txt","r");

  if (argc > 1)
    fd = open(argv[1]);
  else
    fd = open("/usr/include/stdio.h");

  printf("FD: %i\n",fd);

  //while (!feof(out)) {
  while (len != 0) {
    len = read(fd,&buf,8192);
    printf("[%i](%c%c%c%c)\n",len,buf[0],buf[1],buf[2],buf[3]);
    //printf("%c",fgetc(out));
    }

  //fclose(out);

  printf("argc: [%i]\n",argc);

  a = malloc(512);
  b = malloc(512);

  printf("[0x%X]\n",a);
  printf("[0x%X]\n",b);
  free(a);
  a = malloc(512);
  printf("[0x%X]\n",a);
  putchar('A');

  return(0);
  }

/***
 END
 ***/

