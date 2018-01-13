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

 $Id: sem.c 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#include <ubixos/sem.h>
#include <ubixos/time.h>

int sem_close(semID_t id) {
  return(0);
}

int sem_post(semID_t id) {
  return(0);
}

int sem_wait(semID_t id) {
  return(0);
}

int sem_trywait(semID_t id) {
  return(0);
}

int sem_timedwait(semID_t id, const struct timespec) {
  return(0);
}

int sem_init(semID_t *id, unsigned int value) {
  return(0);
}

int sem_open(semID_t *id, const char *name, int oflag, mode_t mode, unsigned int value) {
  return(0);
}

int sem_unlink(const char *name) {
  return(0);
}

int sem_getvalue(semID_t id, int *val) {
  return(0);
}

int sem_destroy(semID_t id) {
  return(0);
}

