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

#ifndef _ISA_8259_H
#define _ISA_8259_H

#include <sys/types.h>

#define mPic    0x20 // I/O for master PIC      
#define mImr    0x21 // I/O for master IMR      
#define sPic    0xA0 // I/O for slave PIC       
#define sImr    0xA1 // I/O for slace IMR       
#define eoi     0x20 // EOI command             
#define icw1    0x11 // Cascade, Edge triggered 
#define icw4    0x01 // 8088 mode               
#define mVec    0x68 // Vector for master       
#define sVec    0x70 // Vector for slave        
#define ocw3Irr 0x0A // Read IRR                
#define ocw3Isr 0x0B // Read ISR                

int i8259_init();
void irqEnable(uInt16 irqNo);
void irqDisable(uInt16 irqNo);

#endif

/***
 $Log: 8259.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:39  reddawg
 no message

 Revision 1.4  2004/07/09 13:20:08  reddawg
 Oh yeah duh you can not name functions with numbers

 Revision 1.3  2004/07/09 13:14:29  reddawg
 8259: changed init8259 to 8259_init
 Adjusted Startup Routines

 Revision 1.2  2004/05/21 14:57:16  reddawg
 Cleaned up

 END
 ***/
