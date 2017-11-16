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

 $Id: gdt.h 204 2016-01-23 17:53:11Z reddawg $

 *****************************************************************************************/

#ifndef _GDT_H_
#define _GDT_H_

#define SEL_GET_PL(sel)  ((sel)&3) /* Get Priority Level Of Selector */
#define SEL_GET_LG(sel)  ((sel)&4) /* Return Local/Global Descriptor */
#define SEL_GET_IDX(sel) (((sel)>>3) & 0x1FFF) /* Get Selector Index */

#define SEL_PL_KERN 0x0
#define SEL_PL_USER 0x3

/* Descriptor Definitions */
#define dCall 0x0C00 /* 386 Call Gate                */
#define dCode 0x1800 /* Code Segment                 */
#define dData 0x1000 /* Data Segment                 */
#define dInt  0x0E00 /* 386 Interrupt Gate           */
#define dLdt  0x200  /* Local Descriptor Table (LDT) */
#define dTask 0x500  /* Task gate                    */
#define dTrap 0x0F00 /* 386 Trap Gate                */
#define dTss  0x900  /* Task State Segment (TSS)     */

/* Descriptor Options */
#define dDpl3     0x6000 /* DPL3 or mask for DPL             */
#define dDpl2     0x4000 /* DPL2 or mask for DPL             */
#define dDpl1     0x2000 /* DPL1 or mask for DPL             */
#define dDpl0     0x0000 /* DPL0 or mask for DPL             */
#define dPresent  0x8000 /* Present                          */
#define dNpresent 0x8000 /* Not Present                      */
#define dAcc      0x100  /* Accessed (Data or Code)          */
#define dWrite    0x200  /* Writable (Data segments only)    */
#define dRead     0x200  /* Readable (Code segments only)    */
#define dBusy     0xB00  /* Busy (TSS only)    was 200       */
#define dEexdown  0x400  /* Expand down (Data segments only) */
#define dConform  0x400  /* Conforming (Code segments only)  */
#define dBig      0x40   /* Default to 32 bit mode           */
#define dBiglim   0x80   /* Limit is in 4K units             */

/* GDT Descriptor */
struct gdtDescriptor {
    unsigned short limitLow; /* Limit 0..15    */
    unsigned short baseLow; /* Base  0..15    */
    unsigned char baseMed; /* Base  16..23   */
    unsigned char access; /* Access Byte    */
    unsigned int limitHigh :4; /* Limit 16..19   */
    unsigned int granularity :4; /* Granularity    */
    unsigned char baseHigh; /* Base 24..31    */
}__attribute__ ((packed));

struct gdtGate {
    unsigned short offsetLow; /* Offset 0..15  */
    unsigned short selector; /* Selector      */
    unsigned short access; /* Access Flags  */
    unsigned short offsetHigh; /* Offset 16..31 */
}__attribute__ ((packed));

union descriptorTableUnion {
    struct gdtDescriptor descriptor; /* Normal descriptor */
    struct gdtGate gate; /* Gate descriptor */
    unsigned long dummy; /* Any other info */
};

#define ubixDescriptorTable(name, length) union descriptorTableUnion name[length] =
#define ubixStandardDescriptor(base, limit, control) {.descriptor = {(limit & 0xffff), (base & 0xffff), ((base >> 16) & 0xff), ((control + dPresent) >> 8), (limit >> 16), ((control & 0xff) >> 4), (base >> 24)}}
#define ubixGateDescriptor(offset, selector, control) {.gate = {(offset & 0xffff), selector, (control+dPresent), (offset >> 16) }}

extern union descriptorTableUnion ubixGDT[11];

#endif

/***
 $Log: gdt.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:15  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:52  reddawg
 no message

 Revision 1.5  2004/08/15 16:47:49  reddawg
 Fixed

 Revision 1.4  2004/07/22 20:53:07  reddawg
 atkbd: fixed problem

 Revision 1.3  2004/05/21 15:12:17  reddawg
 Cleaned up

 END
 ***/
