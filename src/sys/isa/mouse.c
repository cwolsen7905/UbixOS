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

 $Id: mouse.c 79 2016-01-11 16:21:27Z reddawg $

 *****************************************************************************************/

#include <isa/mouse.h>
#include <isa/8259.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <sys/io.h>
#include <lib/kprintf.h>

static uInt8 kbdRead() {
  unsigned long Timeout;
  uInt8 Stat, Data;

  for ( Timeout = 50000L; Timeout != 0 ; Timeout-- ) {
    Stat = inportByte( 0x64 );

    /* loop until 8042 output buffer full */
    if ( ( Stat & 0x01 ) != 0 ) {
      Data = inportByte( 0x60 );

      /* loop if parity error or receive timeout */
      if ( ( Stat & 0xC0 ) == 0 )
        return Data;
    }
  }
  return -1;
}

static void kbdWrite( uInt16 port, uInt8 data ) {
  uInt32 timeout;
  uInt8 stat;

  for ( timeout = 500000L; timeout != 0 ; timeout-- ) {
    stat = inportByte( 0x64 );

    if ( ( stat & 0x02 ) == 0 )
      break;
  }

  if ( timeout != 0 )
    outportByte( port, data );
}

static uInt8 kbdWriteRead( uInt16 port, uInt8 data, const char* expect ) {
  int RetVal;

  kbdWrite( port, data );
  for ( ; *expect ; expect++ ) {
    RetVal = kbdRead();
    if ( ( uInt8 ) * expect != RetVal ) {
      return RetVal;
    }
  }

  return 0;
}

int mouseInit() {
  static uInt8 s1[] = { 0xF3, 0xC8, 0xF3, 0x64, 0xF3, 0x50, 0 };
  static uInt8 s2[] = { 0xF6, 0xE6, 0xF4, 0xF3, 0x64, 0xE8, 0x03, 0 };
  const uInt8* ch;
  Int8 cmd = 0x0;

  kbdWrite( 0x64, 0xA8 );
  for ( ch = s1; *ch ; ch++ ) {
    kbdWrite( 0x64, 0xD4 );
    kbdWriteRead( 0x60, *ch, "\xFA" );
  }
  for ( ch = s2; *ch ; ch++ ) {
    kbdWrite( 0x64, 0xD4 );
    kbdWriteRead( 0x60, *ch, "\xFA" );
  }
  kbdWrite( 0x64, 0xD4 );
  if ( kbdWriteRead( 0x60, 0xF2, "\xFA" ) != 0x0 ) {
    kprintf( "Error With Mouse\n" );
  }
  cmd = kbdRead();
  kprintf( "CMD: [0x%X]\n", cmd );
  kbdWrite( 0x64, 0xD4 );
  kbdWriteRead( 0x60, 0xF4, "\xFA" );

  setVector( &mouseISR, mVec + 12, dPresent + dInt + dDpl3 );

  outportByte( mPic, eoi );
  outportByte( sPic, eoi );
  irqEnable( 12 );
  outportByte( mPic, eoi );
  outportByte( sPic, eoi );

  kprintf( "psm0 - Address: [0x%X]\n", &mouseISR );

  /* Return so we know everything went well */
  return ( 0x0 );
}

asm(
    ".globl mouseISR \n"
    "mouseISR:       \n"
    "  pusha         \n" /* Save all registers           */
    "  call mouseHandler \n"
    "  popa          \n"
    "  iret          \n" /* Exit interrupt               */
);

void mouseHandler() {
  kprintf( "MOUSE!!!\n" );

  outportByte( mPic, eoi );
  outportByte( sPic, eoi );
  /* Return */
  return;
}

/***
 $Log: mouse.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:12  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:02  reddawg
 no message

 Revision 1.3  2004/09/07 21:54:38  reddawg
 ok reverted back to old scheduling for now....

 Revision 1.2  2004/09/06 15:13:25  reddawg
 Last commit before FreeBSD 6.0

 Revision 1.1  2004/06/04 10:20:53  reddawg
 mouse drive: fixed a few bugs works a bit better now

 END
 ***/

