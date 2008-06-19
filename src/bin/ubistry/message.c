/*****************************************************************************************
 Copyright (c) 2002-2004,2008 The UbixOS Project
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
#include <unistd.h>
#include <string.h>
#include <sys/sys.h>
#include <sys/mpi.h>
#include "./include/ubistry.h"

int ubistryInitMbox(char *name) {

  if (mpi_createMbox(name) != 0x0) {
    printf("Error: Error Creating Mail Box: [%s]\n",name);
    return(-1);
    }

  return(0x0);
  }

void ubistryProcessMessages() {
  mpi_message_t       msg;
  struct ubistryKey *tmpKey = 0x0;
  char *key,*value;

  mfmStart:
  if (mpi_fetchMessage("ubistry",&msg) == 0x0) {
    switch (msg.header) {
      case 50:
        tmpKey = ubistryFindKey(msg.data);
        if (tmpKey == 0x0)
          printf("ubistry: Key (%s) Not Found\n",msg.data);
        else
          printf("ubistry: Key (%s) Found Has Value (%s)\n",tmpKey->name,tmpKey->value);
        break;
      case 51:
         key = strtok(msg.data,",");
         value = strtok(NULL,"\n");
         printf("ubistry: Adding key (%s) with value (%s)\n",key,value); 
         ubistryAddKey(key,value);
         break;
      case 666:
        mpi_destroyMbox("ubistry");
        if (fork() == 0x0) {
          printf("ubistry: Restarting\n");
          exec("ubistry@sys",0x0,0x0);
          }
        else {
          exit(0x0);
          }
        break;
      default:
        printf("ubistry: Command (%i) With Data (%s) Not Valid\n",msg.header,msg.data);
        break;
      }
    goto mfmStart;
    }
  return;
  }

/***
 $Log$
 Revision 1.1.1.1  2007/01/17 03:30:20  reddawg
 UbixOS

 Revision 1.2  2006/12/12 14:09:17  reddawg
 Changes

 Revision 1.1.1.1  2006/06/01 12:46:09  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:28  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:14:04  reddawg
 no message

 Revision 1.2  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.1  2004/05/26 15:41:20  reddawg
 ubistry: added command 666 which will restart the registry server also added
          command 51 to add a key format key,value

 END
 ***/
