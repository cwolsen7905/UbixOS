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

 $Id: fdc.h 79 2016-01-11 16:21:27Z reddawg $

*****************************************************************************************/

#ifndef _FDC_H
#define _FDC_H

#include <sys/types.h>

typedef struct DrvGeom {
   Int8 heads;
   Int8 tracks;
   Int8 spt;
} drvGeom;


#define fdcMsr  (0x3f4)
#define fdcData (0x3f5)
#define fdcDir  (0x3f7)
#define fdcCcr  (0x3f7)
#define fdcDor  (0x3f2)
#define fdcDrs  (0x3f4)

#define cmdWrite   (0xc5)
#define cmdRead    (0xe6)
#define cmdSeek    (0x0f)
#define cmdSensei  (0x08)
#define cmdRecal   (0x07)
#define cmdSpecify (0x03)

#define dg144Heads  2     /* heads per drive (1.44M) */
#define dg144Tracks 80
#define dg144Spt    18
#define dg144Gap3rw 0x1b
#define dg168Gap3rw 0x1c



int fdc_init();
void floppyIsr();
void floppyIsrhndlr();
void sendByte(int Int8);
int getByte();
bool fdcRw(int block,unsigned char *blockBuffer,bool read,unsigned long numSectors);
void block2Hts(int block,int *head,int *track,int *sector);
void motorOn(void);
void motorOff(void);
bool seek(int track);
bool waitFdc(bool sensei);
int getByte();
void sendByte(int Int8);
void recalibrate(void);
void reset(void);
bool writeBlock(int block,Int8 *blockBuffer, unsigned long numSectors);
bool readBlock(int block,Int8 *blockBuffer, unsigned long numSectors);
void fdcWrite(void *info,void *,uInt32 startSector,uInt32 sectorCount);
void fdcRead(void *info,void *,uInt32 startSector,uInt32 sectorCount);

#endif

/***
 $Log: fdc.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:39  reddawg
 no message

 Revision 1.6  2004/07/17 02:38:31  reddawg
 Fixed a few problems

 Revision 1.5  2004/07/14 12:42:46  reddawg
 fdc: fdcInit to fdc_init
 Changed Startup Routines

 Revision 1.4  2004/05/21 14:57:16  reddawg
 Cleaned up

 END
 ***/
