/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <lib/string.h>

u_char const bcd2bin_data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 0, 0, 0, 0, 0, 0, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 0, 0, 0, 0, 0, 0, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 0, 0, 0, 0, 0, 0, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 0, 0, 0, 0, 0, 0, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 0, 0, 0, 0, 0, 0, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 0, 0, 0, 0, 0, 0, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 0, 0, 0, 0, 0, 0, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 0, 0, 0, 0, 0, 0, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99 };

u_char const bin2bcd_data[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99 };

/* This is actually used with radix [2..36] */
char const hex2ascii_data[] = "0123456789abcdefghijklmnopqrstuvwxyz";

int strcmp(const char *str1, const char *str2) {
  while ((*str1 == *str2) && (*str1 != 0x0) && (*str2 != 0x0)) {
    str1++;
    str2++;
  }
  if (*str1 == *str2) {
    return (0);
  }
  else if (*str1 > *str2) {
    return (1);
  }
  else {
    return (-1);
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
  for (i = 0x0; i < length; i++)
    ((char *) dst)[i] = ((char *) src)[i];

  return ((void *) dst);
}

int memcmp(const void * dst, const void * src, size_t length) {
  size_t x = length >> 2;
  size_t y = length & 0xf;
  size_t i;

  for (i = 0; i < x; i++) {
    if (((unsigned long *) dst)[i] > ((unsigned long *) src)[i])
      return 1;
    if (((unsigned long *) dst)[i] < ((unsigned long *) src)[i])
      return -1;
  }

  for (i = 0; i < y; i++) {
    if (((char *) dst)[length - y + i] > ((char *) src)[length - y + i])
      return 1;
    if (((char *) dst)[length - y + i] < ((char *) src)[length - y + i])
      return -1;
  }

  return 0;
}

void strncpy(char * dest, const char * src, size_t size) {
  if (size == 0)
    return;
  do {
    *dest = *src;
    dest++;
    src++;
    size--;
  } while (('\0' != *(src - 1)) && (size));
}

char *strstr(const char *s, char *find) {
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
  return ((char *) s);
}

/***
 $Log: string.c,v $
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

