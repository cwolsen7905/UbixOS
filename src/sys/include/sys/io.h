/*****************************************************************************************
 Copyright (c) 2002-2004, 2016 The UbixOS Project
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

 $Id: io.h 88 2016-01-12 00:11:29Z reddawg $

 *****************************************************************************************/

#ifndef _IO_H_
#define _IO_H_

unsigned char inportByte( unsigned int );
unsigned short inportWord( unsigned int );
unsigned long inportDWord( unsigned int );

void outportByte( unsigned int, unsigned char );
void outportByteP( unsigned int port, unsigned char value );
void outportWord( unsigned int, unsigned short );
void outportDWord( unsigned int port, unsigned long value );

#endif

/***
 $Log: io.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:15  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:52  reddawg
 no message

 Revision 1.3  2004/07/22 20:14:34  reddawg
 still working here

 Revision 1.2  2004/05/21 15:12:17  reddawg
 Cleaned up


 END
 ***/
