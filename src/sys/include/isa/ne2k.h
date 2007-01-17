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

#ifndef _NE2K_H
#define _NE2K_H

#include <ubixos/types.h>
#include <sys/device.old.h>

#define ether_addr  ether_addr_t
typedef struct dp_rcvhdr
{
        uInt8 dr_status;                 /* Copy of rsr                       */
        uInt8 dr_next;                   /* Pointer to next packet            */
        uInt8 dr_rbcl;                   /* Receive Byte Count Low            */
        uInt8 dr_rbch;                   /* Receive Byte Count High           */
} dp_rcvhdr_t;

typedef union etheraddr {
    unsigned char bytes[6];             /* byteorder safe initialization */
    unsigned short shorts[3];           /* force 2-byte alignment */
} ether_addr;


struct nicBuffer {
  struct nicBuffer *next;
  int               length;
  char             *buffer;
  };

#define RSR_FO         0x08
#define RSR_PRX                0x01
#define DEF_ENABLED    0x200

#define OK      0


#define startPage 0x4C
#define stopPage  0x80


#define NE_CMD       0x00
#define NE_PSTART    0x01
#define NE_PSTOP     0x02
#define NE_BNRY      0x03
#define NE_TPSR      0x04
#define NE_ISR       0x07
#define NE_CURRENT   0x07
#define NE_RBCR0     0x0A
#define NE_RBCR1     0x0B
#define NE_RCR       0x0C
#define NE_TCR       0x0D
#define NE_DCR       0x0E
#define NE_IMR       0x0F


#define NE_DCR_WTS   0x01
#define NE_DCR_LS    0x08
#define NE_DCR_AR    0x10
#define NE_DCR_FT1   0x40
#define NE_DCR_FT0   0x20



#define E8390_STOP   0x01
#define E8390_NODMA  0x20
#define E8390_PAGE0  0x00
#define E8390_PAGE1  0x40
#define E8390_CMD    0x00
#define E8390_START  0x02
#define E8390_RREAD  0x08
#define E8390_RWRITE 0x10
#define E8390_RXOFF  0x20
#define E8390_TXOFF  0x00
#define E8390_RXCONFIG 0x04
#define E8390_TXCONFIG 0x00

#define EN0_COUNTER0 0x0d
#define EN0_DCFG     0x0e
#define EN0_RCNTLO   0x0a
#define EN0_RCNTHI   0x0b
#define EN0_ISR      0x07
#define EN0_IMR      0x0f
#define EN0_RSARLO   0x08
#define EN0_RSARHI   0x09
#define EN0_TPSR     0x04
#define EN0_RXCR     0x0c
#define EN0_TXCR     0x0D
#define EN0_STARTPG  0x01
#define EN0_STOPPG   0x02
#define EN0_BOUNDARY 0x03

#define EN1_PHYS     0x01
#define EN1_CURPAG   0x07
#define EN1_MULT     0x08

#define NE1SM_START_PG 0x20
#define NE1SM_STOP_PG 0x40
#define NESM_START_PG 0x40
#define NESM_STOP_PG  0x80

#define ENISR_ALL    0x3f

#define ENDCFG_WTS   0x01

#define NE_DATAPORT  0x10

#define TX_2X_PAGES 12
#define TX_1X_PAGES 6
#define TX_PAGES (dev->priv->pingPong ? TX_2X_PAGES : TX_1X_PAGES)


#define DP_CURR         0x7     /* Current Page Register             */
#define DP_MAR0         0x8     /* Multicast Address Register 0      */
#define DP_MAR1         0x9     /* Multicast Address Register 1      */
#define DP_MAR2         0xA     /* Multicast Address Register 2      */
#define DP_MAR3         0xB     /* Multicast Address Register 3      */
#define DP_MAR4         0xC     /* Multicast Address Register 4      */
#define DP_MAR5         0xD     /* Multicast Address Register 5      */
#define DP_MAR6         0xE     /* Multicast Address Register 6      */
#define DP_MAR7         0xF     /* Multicast Address Register 7      */

#define DP_CNTR0        0xD     /* Tally Counter 0                   */
#define DP_CNTR1        0xE     /* Tally Counter 1                   */
#define DP_CNTR2        0xF     /* Tally Counter 2                   */


#define DP_PAGESIZE     256

extern char *nicPacket;
extern uInt32 packetLength;


int ne2k_init();
int ne2kProbe(int,struct device *);
int ne2kDevInit(struct device *);
void NS8390_init(struct device *dev,int startp);

void ne2kISR();
void ne2kHandler();

int NICtoPC(struct device *dev,void *packet,int length,int nic_addr);
int PCtoNIC(struct device *dev,void *packet,int length);

struct nicBuffer *ne2kAllocBuffer(int);
struct nicBuffer *ne2kGetBuffer();
void ne2kFreeBuffer(struct nicBuffer *);

#endif

/***
 $Log$
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:36  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:39  reddawg
 no message

 Revision 1.6  2004/07/14 12:03:49  reddawg
 ne2k: ne2kInit to ne2k_init
 Changed Startup Routines

 Revision 1.5  2004/05/21 14:57:16  reddawg
 Cleaned up


 END
 ***/
