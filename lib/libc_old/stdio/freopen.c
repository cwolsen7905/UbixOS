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

 $Id: freopen.c 89 2016-01-12 00:20:40Z reddawg $

*****************************************************************************************/

/* Pieces of code from fopen, and fclose, modified by Tyrell Atterberry */



#include <stdio.h>

#include <stdlib.h>



FILE *freopen(const char *file,const char *mode, FILE *fp) {
  
  //FILE *fp = malloc(sizeof(FILE));

  int status = 0x0;


  if(!file || !mode || !fp) return NULL;



  asm volatile(

    "int %0\n"

    : : "i" (0x80),"a" (10),"b" (fp),"c" (&status)

    );



  fp->fd = -1;


  asm volatile(

    "int %0\n"

    : : "i" (0x80),"a" (8),"b" (file),"c" (mode),"d" (fp)

    );

  return ((FILE *)fp);

  }


/***
 $Log: freopen.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:10  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:22:55  reddawg
 no message

 Revision 1.2  2004/06/17 15:16:02  reddawg
 Made asm statements volatile

 Revision 1.1  2004/06/16 20:28:42  reddawg
 New Code

 END
 ***/
