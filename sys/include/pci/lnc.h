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

 $Id: lnc.h 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#ifndef _LNC_H
#define _LNC_H

#include <ubixos/types.h>

#define NDESC(len2) (1 << len2)
#define NORMAL 0
#define MEM_SLEW 8
#define TRANSBUFSIZE 1518
#define RECVBUFSIZE 1518
#define NRDRE 3
#define NTDRE 3
#define    ETHER_ADDR_LEN          6
#define NE2100_IOSIZE  24
#define PCNET_RDP    0x10        /* Register Data Port */
#define PCNET_RAP    0x12        /* Register Address Port */
#define PCNET_RESET  0x14
#define PCNET_BDP    0x16
#define PCNET_VSW    0x18
#define NE2100          2

/* mem_mode values */
#define DMA_FIXED       1
#define DMA_MBUF        2
#define SHMEM           4


/********** Chip Types **********/
#define UNKNOWN         0        /* Unknown  */
#define LANCE           1        /* Am7990   */
#define C_LANCE         2        /* Am79C90  */
#define PCnet_ISA       3        /* Am79C960 */
#define PCnet_ISAplus   4        /* Am79C961 */
#define PCnet_ISA_II    5        /* Am79C961A */
#define PCnet_32        6        /* Am79C965 */
#define PCnet_PCI       7        /* Am79C970 */
#define PCnet_PCI_II    8        /* Am79C970A */
#define PCnet_FAST      9        /* Am79C971 */
#define PCnet_FASTplus  10       /* Am79C972 */
#define PCnet_Home      11       /* Am79C978 */

/******** AM7990 Specifics **************/
#define CSR0    0x0000
#define CSR1    1
#define CSR2    2
#define CSR3    3
#define CSR88   88
#define CSR89   89

#define ERR     0x8000
#define BABL    0x4000
#define CERR    0x2000
#define MISS    0x1000
#define MERR    0x0800
#define RINT    0x0400
#define TINT    0x0200
#define IDON    0x0100
#define INTR    0x0080
#define INEA    0x0040
#define RXON    0x0020
#define TXON    0x0010
#define TDMD    0x0008
#define STOP    0x0004
#define STRT    0x0002
#define INIT    0x0001


/* CSR88-89: Chip ID masks */
#define AMD_MASK  0x003
#define PART_MASK 0xffff
#define Am79C960  0x0003
#define Am79C961  0x2260
#define Am79C961A 0x2261
#define Am79C965  0x2430
#define Am79C970  0x0242
#define Am79C970A 0x2621
#define Am79C971  0x2623
#define Am79C972  0x2624
#define Am79C973  0x2625
#define Am79C978  0x2626

/********** Structs **********/




struct initBlock {
  uInt16 mode;           /* Mode register                        */
  uInt8  padr[6];        /* Ethernet address                     */
  uInt8  ladrf[8];       /* Logical address filter (multicast)   */
  uInt16 rdra;           /* Low order pointer to receive ring    */
  uInt16 rlen;           /* High order pointer and no. rings     */
  uInt16 tdra;           /* Low order pointer to transmit ring   */
  uInt16 tlen;           /* High order pointer and no rings      */
  };

struct mds {
  uInt16 md0;
  uInt16 md1;
  short  md2;
  uInt16 md3;
  };

struct hostRingEntry {
  struct mds *md;
  union {
    //struct mbuf *mbuf;
    char *data;
    }buff;
  };

struct arpcom {
  //struct  ifnet ac_if;            /* network-visible interface */
  uInt8  ac_enaddr[6];           /* ethernet hardware address */
  int    ac_multicnt;            /* length of ac_multiaddrs list */
  void   *ac_netgraph;           /* ng_ether(4) netgraph node info */
  };

struct nicInfo {
  int ident;         /* Type of card */
  int ic;            /* Type of ic, Am7990, Am79C960 etc. */
  int memMode;
  int iobase;
  int mode;          /* Mode setting at initialization */
  };

struct lncInfo {
  struct arpcom        arpcom;
  struct nicInfo       nic;
  struct hostRingEntry *recvRing;
  struct hostRingEntry *transRings;
  struct initBlock     *initBloack;
  int                  rap;
  int                  rdp;
  int                  bdp;
  int                  nrdre;
  int                  ntdre;
  };

extern struct lncInfo *lnc;

void writeCsr(struct lncInfo *lnc, uInt16 port, uInt16 val);
uInt16 readCsr(struct lncInfo *lnc, uInt16 port);
void writeBcr(struct lncInfo *lnc, uInt16 port, uInt16 val);
uInt16 readBcr(struct lncInfo *lnc, uInt16 port);

void initLNC();
int probe(struct lncInfo *lnc);
int lanceProbe(struct lncInfo *lnc);
int lncAttach(struct lncInfo *lnc,int unit);


void lncInt();
void _lncInt();

#endif

/***
 $Log: lnc.h,v $
 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:50  reddawg
 no message

 Revision 1.2  2004/05/21 15:05:07  reddawg
 Cleaned up


 END
 ***/
