/*****************************************************************************************
 Copyright (c) 2002-2004,2007 The UbixOS Project
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
#include <sys/sys.h>
#include <sys/sched.h>
#include <sys/mpi.h>

int main(int argc,char **argv) {
  int i=0x0;
  mpi_message_t myMsg;

  /* Create a mailbox for this task */
  if (mpi_createMbox("init") != 0x0) {
    printf("Error: Failed to creating mail box: init\n");
    exit(0x1);
    }

  /* Make sure we have superuser permissions if not exit */
  if ((getuid() != 0x0) && (getgid() != 0x0)) {
    printf("Error: This program must be run by root.\n");
    exit(0x1);
    }

  printf("Initializing UbixOS\n");

#if 0
  /* Start TTYD */
  i = fork();

  if (0x0 == i) {
    exec("sys:/ttyd",0x0);
    printf("Error: Could not start TTYD\n");
    exit(0x0);
    }
    
  while (pidStatus(i) > 0x0)
    sched_yield();
#endif
    
#if 0
  i = fork();
  if (0x0 == i) {
    printf("Starting Ubix Registry (ubistry)\n");
    exec("sys:/ubistry",0x0);
    printf("Error: Error Starting ubistry\n");
    exit(0x0);
    }

  while (pidStatus(i) > 0x0) {
    sched_yield();
    } 
#endif

  startup:
  i = fork();
  printf("We Forked\n");
  if (0 == i) {
    printf("Starting Login Daemon.\n");
    exec("sys:/bin/login",0x0);
    printf("Error Starting System\n");
    exit(0x0);
    }

  while (pidStatus(i) > 0x0) {
    fetchAgain:
    if (mpi_fetchMessage("init",&myMsg) == 0x0) {
      switch (myMsg.header) {
        case 10:
          printf("Exec: (%s)\n",myMsg.data);
          break;
        default:
          printf("MailBox: init Received Message %i:%s\n",myMsg.header,myMsg.data);
          break;
        }
      goto fetchAgain;
      }
    sched_yield();
    }
    
  printf("Error: login exited!");

  goto startup;

  return(0x0);
  }

/***
 END
 ***/
