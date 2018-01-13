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

#include <isa/pit.h>
#include <sys/io.h>
#include <lib/kprintf.h>

/*****************************************************************************************

 Function: int pitInit()

 Description: This Function Will Initialize The Programmable Timer

 Notes:

 0040	r/w	PIT  counter 0, counter divisor	      (XT, AT, PS/2)
 0041	r/w	PIT  counter 1, RAM refresh counter   (XT, AT)
 0042	r/w	PIT  counter 2, cassette & speaker    (XT, AT, PS/2)
 0043	r/w	PIT  mode port, control word register for counters 0-2

 bit 7-6 = 00  counter 0 select
 = 01  counter 1 select	  (not PS/2)
 = 10  counter 2 select
 bit 5-4 = 00  counter latch command
 = 01  read/write counter bits 0-7 only
 = 10  read/write counter bits 8-15 only
 = 11  read/write counter bits 0-7 first, then 8-15
 bit 3-1 = 000 mode 0 select
 = 001 mode 1 select - programmable one shot
 = x10 mode 2 select - rate generator
 = x11 mode 3 select - square wave generator
 = 100 mode 4 select - software triggered strobe
 = 101 mode 5 select - hardware triggered strobe
 bit 0	 = 0   binary counter 16 bits
 = 1   BCD counter

 *****************************************************************************************/
int pit_init() {
  outportByteP(0x43, 0x36);
  outportByteP(0x40, ((1193180 / PIT_TIMER) & 0xFF));
  outportByte(0x40, (((1193180 / PIT_TIMER) >> 8) & 0xFF));

  /* Print out information on the PIT */
  kprintf("pit0 - Port [0x%X], Timer Hz: [%iHz]\n", 0x43, PIT_TIMER);

  /* Return so we know everything went well */
  return (0x0);
}

/***
 $Log: pit.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:12  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:03  reddawg
 no message

 Revision 1.7  2004/07/16 04:06:32  reddawg
 Tune ups this stuff should of been taken care of months ago

 Revision 1.6  2004/07/16 01:08:58  reddawg
 Whew we work once again

 Revision 1.5  2004/07/09 13:29:15  reddawg
 pit: pitInit to pit_init
 Adjusted initialization routines

 Revision 1.4  2004/05/21 12:48:22  reddawg
 Cleaned up

 Revision 1.3  2004/05/20 22:51:09  reddawg
 Cleaned Up Warnings

 Revision 1.2  2004/05/10 02:23:24  reddawg
 Minor Changes To Source Code To Prepare It For Open Source Release

 END
 ***/

