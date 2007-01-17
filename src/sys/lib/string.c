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

#include <lib/string.h>

int strcmp(const char *str1,const char *str2) {
  while ((*str1 == *str2) && (*str1 != 0x0) && (*str2 != 0x0)) {
    str1++;
    str2++;
    }
  if (*str1 == *str2) {
    return(0);
    }
  else if (*str1 > *str2) {
    return(1);
    }
  else {
    return(-1);
    }
  }

int strncmp(const char * a, const char * b, size_t c) {
  int i = 0;
  while (i < c) {
    if ((a[i] != b[i]) || (a[i] == '\0') || (b[i] == '\0'))
      return a[i] - b[i];
    i++;
    }
  return 0;
  }



void *memcpyold(const void *dst, const void * src, size_t length) {
  //size_t x = length >> 2;
  //size_t y = length;// & 0xf;
  size_t i;
/*
  for (i = 0; i < x; i++) {
    ((unsigned long *)dst)[i] = ((unsigned long *)src)[i];
    }
*/
/*
  for (i = 0; i < y; i++) {
    ((char *) dst)[length-y+i] = ((char *) src)[length-y+i];
    }
*/
  for (i = 0x0;i < length;i++)
    ((char *)dst)[i] = ((char *)src)[i];

  return((void *)dst);
  }

int memcmp(const void * dst, const void * src, size_t length)
{
    size_t x = length >> 2;
    size_t y = length & 0xf;
    size_t i;

    for (i = 0; i < x; i++)
    {
	if (((unsigned long *)dst)[i] > ((unsigned long *)src)[i])
	    return 1;
	if (((unsigned long *)dst)[i] < ((unsigned long *)src)[i])
	    return -1;
    }

    for (i = 0; i < y; i++)
    {
	if (((char *) dst)[length-y+i] > ((char *) src)[length-y+i])
	    return 1;
	if (((char *) dst)[length-y+i] < ((char *) src)[length-y+i])
	    return -1;
    }

    return 0;
}

void strncpy(char * dest, const char * src, size_t size)
{
    if (size == 0)
	return;
    do
    {
        *dest = *src;
	dest++; src++;
        size--;
    }
    while(('\0' != *(src-1)) && (size));
}

char *strstr(const char *s,char *find) {
  char c, sc;
  size_t len;

  if ((c = *find++) != 0) {
    len = strlen(find);
    do {
      do {
        if ((sc = *s++) == 0)
          return (NULL);
        } while (sc != c);
      } while (strncmp(s, find, len) != 0);
    s--;
    }
  return ((char *)s);
  }


/***
 $Log$
 Revision 1.3  2006/12/12 14:09:18  reddawg
 Changes

 Revision 1.2  2006/12/05 14:10:21  reddawg
 Workign Distro

 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:13  reddawg
 no message

 Revision 1.6  2004/07/28 15:05:43  reddawg
 Major:
   Pages now have strict security enforcement.
   Many null dereferences have been resolved.
   When apps loaded permissions set for pages rw and ro

 Revision 1.5  2004/07/20 18:42:41  flameshadow
 add: strcpy()
 chg: modified dirCache.c to use strcpy()

 Revision 1.4  2004/07/05 23:06:32  reddawg
 Fixens

 Revision 1.3  2004/06/28 23:12:58  reddawg
 file format now container:/path/to/file

 Revision 1.2  2004/05/19 14:40:58  reddawg
 Cleaned up some warning from leaving out typedefs

 Revision 1.1.1.1  2004/04/15 12:07:11  reddawg
 UbixOS v1.0

 Revision 1.5  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/

