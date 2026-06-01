/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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

#ifndef _PCI_H
#define _PCI_H

#include <sys/types.h>

struct pciConfig {
    u_int16_t vendorID;
    u_int16_t deviceID;

    u_int16_t command;
    u_int16_t status;

    u_int8_t revisionID;
    u_int8_t progIf;
    u_int8_t subClass;
    u_int8_t classCode;

    u_int8_t cacheLineSize;
    u_int8_t latencyTimer;
    u_int8_t headerType;
    u_int8_t bist;

    u_int32_t bar[6];

    u_int32_t cbPointer;

    u_int16_t subsysVendorID;
    u_int16_t subsysID;

    u_int32_t epromAddr;

    u_int16_t capabilites;
    u_int16_t res1;

    u_int32_t res2;

    u_int8_t intLine;
    u_int8_t intPin;
    u_int8_t minGrant;
    u_int8_t maxLatency;

    /* device info */
    //u_int8_t  bus;
    //u_int8_t  dev;
    //u_int8_t  func;
    //u_int8_t  irq;
    //u_int8_t irqLine;
    /* base registers */
    //u_int32_t base[6];
    //u_int32_t size[6];
    //u_int16_t subsysVendor;
    //u_int16_t subsys;
    /* Device Info */
    //Move this to anotther struct eventually
    u_int8_t bus;
    u_int8_t dev;
    u_int8_t func;
    u_int8_t _pad;

    /* Decoded BAR sizes (filled by pciProbe after BAR size discovery) */
    u_int32_t barSize[6];
};

struct confadd {
    u_int8_t reg :8;
    u_int8_t func :3;
    u_int8_t dev :5;
    u_int8_t bus :8;
    u_int8_t rsvd :7;
    u_int8_t enable :1;
};

#define countof(a)     (sizeof(a) / sizeof(a[0]))

struct pci_driver {
    u_int16_t vendor;
    u_int16_t device;
    int (*init)(struct pciConfig *);
};

int pci_init();

u_int32_t pciProbe(int bus, int dev, int func);
u_int32_t pciRead(int bus, int dev, int func, int reg, int bytes);
void pciWrite(int bus, int dev, int func, int reg, u_int32_t v, int bytes);

#endif
