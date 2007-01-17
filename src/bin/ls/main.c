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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define permRead    0x8
#define permWrite   0x4
#define permExecute 0x2
#define permHidden  0x1


//UbixFS Directory Entry
struct directoryEntry {
  u_int32_t  startCluster;   //Starting Cluster Of File
  u_int32_t  size;           //Size Of File
  u_int32_t  creationDate;  //Date Created
  u_int32_t  lastModified;  //Date Last Modified
  u_int32_t  uid;           //UID Of Owner
  u_int32_t  gid;           //GID Of Owner
  u_int16_t attributes;    //Files Attributes
  u_int16_t permissions;   //Files Permissions
  char   fileName[256]; //File Name
  };

#define typeFile      1
#define typeContainer 2
#define typeDirectory 4
#define typeDeleted   8

int main(int argc,char **argv) {
  int i = 0x0,x = 0x0,tmpPerms = 0x0;
  char *pwd       = 0x0;
  char *permsData = 0x0;
  FILE *fd;
  struct directoryEntry *dirEntry = 0x0;

  pwd       = (char *)malloc(256);
  permsData = (char *)malloc(13);

  if (argv == 0x0) {
      getcwd(pwd,256);
    if ((fd = fopen(pwd,"rb")) == 0x0) {
      printf("Error: Reading Directory\n");
      exit(1);
      }
    }
  else if (argv[1] == 0x0) {
    getcwd(pwd,256);
    if ((fd = fopen(pwd,"rb")) == 0x0) {
      printf("Error: Reading Directory\n");
      exit(1);
      }
    }
  else {
    fd = fopen(argv[1],"rb");
    if (fd->fd == 0x0) {
      printf("Error: Reading Directory\n");
      exit(1);
      }    
    }
  dirEntry = (struct directoryEntry *)malloc(fd->size);
  fread(dirEntry,fd->size,1,fd);
  pwd[0] = '/';
  for (i=0;i<(fd->size/sizeof(struct directoryEntry));i++) {
    if ((dirEntry[i].fileName[0] > 0) && (dirEntry[i].fileName[0] != '/')) {
      for (x=0;x<12;x++) {
        permsData[x] = '-';
        }
      if ((dirEntry[i].attributes & typeDeleted) == typeDeleted) {
        permsData[0] = 'd';
        goto next;
        }
      else if ((dirEntry[i].attributes & typeFile) == typeFile) {
        permsData[0] = 'F';
        }
      else if ((dirEntry[i].attributes & typeDirectory) == typeDirectory) {
        permsData[0] = 'D';
        }
      else if ((dirEntry[i].attributes & typeContainer) == typeContainer) {
        permsData[0] = '@';
        }
      else {
        permsData[0] = 'U';
        }
      tmpPerms = ((dirEntry[i].permissions & 0xF00) >> 8);
      if ((tmpPerms & permRead) == permRead) permsData[1] = 'R';
      if ((tmpPerms & permWrite) == permWrite) permsData[2] = 'W';
      if ((tmpPerms & permExecute) == permExecute) permsData[3] = 'E';
      if ((tmpPerms & permHidden) == permHidden) permsData[4] = 'H';
      tmpPerms = ((dirEntry[i].permissions & 0x0F0) >> 4);
      if ((tmpPerms & permRead) == permRead) permsData[5] = 'R';
      if ((tmpPerms & permWrite) == permWrite) permsData[6] = 'W';
      if ((tmpPerms & permExecute) == permExecute) permsData[7] = 'E';
      if ((tmpPerms & permHidden) == permHidden) permsData[8] = 'H';
      tmpPerms = ((dirEntry[i].permissions & 0x00F) >> 0);
      if ((tmpPerms & permRead) == permRead) permsData[9] = 'R';
      if ((tmpPerms & permWrite) == permWrite) permsData[10] = 'W';
      if ((tmpPerms & permExecute) == permExecute) permsData[11] = 'E';
      if ((tmpPerms & permHidden) == permHidden) permsData[12] = 'H';       
      printf("%s %i %i %i %s\n",permsData,(int)dirEntry[i].uid,(int)dirEntry[i].gid,(int)dirEntry[i].size,dirEntry[i].fileName);
      next:
      asm("nop");
      }
    }
  if (fclose(fd) != 0x0) {
    printf("Error Closing Directory\n");
    }
  return(0);
  }
