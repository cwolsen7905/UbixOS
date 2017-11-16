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

 $Id: strings.h 88 2016-01-12 00:11:29Z reddawg $

*****************************************************************************************/

#ifndef _STRINGS_H
#define _STRINGS_H

#include <sys/cdefs.h>
#include <sys/_types.h>

#ifndef _SIZE_T_DECLARED
typedef __size_t        size_t;
#define _SIZE_T_DECLARED
#endif

int      bcmp(const void *, const void *, size_t);      /* LEGACY */
void     bcopy(const void *, void *, size_t);           /* LEGACY */
void     bzero(void *, size_t);                         /* LEGACY */
int      ffs(int);
char    *index(const char *, int);
char    *rindex(const char *, int);
int      strcasecmp(const char *, const char *);
int      strncasecmp(const char *, const char *, size_t);

#endif

/***
 $Log: strings.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:08  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:29  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:14:09  reddawg
 no message

 Revision 1.1  2004/07/28 18:45:39  reddawg
 movement of files

 END
 ***/

