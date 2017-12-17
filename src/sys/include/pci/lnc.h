/*-
 * Copyright (c) 2002-2004 The UbixOS Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of
 * conditions, the following disclaimer and the list of authors.  Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions, the following
 * disclaimer and the list of authors in the documentation and/or other materials provided
 * with the distribution. Neither the name of the UbixOS Project nor the names of its
 * contributors may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * THIS EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LNC_H
#define _LNC_H

#include <sys/types.h>

// TEMP COMMENT FRESH
#define RDP      0x10 // Register Data Port 16Bit
#define RDP32    0x10 // Register Data Port 32Bit
#define RAP      0x12 // Register Address Port 16Bit
#define RAP32    0x14 // Register Address Port 32Bit
#define RESET    0x14 // Reset Port 16Bit
#define RESET32  0x18 // Reset Port 32Bit
#define BDP      0x16 // 16Bit
#define BDP32    0x1C // 32Bit

// BCR18
#define BCR18      18
#define BCR18_DWIO 0x0080

// BCR20
#define BCR20        0x0014 

// Modes
#define MODE_16      0
#define MODE_32      1
#define MODE_INVALID 3


// CSR0
#define CSR0         0x0000
#define CSR0_STOP    0x0004

// CSR15
#define CSR15         15
#define CSR15_DXMTFCS 0x0008
#define CSR15_DRTY    0x0020
#define CSR15_PROM    0x8000

// CSR58
#define CSR58 0x003A

/* Structs */
struct mds {
    uint16_t md0;
    uint16_t md1;
    short md2;
    uint16_t md3;
};

struct hostRingEntry_old {
  struct mds *md;
  union {
    //struct mbuf *mbuf;
    char *data;
  } buff;
};

struct hostRingEntry {
  uint32_t addr;
  uint16_t bcnt;
  uint8_t  md[6];
  uint32_t reserved;
};

struct arpcom {
  //struct  ifnet ac_if;            /* network-visible interface */
  uint8_t ac_enaddr[6]; /* ethernet hardware address */
  int ac_multicnt; /* length of ac_multiaddrs list */
  void *ac_netgraph; /* ng_ether(4) netgraph node info */
};

struct nicInfo {
  int ident; /* Type of card */
  int ic; /* Type of ic, Am7990, Am79C960 etc. */
  int memMode;
  int iobase;
  int mode; /* Mode setting at initialization */
};

struct initBlock16 {
  uint16_t mode;    // Mode register
  uint8_t padr[6];  // Ethernet address
  uint8_t ladrf[8]; // Logical address filter (multicast)
  uint16_t rdra;    // Low order pointer to receive ring
  uint16_t rlen;    // High order pointer and no. rings
  uint16_t tdra;    // Low order pointer to transmit ring
  uint16_t tlen;    // High order pointer and no rings
};

struct initBlock32 {
  uint16_t mode;
  uint8_t rlen;
  uint8_t tlen;
  uint8_t padr[6];
  uint16_t res;
  uint8_t ladrf[8];
  uint32_t rdra;
  uint32_t tdra;
};


struct lncInfo {
    struct arpcom arpcom;
    struct nicInfo nic;
    struct hostRingEntry *rxRing;
    char *rxBuffer;
    struct hostRingEntry *txRing;
    char *txBuffer;
    struct initBlock32 init;
    unsigned int ioAddr;
    int nrdre;
    int ntdre;
    int bufferSize;
    int txPtr;
};

/* Functions */
void lnc_writeCSR(struct lncInfo *, uint16_t, uint16_t);
void lnc_writeCSR32(struct lncInfo *, uint32_t, uint32_t);

uint16_t lnc_readCSR(struct lncInfo *, uint16_t);
uint32_t lnc_readCSR32(struct lncInfo *, uint32_t);

void lnc_writeBCR(struct lncInfo *, uint16_t, uint16_t);
void lnc_writeBCR32(struct lncInfo *, uint32_t, uint32_t);

uint16_t lnc_readBCR(struct lncInfo *, uint16_t);
uint32_t lnc_readBCR32(struct lncInfo *, uint32_t);

void lnc_reset(struct lncInfo *);
void lnc_reset32(struct lncInfo *);

int lnc_probe(struct lncInfo *);

int lnc_switchDWord(struct lncInfo *);

int lnc_getMode(struct lncInfo *);

void lnc_isr();

// OLD

#define NDESC(len2)    (1 << len2)
#define NORMAL         0
#define MEM_SLEW       8
#define TRANSBUFSIZE   1518
#define RECVBUFSIZE    1518
#define NRDRE          3
#define NTDRE          3
#define ETHER_ADDR_LEN 6
#define NE2100_IOSIZE  24

#define PCNET_VSW      0x18
#define NE2100         2

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


extern struct lncInfo *lnc;


int initLNC();
int probe(struct lncInfo *lnc);
int lanceProbe(struct lncInfo *lnc);
int lncAttach(struct lncInfo *lnc, int unit);

void lncInt();
void _lncInt();

int lnc_sendPacket(struct lncInfo *lnc, void *packet, size_t len, uInt8 *dest);

#endif
