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

#include <isa/8259.h>
#include <sys/io.h>
#include <lib/kprintf.h>

static unsigned int irqMask = 0xFFFF;

/*!
 * \brief initialize the 8259 PIC
 *
 * This will initialize both PICs for all of our IRQs
 *
 */
int i8259_init() {
  outportByte(mPic, icw1); /* Initialize Master PIC           */
  outportByte(sPic, icw1); /* Initialize Seconary PIC         */
  outportByte(mPic + 1, mVec); /* Master Interrup Vector          */
  outportByte(sPic + 1, sVec); /* Secondary Interrupt Vector      */
  outportByte(mPic + 1, 1 << 2); /* Bitmask for cascade on IRQ 2    */
  outportByte(sPic + 1, 2); /* Cascade on IRQ 2                */
  outportByte(mPic + 1, icw4); /* Finish Primary Initialization   */
  outportByte(sPic + 1, icw4); /* Finish Seconary Initialization  */
  outportByte(mImr, 0xff); /* Mask All Primary Interrupts     */
  outportByte(sImr, 0xff); /* Mask All Seconary Interrupts    */

  /* Print out the system info for this */
  kprintf("pic0 - Port: [0x%X]\n", mPic);
  kprintf("pic1 - Port: [0x%X]\n", sPic);

  /* Return so the system knows it went well */
  return (0x0);
}

/*!
 * \brief enable specified IRQ
 *
 * \param irqNo IRQ to enable
 */
void irqEnable_old(u_int16_t irqNo) {
  irqMask &= ~(1 << irqNo);
  if (irqNo >= 8) {
    irqMask &= ~(1 << 2);
  }
  outportByte(mPic + 1, irqMask & 0xFF);
  outportByte(sPic + 1, (irqMask >> 8) & 0xFF);
}

void irqEnable(uint16_t irqNo) {
  uint16_t port;
  uint8_t value;

  if (irqNo < 8) {
    port = mImr;
  }
  else {
    port = sImr;
    irqNo -= 8;
  }
  value = inportByte(port) & ~(1 << irqNo);
  outportByte(port, value);
}

/*!
 * \brief disable specified IRQ
 *
 * \param irqNo IRQ to disable
 */
void irqDisable_old(u_int16_t irqNo) {
  irqMask |= (1 << irqNo);
  if ((irqMask & 0xFF00) == 0xFF00) {
    irqMask |= (1 << 2);
  }
  outportByte(mPic + 1, irqMask & 0xFF);
  outportByte(sPic + 1, (irqMask >> 8) & 0xFF);
}

void irqDisable(uint16_t irqNo) {
  uint16_t port;
  uint8_t value;

  if (irqNo < 8) {
    port = mImr;
  }
  else {
    port = sImr;
    irqNo -= 8;
  }
  value = inportByte(port) | (1 << irqNo);
  outportByte(port, value);
}

/***
 END
 ***/
