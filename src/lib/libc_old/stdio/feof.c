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

#include <stdio.h>
#include <stdlib.h>

typedef struct _feof
{
	FILE *fp;
	int status;
} feof_t;



int feof(FILE *fp)
{
	feof_t *f;

	f = malloc(sizeof *f);
	if(f == NULL)
	{
		printf("feof.c: out of memory");
		return -1;
	}

	if(fp == NULL)
	{
		return -1;
	}
	f->fp = fp;

	asm volatile(
	"int %0\n"
	: : "i" (0x80),"a" (9),"b" (f)
	);

	if(f->status == 0)
	{
		free(f);
		return 0;
	}
	else
	{
		free(f);
		return 1;
	}
}

/***
 $Log$
 Revision 1.3  2005/08/05 10:09:24  fsdfs

 fixed again. freebsd needs copy and paste

 Revision 1.2  2005/08/05 10:02:51  fsdfs
 fixed

 Revision 1.1  2005/08/05 09:48:10  fsdfs
 feof() syscall implemented

 Revision 1.4  2004/07/28 15:05:43  reddawg
 Major:
   Pages now have strict security enforcement.
   Many null dereferences have been resolved.
   When apps loaded permissions set for pages rw and ro

 Revision 1.3  2004/06/17 15:16:02  reddawg
 Made asm statements volatile

 Revision 1.2  2004/06/16 19:38:26  reddawg
 Updated CW Cleaned Out Dead Code

 END
 ***/
