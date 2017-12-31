/*****************************************************************************************
 Copyright (c) 2002 The UbixOS Project
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

 $Id: strtok.c 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#include <lib/string.h>
#include <sys/types.h>

char *strtok_r(char *s, const char *delim, char **last) {
  char *spanp;
  int c, sc;
  char *tok;

  if ((s == NULL) && ((s = *last) == NULL)) {
    return(NULL);
    }

cont:
  c = *s++;
  for (spanp = (char *)delim; (sc = *spanp++) != 0; ) {
    if (c == sc) {
      goto cont;
      }
    }
  if (c == 0) {
    *last = NULL;
    return(NULL);
    }
  tok = s - 1;

  for (;;) {
    c = *s++;
    spanp = (char *)delim;
    do {
      if ((sc = *spanp++) == c) {
        if (c == 0) {
          s = NULL;
          }
        else {
          char *w = s - 1;
          *w = '\0';
          }
        *last = s;
        return(tok);
        }
      } while (sc != 0);
    }
  }

char *strtok(char *s, const char *delim) {
  static char *last;
  return (strtok_r(s, delim, &last));
  }

/***
 $Log: strtok.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:13  reddawg
 no message

 Revision 1.2  2004/05/19 03:46:32  reddawg
 A Few Quick Hacks To Make Things Work

 Revision 1.1.1.1  2004/04/15 12:07:11  reddawg
 UbixOS v1.0

 Revision 1.2  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/


