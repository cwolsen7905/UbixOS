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

#include <isa/ne2k.h>
#include <isa/8259.h>
#include <sys/device.old.h>
#include <sys/io.h>
#include <sys/idt.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>
#include <ubixos/kpanic.h>
#include <ubixos/vitals.h>
#include <ubixos/spinlock.h>
#include <assert.h>


static spinLock_t ne2k_spinLock = SPIN_LOCK_INITIALIZER;

static int dp_pkt2user(struct device *dev,int page,int length);
static void getblock(struct device *dev,int page,size_t offset,size_t size,void *dst);
static int dp_recv(struct device *);

static struct nicBuffer *ne2kBuffer = 0x0;
static struct device    *mDev        = 0x0;

asm(
  ".globl ne2kISR         \n"
  "ne2kISR:               \n"
  "  pusha                \n" /* Save all registers           */
  "  call ne2kHandler     \n"
  "  popa                 \n"
  "  iret                 \n" /* Exit interrupt               */
  );

/************************************************************************

Function: int ne2kInit(uInt32 ioAddr)
Description: This Function Will Initialize The Programmable Timer

Notes:

************************************************************************/
int ne2k_init() {
  mDev = (struct device *)kmalloc(sizeof(struct device));
  mDev->ioAddr = 0x280;
  mDev->irq    = 10;
  setVector(&ne2kISR, mVec+10, dPresent + dInt + dDpl0);
  irqEnable(10);
//  kprintf("ne0 - irq: %i, ioAddr: 0x%X MAC: %X:%X:%X:%X:%X:%X\n",dev->irq,dev->ioAddr,dev->net->mac[0] & 0xFF,dev->net->mac[1] & 0xFF,dev->net->mac[2] & 0xFF,dev->net->mac[3] & 0xFF,dev->net->mac[4] & 0xFF,dev->net->mac[5] & 0xFF);

  outportByte(mDev->ioAddr + NE_CMD, 0x21);        // stop mode
  outportByte(mDev->ioAddr + NE_DCR,0x29);         // 0x29 data config reg
  outportByte(mDev->ioAddr + NE_RBCR0,0x00);       // LOW byte count (remote)
  outportByte(mDev->ioAddr + NE_RBCR1,0x00);       // HIGH byte count (remote)
  outportByte(mDev->ioAddr + NE_RCR,0x3C);         // receive config reg
  outportByte(mDev->ioAddr + NE_TCR,0x02);         // LOOP mode (temp)
  outportByte(mDev->ioAddr + NE_PSTART,startPage); // 0x26 PAGE start
  outportByte(mDev->ioAddr + NE_BNRY,startPage);   // 0x26 BOUNDARY
  outportByte(mDev->ioAddr + NE_PSTOP,stopPage);   // 0x40 PAGE stop
  outportByte(mDev->ioAddr + NE_ISR,0xFF);         // interrupt status reg
  outportByte(mDev->ioAddr + NE_IMR,0x0B);
  outportByte(mDev->ioAddr + NE_CMD,0x61);         // PAGE 1 regs

  outportByte(mDev->ioAddr + DP_MAR0, 0xFF);
  outportByte(mDev->ioAddr + DP_MAR1, 0xFF);
  outportByte(mDev->ioAddr + DP_MAR2, 0xFF);
  outportByte(mDev->ioAddr + DP_MAR3, 0xFF);
  outportByte(mDev->ioAddr + DP_MAR4, 0xFF);
  outportByte(mDev->ioAddr + DP_MAR5, 0xFF);
  outportByte(mDev->ioAddr + DP_MAR6, 0xFF);
  outportByte(mDev->ioAddr + DP_MAR7, 0xFF);
  outportByte(mDev->ioAddr + DP_CURR, startPage + 1);
  outportByte(mDev->ioAddr + NE_CMD,  0x20);
  inportByte(mDev->ioAddr + DP_CNTR0);                /* reset counters by reading */
  inportByte(mDev->ioAddr + DP_CNTR1);
  inportByte(mDev->ioAddr + DP_CNTR2);

  outportByte(mDev->ioAddr + NE_TCR,  0x00);

  outportByte(mDev->ioAddr + NE_CMD, 0x0);
  outportByte(mDev->ioAddr + NE_DCR, 0x29);

  kprintf("Initialized");
  /* Return so we know everything went well */
  return(0x0);
  } /* ne2k_init */

int PCtoNIC(struct device *dev,void *packet,int length) {
  int     i        = 0x0;
  uInt16 *packet16 = (uInt16 *)packet;
  uInt8  *packet8  = (uInt8  *)packet;
  uInt8   word16   = 0x1;

  if ((inportByte(dev->ioAddr) & 0x04) == 0x04) {
    kpanic("Device Not Ready\n");
    }

  assert(length);
  if ((word16 == 1) && (length & 0x01)) {
    length++;
    }

  outportByte(dev->ioAddr+EN0_RCNTLO,(length & 0xFF));
  outportByte(dev->ioAddr+EN0_RCNTHI,(length >> 8));

  outportByte(dev->ioAddr+EN0_RSARLO,0x0);
  outportByte(dev->ioAddr+EN0_RSARHI,0x41);

  outportByte(dev->ioAddr,E8390_RWRITE+E8390_START);

  if (word16 != 0x0) {
    for(i=0;i<length/2;i++){
      outportWord(dev->ioAddr + NE_DATAPORT,packet16[i]);
      }
    }
  else {
    for(i=0;i<length;i++){
      outportByte(dev->ioAddr + NE_DATAPORT,packet8[i]);
      }
    }
  
  for (i = 0;i<=100;i++) {
    if ((inportByte(dev->ioAddr+EN0_ISR) & 0x40) == 0x40) {
      break;
      }
    }

  outportByte(dev->ioAddr+EN0_ISR,0x40);
  outportByte(dev->ioAddr+EN0_TPSR,0x41);//ei_local->txStartPage);
  outportByte(dev->ioAddr+0x05,(length & 0xFF));
  outportByte(dev->ioAddr+0x06,(length >> 8));
  outportByteP(dev->ioAddr,0x26);
  //kprintf("SENT\n");
  return(length);
  }

int NICtoPC(struct device *dev,void *packet,int length,int nic_addr) {
  int i = 0x0;
  uInt16 *packet16 = (uInt16 *)packet;

  assert(length);
  
  if (length & 0x01)
    length++;

   

  outportByte(dev->ioAddr+EN0_RCNTLO,(length & 0xFF));
  outportByte(dev->ioAddr+EN0_RCNTHI,(length >> 8));

  outportByte(dev->ioAddr+EN0_RSARLO,nic_addr & 0xFF);
  outportByte(dev->ioAddr+EN0_RSARHI,nic_addr >> 8);

  outportByte(dev->ioAddr,0x0A);

  for(i=0;i<length/2;i++){
    packet16[i] = inportWord(dev->ioAddr + NE_DATAPORT);
    }

  outportByte(dev->ioAddr+EN0_ISR,0x40);
  return(length);
  } /* PCtoNIC */

void ne2kHandler() {
  uInt16 isr    = 0x0;
  uInt16 status = 0x0;

  irqDisable(10);
  outportByte(mPic, eoi);
  outportByte(sPic, eoi);

  asm("sti");

  isr = inportByte(mDev->ioAddr + NE_ISR);

  if ((isr & 0x02) == 0x02) {
    outportByte(mDev->ioAddr + NE_ISR, 0x0A);
    status = inportByte(0x280 + NE_TPSR);
    } 
  if ((isr & 0x01) == 0x01) {
    if (dp_recv(mDev)) {
      kprintf("Error Getting Packet\n");
      }
    outportByte(mDev->ioAddr + NE_ISR, 0x05);
    }

  outportByte(mDev->ioAddr + NE_IMR,0x0);
  outportByte(mDev->ioAddr + NE_IMR,0x0B);

  asm("cli");
  irqEnable(10);

  return;
  } /* ne2kHandler */

static int dp_recv(struct device *dev) {
  dp_rcvhdr_t header;
  unsigned int pageno = 0x0, curr = 0x0, next = 0x0;
  int packet_processed = 0x0, r = 0x0;
  uInt16 eth_type = 0x0;  

  uInt32 length = 0x0;

  pageno = inportByte(dev->ioAddr + NE_BNRY) + 1;
  if (pageno == stopPage) pageno = startPage;

  do {
    outportByte(dev->ioAddr + NE_CMD, 0x40);
    curr = inportByte(dev->ioAddr + NE_CURRENT);
    outportByte(dev->ioAddr, 0x0);
    if (curr == pageno) break;
    getblock(dev, pageno, (size_t)0, sizeof(header), &header);
    getblock(dev, pageno, sizeof(header) + 2*sizeof(ether_addr_t), sizeof(eth_type), &eth_type);

    length = (header.dr_rbcl | (header.dr_rbch << 8)) - sizeof(dp_rcvhdr_t);
    next = header.dr_next;

    //kprintf("length: [0x%X:0x%X:0x%X]\n",header.dr_next,header.dr_status,length);

    if (length < 60 || length > 1514) {
      kprintf("dp8390: packet with strange length arrived: %d\n",length);
      next= curr;
      }
    else if (next < startPage || next >= stopPage) {
      kprintf("dp8390: strange next page\n");
      next= curr;
      }
    else if (header.dr_status & RSR_FO) {
      kpanic("dp8390: fifo overrun, resetting receive buffer\n");
      next = curr;
      }
    else if (header.dr_status & RSR_PRX) {
      r = dp_pkt2user(dev, pageno, length);
      if (r != OK) {
        kprintf("FRUIT");
        return(0x0);
        }

      packet_processed = 0x1;
      }
    if (next == startPage)
      outportByte(dev->ioAddr + NE_BNRY, stopPage - 1);
    else
      outportByte(dev->ioAddr + NE_BNRY, next - 1);

    pageno = next;

    } while (packet_processed == 0x0);
  return(0x0);
  } /* dp_recv */

static void getblock(struct device *dev,int page,size_t offset,size_t size,void *dst) {
        uInt16 *ha = 0x0;
        int i      = 0x0;

        ha = (uInt16 *) dst;
        offset = page * DP_PAGESIZE + offset;
        outportByte(dev->ioAddr + NE_RBCR0, size & 0xFF);
        outportByte(dev->ioAddr + NE_RBCR1, size >> 8);
        outportByte(dev->ioAddr + EN0_RSARLO, offset & 0xFF);
        outportByte(dev->ioAddr + EN0_RSARHI, offset >> 8);
        outportByte(dev->ioAddr + NE_CMD, E8390_RREAD | E8390_START);

        size /= 2;
        for (i= 0; i<size; i++)
                ha[i]= inportWord(dev->ioAddr + NE_DATAPORT);
  outportByte(dev->ioAddr+EN0_ISR,0x40);
  } /* getblock */

static int dp_pkt2user(struct device *dev,int page,int length) {
  int last = 0x0;
  struct nicBuffer *tmpBuf = 0x0;

  last = page + (length - 1) / DP_PAGESIZE;

  if (last >= stopPage) {
    kprintf("FOOK STOP PAGE!!!");
    }
  else {
    tmpBuf = ne2kAllocBuffer(length);
    NICtoPC(dev,tmpBuf->buffer,length,page * DP_PAGESIZE + sizeof(dp_rcvhdr_t));
    }
  return(OK);
  } /* db_pkt2user */

struct nicBuffer *ne2kAllocBuffer(int length) {
  struct nicBuffer *tmpBuf = 0x0;

  spinLock(&ne2k_spinLock);

  if (ne2kBuffer == 0x0) {
    ne2kBuffer = (struct nicBuffer *)kmalloc(sizeof(struct nicBuffer));
    ne2kBuffer->next   = 0x0;
    ne2kBuffer->length = length;
    ne2kBuffer->buffer = (char *)kmalloc(length);
    spinUnlock(&ne2k_spinLock);
    return(ne2kBuffer);
    }
  else {
    for (tmpBuf = ne2kBuffer;tmpBuf->next != 0x0;tmpBuf = tmpBuf->next);

    tmpBuf->next   = (struct nicBuffer *)kmalloc(sizeof(struct nicBuffer));
    tmpBuf         = tmpBuf->next;
    tmpBuf->next   = 0x0;
    tmpBuf->length = length;
    tmpBuf->buffer = (char *)kmalloc(length);
    spinUnlock(&ne2k_spinLock);
    return(tmpBuf);
    }
  spinUnlock(&ne2k_spinLock);
  return(0x0);
  } /* ne2kAllocBuffer */

struct nicBuffer *ne2kGetBuffer() {
  struct nicBuffer *tmpBuf = 0x0;

  if (ne2k_spinLock == 0x1)
    return(0x0);

  tmpBuf     = ne2kBuffer;
  if (ne2kBuffer != 0x0)
    ne2kBuffer = ne2kBuffer->next;
  return(tmpBuf);
  } /* nicBuffer */

void ne2kFreeBuffer(struct nicBuffer *buf) {
  kfree(buf->buffer);
  kfree(buf);
  return;
  } /* ne2kFreeBuffer */

/***
 END
 ***/
