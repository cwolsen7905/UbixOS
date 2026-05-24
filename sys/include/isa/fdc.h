/*****************************************************************************************
 Copyright (c) 2002-2026 The UbixOS Project
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
   int8_t heads;
   int8_t tracks;
   int8_t spt;
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
void sendByte(int val);
int getByte();
bool fdcRw(int block,unsigned char *blockBuffer,bool read,unsigned long numSectors);
void block2Hts(int block,int *head,int *track,int *sector);
void motorOn(void);
void motorOff(void);
bool seek(int track);
bool waitFdc(bool sensei);
int getByte();
void sendByte(int val);
void recalibrate(void);
void reset(void);
bool writeBlock(int block,u_int8_t *blockBuffer, unsigned long numSectors);
bool readBlock(int block,u_int8_t *blockBuffer, unsigned long numSectors);
void fdcWrite(void *info,void *,u_int32_t startSector,u_int32_t sectorCount);
void fdcRead(void *info,void *,u_int32_t startSector,u_int32_t sectorCount);

#endif
