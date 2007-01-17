/*****************************************************************************************
 Copyright (c) 2002-2005 The UbixOS Project
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

#include <sys/dma.h>
#include <sys/io.h>
#include <ubixos/types.h>

#define lowByte(x)  (x & 0x00FF)
#define highByte(x) ((x & 0xFF00) >> 8)
   
static uInt8 maskReg[8]   = { 0x0A, 0x0A, 0x0A, 0x0A, 0xD4, 0xD4, 0xD4, 0xD4 };
static uInt8 clearReg[8]  = { 0x0C, 0x0C, 0x0C, 0x0C, 0xD8, 0xD8, 0xD8, 0xD8 };
static uInt8 modeReg[8]   = { 0x0B, 0x0B, 0x0B, 0x0B, 0xD6, 0xD6, 0xD6, 0xD6 };
static uInt8 addrPort[8]  = { 0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC };
static uInt8 pagePort[8]  = { 0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A };
static uInt8 countPort[8] = { 0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE };

void dmaXfer(uInt8 channel,uInt32 address,uInt length,uInt8 read) {
  unsigned char page=0, mode=0;
  unsigned int offset = 0;
  if (read) {
    mode = 0x48 + channel;
    }
  else {
    mode = 0x44 + channel;
    }
  page = address >> 16;
  offset = address & 0xFFFF;
  length--;
  _dmaXfer(channel, page, offset, length, mode);
  }

void _dmaXfer(uInt8 dmaChannel,uInt8 page,uInt offset,uInt length,uInt8 mode) {
  //asm("cli");
  outportByte(maskReg[dmaChannel], 0x04 | dmaChannel);
  outportByte(clearReg[dmaChannel], 0x00);
  outportByte(modeReg[dmaChannel], mode);
  outportByte(addrPort[dmaChannel], lowByte(offset));
  outportByte(addrPort[dmaChannel], highByte(offset));
  outportByte(pagePort[dmaChannel], page);
  outportByte(countPort[dmaChannel], lowByte(length));
  outportByte(countPort[dmaChannel], highByte(length));
  outportByte(maskReg[dmaChannel], dmaChannel);
  //asm("sti");
  }

/***
 END
 ***/
