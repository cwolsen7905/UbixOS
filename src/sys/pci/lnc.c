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

 $Log: lnc.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:35  reddawg
 no message

 Revision 1.1.1.1  2004/04/15 12:07:15  reddawg
 UbixOS v1.0

 Revision 1.4  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license



 $Id: lnc.c 54 2016-01-11 01:29:55Z reddawg $

*****************************************************************************************/

#include <pci/lnc.h>
#include <sys/io.h>
#include <sys/types.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <sys/video.h>
#include <isa/8259.h>

struct lncInfo *lnc = 0x0;

struct leinit {
        uInt16 init_mode;             /* +0x0000 */
        uInt16 init_padr[3];          /* +0x0002 */
        uInt16 init_ladrf[4];         /* +0x0008 */
        uInt16 init_rdra;             /* +0x0010 */
        uInt16 init_rlen;             /* +0x0012 */
        uInt16 init_tdra;             /* +0x0014 */
        uInt16 init_tlen;             /* +0x0016 */
        uInt16 pad0[4];               /* Pad to 16 shorts. */
} __packed;


static char const * const nicIdent[] = {
  "Unknown",
  "BICC",
  "NE2100",
  "DEPCA",
  "CNET98S",      /* PC-98 */
  };

static char const * const icIdent[] = {
  "Unknown",
  "LANCE",
  "C-LANCE",
  "PCnet-ISA",
  "PCnet-ISA+",
  "PCnet-ISA II",
  "PCnet-32 VL-Bus",
  "PCnet-PCI",
  "PCnet-PCI II",
  "PCnet-FAST",
  "PCnet-FAST+",
  "PCnet-Home",
  };  

struct leinit *init = 0x0;

void writeCsr(struct lncInfo *lnc, uInt16 port, uInt16 val) {
  outportWord(lnc->rap, port);
  outportWord(lnc->rdp, val);
  }

uInt16 readCsr(struct lncInfo *lnc, uInt16 port) {
  outportWord(lnc->rap, port);
  return(inportWord(lnc->rdp));
  }

void writeBcr(struct lncInfo *lnc, uInt16 port, uInt16 val) {
  outportWord(lnc->rap, port);
  outportWord(lnc->bdp, val);
  }

uInt16 readBcr(struct lncInfo *lnc, uInt16 port) {
  outportWord(lnc->rap, port);
  return (inportWord(lnc->bdp));
}

uInt32 readBCR32(struct lncInfo *lnc, uInt32 port) {
  outportDWord(lnc->rap, port);
  return (inportDWord(lnc->bdp));
}


int initLNC() {
  int            i    = 0x0;
  lnc = kmalloc(sizeof(struct lncInfo));
  
  lnc->rap = 0xD000 + PCNET_RAP;
  lnc->rdp = 0xD000 + PCNET_RDP;
  lnc->bdp = 0xD000 + PCNET_BDP;

  init = kmalloc(sizeof(struct leinit));

  inportDWord(0xD000 + 0x18);
  inportWord(0xD000 + 0x14);

  //outportDWord(0xD000 + 0x10, 0);

  lnc->nic.ic = probe(lnc);

  kprintf("ID: %i\n", lnc->nic.ic);

  if ((lnc->nic.ic > 0) && (lnc->nic.ic >= PCnet_32)) {
    lnc->nic.ident = NE2100;
    lnc->nic.memMode = DMA_FIXED;

    lnc->nrdre = NRDRE;
    lnc->ntdre = NTDRE;

    /* Extract MAC address from PROM */
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
      lnc->arpcom.ac_enaddr[i] = inportByte(0xD000 + i);
      kprintf("[0x%X]",lnc->arpcom.ac_enaddr[i]);
      }
    }
  else {
    kprintf("LNC Init Error\n");
    return(-1);
    }

  lncAttach(lnc,0);


  memset(init,0x0,sizeof(struct leinit));

  init->init_mode = 0x8000;
  init->init_padr[0] = (lnc->arpcom.ac_enaddr[1] << 8) | lnc->arpcom.ac_enaddr[0];
  init->init_padr[1] = (lnc->arpcom.ac_enaddr[3] << 8) | lnc->arpcom.ac_enaddr[2];
  init->init_padr[2] = (lnc->arpcom.ac_enaddr[5] << 8) | lnc->arpcom.ac_enaddr[4];

  init->init_rdra = lnc->recvRing;
  init->init_rlen = 3;

  init->init_tdra = lnc->recvRing;
  init->init_tlen = 3;

  writeCsr(lnc, CSR1, (int)init & 0xFFFF);
  writeCsr(lnc, CSR2, (int)init >> 16);

  writeCsr(lnc, CSR3, 0);
  writeCsr(lnc, CSR0, INIT);

  for (i = 0; i < 1000; i++)
    if (readCsr(lnc, CSR0) & IDON)
      break;

  if (readCsr(lnc, CSR0) & IDON) {
    setVector(_lncInt,mVec+9, (dInt + dPresent + dDpl3));
    irqEnable(9);
    writeCsr(lnc, CSR0, STRT | INEA );
    /* 
     * sc->arpcom.ac_if.if_flags |= IFF_RUNNING;
     * sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
     * lnc_start(&sc->arpcom.ac_if);
     */
    }
  else {
    kprintf("LNC: init Error\n");
    return(-1);
    }

  uInt16 iW = 0;

  iW = readBcr(lnc, 0x2);
  kprintf("BCR2.0: [0x%X]",iW);

  iW = readBcr(lnc, 0x4);
  kprintf("BCR2.1: [0x%X]",iW);


  while (1)
  lnc_sendPacket(lnc,0x0,32,0x0);

  return(0);
  }

int probe(struct lncInfo *lnc) {
  uInt32 chipId = 0x0;
  int    type   = 0x0;

  if ((type = lanceProbe(lnc))) {
    chipId = readCsr(lnc, CSR89);
    chipId <<= 16;
    chipId |= readCsr(lnc, CSR88);
    if (chipId & AMD_MASK) {
      chipId >>= 12;
      switch (chipId & PART_MASK) {
        case Am79C960:
          return(PCnet_ISA);
        case Am79C961:
          return (PCnet_ISAplus);
        case Am79C961A:
          return (PCnet_ISA_II);
        case Am79C965:
          return (PCnet_32);
        case Am79C970:
          return (PCnet_PCI);
        case Am79C970A:
          return (PCnet_PCI_II);
        case Am79C971:
          return (PCnet_FAST);
        case Am79C972:
        case Am79C973:
          return (PCnet_FASTplus);
        case Am79C978:
          return (PCnet_Home);
        default:
          break;
        }
      }
    }
  return (type);
  }

int lanceProbe(struct lncInfo *lnc) {
  uInt16 inW = 0;

  writeCsr(lnc, CSR0, STOP);

  inW = inportWord(lnc->rdp);

  //MrOlsen (2017-12-16) - kprintf("[inW: {0x%X} 0x%X - 0x%X - 0x%X - (0x%X)]", lnc->rdp,inW, STOP, inW & STOP, readCsr(lnc, CSR3));

  if ((inW & STOP) && !(readCsr(lnc, CSR3))) {
    writeCsr(lnc, CSR0, INEA);
    if (readCsr(lnc, CSR0) & INEA) {
      return(C_LANCE);
      }
    else {
      return(LANCE);
      }
    }
  else {
    return(UNKNOWN);
    }
  }

void lncInt() {
  uInt16 csr0 = 0x0;
  kprintf("TEST");
  while (1);
  while ((csr0 = inportWord(lnc->rdp)) & INTR) {
    outportWord(lnc->rdp, csr0);
    kprintf("CSR0: [0x%X]\n",csr0);
    if (csr0 & ERR) {
      kprintf("Error: [0x%X]\n",csr0);
      }
    if (csr0 & RINT) {
      kprintf("RINT\n");
      }
    if (csr0 & TINT) {
      kprintf("TINT\n");
      }
    }
  kprintf("Finished!!!\n");
  outportByte(0x20,0x20);
  return;
  }

asm(
    ".global _lncInt     \n"
    "_lncInt   :         \n"

  "  pusha                \n" /* Save all registers           */
  "  pushw %ds            \n" /* Set up the data segment      */
  "  pushw %es            \n"
  "  pushw %ss            \n" /* Note that ss is always valid */
  "  pushw %ss            \n"
  "  popw %ds             \n"
  "  popw %es             \n"
  "  call lncInt \n"
  "  popw %es             \n"
  "  popw %ds             \n" /* Restore registers            */
  "  popa                 \n"
  "  iret                 \n" /* Exit interrupt               */    
    );

int lncAttach(struct lncInfo *lnc,int unit) {
  int lncMemSize = 0x0;

  //lncMemSize = ((NDESC(lnc->nrdre) + NDESC(lnc->ntdre)) * sizeof(struct hostRingEntry));
  lncMemSize = (NDESC(lnc->nrdre) * sizeof(struct hostRingEntry));

  if (lnc->nic.memMode != SHMEM)
    lncMemSize += sizeof(struct initBlock) + (sizeof(struct mds) * (NDESC(lnc->nrdre) + NDESC(lnc->ntdre))) + MEM_SLEW;

  if (lnc->nic.memMode == DMA_FIXED)
    lncMemSize += (NDESC(lnc->nrdre) * RECVBUFSIZE) + (NDESC(lnc->ntdre) * TRANSBUFSIZE);

  if (lnc->nic.memMode != SHMEM) {
    if (lnc->nic.ic < PCnet_32) {
      /* ISA based cards */
      kprintf("ISA Board\n");
      /* sc->recv_ring = contigmalloc(lnc_mem_size, M_DEVBUF, M_NOWAIT,0ul, 0xfffffful, 4ul, 0x1000000); */
      }
    else {
      /*
      * For now it still needs to be below 16MB because the
      * descriptor's can only hold 16 bit addresses.
      */
      /* sc->recv_ring = contigmalloc(lnc_mem_size, M_DEVBUF, M_NOWAIT,0ul, 0xfffffful, 4ul, 0x1000000); */
      lnc->recvRing = kmalloc(lncMemSize);
      kprintf("PCI Board\n");
      }      
    }

  if (!lnc->recvRing) {
    kprintf("lnc%d: Couldn't allocate memory for NIC\n", unit);
    return (-1);
  }


  lncMemSize = (NDESC(lnc->ntdre) * sizeof(struct hostRingEntry));
  lnc->transRing = kmalloc(lncMemSize);

  if (!lnc->recvRing) {
    kprintf("lnc%d: Couldn't allocate memory for NIC\n", unit);
    return (-1);
  }

  lnc->nic.mode = NORMAL;

  /* Fill in arpcom structure entries */
  /*
  lnc->arpcom.ac_if.if_softc = sc;
  lnc->arpcom.ac_if.if_name = lncdriver.name;
  lnc->arpcom.ac_if.if_unit = unit;
  lnc->arpcom.ac_if.if_mtu = ETHERMTU;
  lnc->arpcom.ac_if.if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
  lnc->arpcom.ac_if.if_timer = 0;
  lnc->arpcom.ac_if.if_output = ether_output;
  lnc->arpcom.ac_if.if_start = lnc_start;
  lnc->arpcom.ac_if.if_ioctl = lnc_ioctl;
  lnc->arpcom.ac_if.if_watchdog = lnc_watchdog;
  lnc->arpcom.ac_if.if_init = lnc_init;
  lnc->arpcom.ac_if.if_type = IFT_ETHER;
  lnc->arpcom.ac_if.if_addrlen = ETHER_ADDR_LEN;
  lnc->arpcom.ac_if.if_hdrlen = ETHER_HDR_LEN;
  lnc->arpcom.ac_if.if_snd.ifq_maxlen = IFQ_MAXLEN;
  */
  /* ether_ifattach(&sc->arpcom.ac_if, ETHER_BPF_SUPPORTED); */

  kprintf("lnc%d: ", unit);
  if (lnc->nic.ic == LANCE || lnc->nic.ic == C_LANCE)
    kprintf("%s (%s)",nicIdent[lnc->nic.ident], icIdent[lnc->nic.ic]);
  else
    kprintf("%s", icIdent[lnc->nic.ic]);

  kprintf(" address 0x%X\n", lnc->arpcom.ac_enaddr);   

  return(1);
  }

int lnc_sendPacket(struct lncInfo *lnc, void *packet, size_t len, uInt8 *dest) {
  int tx_ptr = 0;
  char data[1548] = "SDFSDF";

  char *tdes = lnc->transRing;

  tdes[(tx_ptr * 16) + 7] |= 0x2;
  tdes[(tx_ptr * 16) + 7] |= 0x1;

  uInt16 bcnt = (uInt16)(-len);
  bcnt &= 0xFFF;
  bcnt |= 0xF000;
  *(uInt16 *)&tdes[tx_ptr * 16 + 4] = bcnt;
  tdes[tx_ptr * 16 + 7] |= 0x80;
   
}

/***
 END
 ***/

