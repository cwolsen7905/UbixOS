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

#include <mpi/mpi.h>

void sysMpiCreateMbox(uInt32 *status,char *name) {
  if (status && name)
    *status = mpi_createMbox(name); 
  return;
  }

void sysMpiDestroyMbox(uInt32 *status,char *name) {
  if (status && name)
    *status = mpi_destroyMbox(name);
  return;
  }

void sysMpiPostMessage(char *name,uInt32 *type,mpi_message_t *data) {
  if (type && name && data)
    *type = mpi_postMessage(name,*type,data);
  return;
  }

void sysMpiFetchMessage(char *name,mpi_message_t *msg,uInt32 *status) {
  if (status && name && msg)
    *status = mpi_fetchMessage(name,msg);
  return;
  }

void sysMpiSpam(uInt32 type,void *data,uInt32 *status) {
  if (status && data)
    *status = mpi_spam(type,data);
  return;
  }

/***
 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:14  reddawg
 no message

 Revision 1.12  2004/08/14 11:23:02  reddawg
 Changes

 Revision 1.11  2004/07/28 17:07:25  reddawg
 MPI: moved the syscalls

 END
 ***/

