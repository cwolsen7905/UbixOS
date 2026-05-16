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

#include <pci/pci.h>
#include <pci/e1000.h>
#include <pci/hd.h>
#include <pci/lnc.h>
#include <sys/bus.h>
#include <sys/io.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <string.h>

const struct {
  uInt8 baseClass;
  uInt8 subClass;
  uInt8 interface;
  const char* name;
} pciClasses[] = { { 0x00, 0x00, 0x00, "Undefined" }, { 0x00, 0x01, 0x00, "VGA" },

{ 0x01, 0x00, 0x00, "SCSI" }, { 0x01, 0x01, 0x00, "IDE" }, { 0x01, 0x01, 0x8A, "IDE" }, { 0x01, 0x02, 0x00, "Floppy" }, { 0x01, 0x03, 0x00, "IPI" }, { 0x01, 0x04, 0x00, "RAID" }, { 0x01, 0x80, 0x00, "Other" },

{ 0x02, 0x00, 0x00, "Ethernet" }, { 0x02, 0x01, 0x00, "Token Ring" }, { 0x02, 0x02, 0x00, "FDDI" }, { 0x02, 0x03, 0x00, "ATM" }, { 0x02, 0x04, 0x00, "ISDN" }, { 0x02, 0x80, 0x00, "Other" },

{ 0x03, 0x00, 0x00, "VGA" }, { 0x03, 0x00, 0x01, "VGA+8514" }, { 0x03, 0x01, 0x00, "XGA" }, { 0x03, 0x02, 0x00, "3D" }, { 0x03, 0x80, 0x00, "VGA Other" },

{ 0x04, 0x00, 0x00, "Video" }, { 0x04, 0x01, 0x00, "Audio" }, { 0x04, 0x02, 0x00, "Telephony" }, { 0x04, 0x80, 0x00, "Other" },

{ 0x05, 0x00, 0x00, "RAM" }, { 0x05, 0x01, 0x00, "Flash" }, { 0x05, 0x80, 0x00, "Other" },

{ 0x06, 0x00, 0x00, "PCI to HOST" }, { 0x06, 0x01, 0x00, "PCI to ISA" }, { 0x06, 0x02, 0x00, "PCI to EISA" }, { 0x06, 0x03, 0x00, "PCI to MCA" }, { 0x06, 0x04, 0x00, "PCI to PCI" }, { 0x06, 0x04, 0x01, "PCI to PCI (Subtractive Decode)" }, { 0x06, 0x05, 0x00, "PCI to PCMCIA" }, { 0x06, 0x06, 0x00, "PCI to NuBUS" }, { 0x06, 0x07, 0x00, "PCI to Cardbus" }, { 0x06, 0x08, 0x00, "PCI to RACEway" }, { 0x06, 0x09, 0x00, "PCI to PCI" }, { 0x06, 0x0A, 0x00, "PCI to InfiBand" }, { 0x06, 0x80, 0x00, "PCI to Other" },

{ 0x07, 0x00, 0x00, "Serial" }, { 0x07, 0x00, 0x01, "Serial - 16450" }, { 0x07, 0x00, 0x02, "Serial - 16550" }, { 0x07, 0x00, 0x03, "Serial - 16650" }, { 0x07, 0x00, 0x04, "Serial - 16750" }, { 0x07, 0x00, 0x05, "Serial - 16850" }, { 0x07, 0x00, 0x06, "Serial - 16950" }, { 0x07, 0x01, 0x00, "Parallel" }, { 0x07, 0x01, 0x01, "Parallel - BiDir" }, { 0x07, 0x01, 0x02, "Parallel - ECP" }, { 0x07, 0x01, 0x03, "Parallel - IEEE1284" }, { 0x07, 0x01, 0xFE, "Parallel - IEEE1284 Target" }, { 0x07, 0x02, 0x00, "Multiport Serial" }, { 0x07, 0x03, 0x00, "Hayes Compatible Modem" }, { 0x07, 0x03, 0x01, "Hayes Compatible Modem, 16450" }, { 0x07, 0x03, 0x02, "Hayes Compatible Modem, 16550" }, { 0x07, 0x03, 0x03, "Hayes Compatible Modem, 16650" }, { 0x07, 0x03, 0x04, "Hayes Compatible Modem, 16750" }, { 0x07, 0x80, 0x00, "Other" },

{ 0x08, 0x00, 0x00, "PIC" }, { 0x08, 0x00, 0x01, "ISA PIC" }, { 0x08, 0x00, 0x02, "EISA PIC" }, { 0x08, 0x00, 0x10, "I/O APIC" }, { 0x08, 0x00, 0x20, "I/O(x) APIC" }, { 0x08, 0x01, 0x00, "DMA" }, { 0x08, 0x01, 0x01, "ISA DMA" }, { 0x08, 0x01, 0x02, "EISA DMA" }, { 0x08, 0x02, 0x00, "Timer" }, { 0x08, 0x02, 0x01, "ISA Timer" }, { 0x08, 0x02, 0x02, "EISA Timer" }, { 0x08, 0x03, 0x00, "RTC" }, { 0x08, 0x03, 0x00, "ISA RTC" }, { 0x08, 0x03, 0x00, "Hot-Plug" }, { 0x08, 0x80, 0x00, "Other" },

{ 0x09, 0x00, 0x00, "Keyboard" }, { 0x09, 0x01, 0x00, "Pen" }, { 0x09, 0x02, 0x00, "Mouse" }, { 0x09, 0x03, 0x00, "Scanner" }, { 0x09, 0x04, 0x00, "Game Port" }, { 0x09, 0x80, 0x00, "Other" },

{ 0x0a, 0x00, 0x00, "Generic" }, { 0x0a, 0x80, 0x00, "Other" },

{ 0x0b, 0x00, 0x00, "386" }, { 0x0b, 0x01, 0x00, "486" }, { 0x0b, 0x02, 0x00, "Pentium" }, { 0x0b, 0x03, 0x00, "PentiumPro" }, { 0x0b, 0x10, 0x00, "DEC Alpha" }, { 0x0b, 0x20, 0x00, "PowerPC" }, { 0x0b, 0x30, 0x00, "MIPS" }, { 0x0b, 0x40, 0x00, "Coprocessor" }, { 0x0b, 0x80, 0x00, "Other" },

{ 0x0c, 0x00, 0x00, "FireWire" }, { 0x0c, 0x00, 0x10, "OHCI FireWire" }, { 0x0c, 0x01, 0x00, "Access.bus" }, { 0x0c, 0x02, 0x00, "SSA" }, { 0x0c, 0x03, 0x00, "USB (UHCI)" }, { 0x0c, 0x03, 0x10, "USB (OHCI)" }, { 0x0c, 0x03, 0x80, "USB" }, { 0x0c, 0x03, 0xFE, "USB Device" }, { 0x0c, 0x04, 0x00, "Fiber" }, { 0x0c, 0x05, 0x00, "SMBus Controller" }, { 0x0c, 0x06, 0x00, "InfiniBand" }, { 0x0c, 0x80, 0x00, "Other" },

{ 0x0d, 0x00, 0x00, "iRDA" }, { 0x0d, 0x01, 0x00, "Consumer IR" }, { 0x0d, 0x10, 0x00, "RF" }, { 0x0d, 0x80, 0x00, "Other" },

{ 0x0e, 0x00, 0x00, "I2O" }, { 0x0e, 0x80, 0x00, "Other" },

{ 0x0f, 0x01, 0x00, "TV" }, { 0x0f, 0x02, 0x00, "Audio" }, { 0x0f, 0x03, 0x00, "Voice" }, { 0x0f, 0x04, 0x00, "Data" }, { 0x0f, 0x80, 0x00, "Other" },

{ 0x10, 0x00, 0x00, "Network" }, { 0x10, 0x10, 0x00, "Entertainment" }, { 0x10, 0x80, 0x00, "Other" },

{ 0x11, 0x00, 0x00, "DPIO Modules" }, { 0x11, 0x01, 0x00, "Performance Counters" }, { 0x11, 0x10, 0x00, "Comm Sync, Time+Frequency Measurement" }, { 0x11, 0x80, 0x00, "Other" },

};

uInt32 pciRead(int bus, int dev, int func, int reg, int bytes) {
  uInt16 base;

  union {
    struct confadd c;
    uInt32 n;
  } u;

  u.n = 0;
  u.c.enable = 1;
  u.c.rsvd = 0;
  u.c.bus = bus;
  u.c.dev = dev;
  u.c.func = func;
  u.c.reg = reg & 0xFC;

  outportDWord(0xCF8, u.n);

  base = 0xCFC + (reg & 0x03);

  switch (bytes) {
    case 1:
      return (inportByte(base));
    case 2:
      return (inportWord(base));
    case 4:
      return (inportDWord(base));
    default:
      return 0;
  }
}

void pciWrite(int bus, int dev, int func, int reg, uInt32 v, int bytes) {
  uInt16 base;

  union {
    struct confadd c;
    uInt32 n;
  } u;

  u.n = 0;
  u.c.enable = 1;
  u.c.rsvd = 0;
  u.c.bus = bus;
  u.c.dev = dev;
  u.c.func = func;
  u.c.reg = reg & 0xFC;

  base = 0xCFC + (reg & 0x03);
  outportDWord(0xCF8, u.n);
  switch (bytes) {
    case 1:
      outportByte(base, (uInt8) v);
    break;
    case 2:
      outportWord(base, (uInt16) v);
    break;
    case 4:
      outportDWord(base, v);
    break;
  }
}

uint32_t pciProbe(int bus, int dev, int func) {
  struct pciConfig *cfg = 0x0;
  int i;

  cfg = kmalloc(sizeof(struct pciConfig));
  memset(cfg, 0x0, sizeof(struct pciConfig));

  uint32_t *word = (uint32_t *) cfg;

  for (i = 0; i < 4; i++) {
    word[i] = pciRead(bus, dev, func, 4 * i, 4);

    /* This is TEMPORARY */
    if (cfg->vendorID == 0x1022 && i == 1) {
      kprintf("got it: 0x%X", word[i]);
      word[i] &= 0xffff0000;
      word[i] |= 0x5; //0x1 //0x5;
      pciWrite(bus, dev, func, 4 * i, word[i], 4);
      kprintf("set it: 0x%X\n", word[i]);
    }
  }

  if (cfg->vendorID == 0xffff) {
    kfree(cfg);
    return 0x0;
  }

  if (cfg->vendorID == 0x0) {
    kfree(cfg);
    return 0x0;
  }

  cfg->bus = bus;
  cfg->dev = dev;
  cfg->func = func;

  /*
   if (cfg->vendorID == 0x1022)
   pciWrite(bus, dev, func, 0x3C, 0x5,1);
   */

  switch (cfg->headerType & 0x7F) {
    case 0x0: /* normal device */
      for (i = 4; i <= 16; i++) {
        word[i] = pciRead(bus, dev, func, 4 * i, 4);
      }
      /* BAR size discovery: write all-ones, read back mask, restore. */
      for (i = 0; i < 6; i++) {
        uint32_t orig = cfg->bar[i];
        if (!orig) {
          cfg->barSize[i] = 0;
          continue;
        }
        pciWrite(bus, dev, func, 0x10 + i * 4, 0xFFFFFFFFu, 4);
        uint32_t mask = pciRead(bus, dev, func, 0x10 + i * 4, 4);
        pciWrite(bus, dev, func, 0x10 + i * 4, orig, 4);
        if (orig & 1u)
          cfg->barSize[i] = (~(mask & ~3u) + 1u) & 0xFFFFu; /* I/O */
        else
          cfg->barSize[i] = ~(mask & ~15u) + 1u;             /* memory */
      }
      if (cfg->vendorID == 0x1022) {
        kprintf("Device Info: /bus/pci/%d/%d/%d\n", bus, dev, func);
        kprintf("  * Vendor: %X   Device: %X  Class/SubClass/Interface %X/%X/%X\n", cfg->vendorID, cfg->deviceID, cfg->classCode, cfg->subClass, cfg->progIf);
        kprintf("  * Status: %X  Command: %X  BIST/Type/Lat/CLS: %X/%X/%X/%X\n", cfg->status, cfg->command, cfg->bist, cfg->headerType, cfg->latencyTimer, cfg->cacheLineSize);
        kprintf("  * IRQ: 0x%X.0x%X, BAR[0]: 0x%X\n", cfg->intLine, cfg->intPin, cfg->bar[0]);
      }
    break;
    case 0x1:
      kprintf("  * PCI <-> PCI Bridge\n");
    break;
    case 0x2:
      kprintf("  * PCI <-> CardBus Bridge\n");
    break;
    default:
      kprintf("  * Unknown Header Type\n");
    break;

  }

  /*
   switch (cfg->headerType & 0x7F) {
   case 0: // normal device
   for (i = 0; i < 6; i++) {
   v = pciRead(bus, dev, func, i * 4 + 0x10, 4);
   if (v) {
   int v2;
   pciWrite(bus, dev, func, i * 4 + 0x10, 0xffffffff, 4);
   v2 = pciRead(bus, dev, func, i * 4 + 0x10, 4) & 0xfffffff0;
   pciWrite(bus, dev, func, i * 4 + 0x10, v, 4);
   v2 = 1 + ~v2;
   if (v & 1) {
   cfg->base[i] = v & 0xffff;
   cfg->size[i] = v2 & 0xffff;
   }
   else {
   cfg->base[i] = v;
   cfg->size[i] = v2;
   }
   }
   else {
   cfg->base[i] = 0;
   cfg->size[i] = 0;
   }
   }
   v = pciRead(bus, dev, func, 0x3c, 1);
   cfg->irq = (v == 0xff ? 0 : v);
   v = pciRead(bus, dev, func, 0x40, 1);
   cfg->irqLine = (v == 0xff ? 0 : v);
   break;
   case 1:


   }
   */

  return ((uint32_t) cfg);
}

static struct ubx_driver * const pci_drv_table[] = {
	&e1000_ubx_driver,
	&ide_ubx_driver,
	&lnc_ubx_driver,
	NULL,
};

/*
 * Populate an ubx_device from a probed pciConfig.
 * Records all valid BARs and the interrupt line as resources.
 * The pciConfig is freed after this call; do not use it afterwards.
 */
static struct ubx_device *
pci_device_from_cfg(struct pciConfig *cfg)
{
	struct ubx_device *dev;
	struct ubx_resource *res;
	char nameunit[32];
	int i;

	snprintf(nameunit, sizeof(nameunit), "pci%u.%u.%u",
	    cfg->bus, cfg->dev, cfg->func);

	dev = ubx_device_alloc(NULL, nameunit);
	if (dev == NULL) {
		kfree(cfg);
		return (NULL);
	}

	dev->dev_vendor    = cfg->vendorID;
	dev->dev_device_id = cfg->deviceID;
	dev->dev_class     = cfg->classCode;
	dev->dev_subclass  = cfg->subClass;
	dev->dev_progif    = cfg->progIf;
	dev->dev_bus       = cfg->bus;
	dev->dev_slot      = cfg->dev;
	dev->dev_func      = cfg->func;

	/* Record BARs as typed resources (not yet mapped). */
	for (i = 0; i < 6 && dev->dev_nres < UBX_MAX_RESOURCES; i++) {
		if (cfg->bar[i] == 0)
			continue;
		res = &dev->dev_res[dev->dev_nres++];
		if (cfg->bar[i] & 1u) {
			res->r_type  = UBX_RES_IOPORT;
			res->r_start = cfg->bar[i] & ~3u;
		} else {
			res->r_type  = UBX_RES_MEMORY;
			res->r_start = cfg->bar[i] & ~0xFu;
		}
		res->r_size  = cfg->barSize[i];
		res->r_vaddr = NULL;
	}

	/* Record interrupt line. */
	if (cfg->intLine != 0 && cfg->intLine != 0xFF &&
	    dev->dev_nres < UBX_MAX_RESOURCES) {
		res = &dev->dev_res[dev->dev_nres++];
		res->r_type  = UBX_RES_IRQ;
		res->r_start = cfg->intLine;
		res->r_size  = 0;
		res->r_vaddr = NULL;
	}

	kfree(cfg);
	return (dev);
}

int pci_init() {
  uint16_t bus, dev, func;
  int i;

  struct pciConfig *pcfg;
  struct ubx_device *udev;

  for (bus = 0x0; bus < 0x2; bus++) {
    for (dev = 0; dev < 32; dev++) {
      int multifunction = 0;
      for (func = 0; func < 8; func++) {
        /* Only scan functions 1-7 if function 0 advertised multi-function. */
        if (func != 0 && !multifunction)
          break;

        pcfg = (struct pciConfig *) pciProbe(bus, dev, func);
        if (pcfg == NULL) {
          if (func == 0) break; /* no device here at all */
          continue;
        }

        if (func == 0)
          multifunction = (pcfg->headerType & 0x80) ? 1 : 0;

        for (i = 0x0; i < (int)countof(pciClasses); i++) {
          if (pcfg->classCode == pciClasses[i].baseClass &&
              pcfg->subClass  == pciClasses[i].subClass  &&
              pcfg->progIf    == pciClasses[i].interface) {
            kprintf("pci: %u:%u.%u %04X:%04X %s IRQ %u\n",
                pcfg->bus, pcfg->dev, pcfg->func,
                pcfg->vendorID, pcfg->deviceID,
                pciClasses[i].name, pcfg->intLine);
            break;
          }
        }

        udev = pci_device_from_cfg(pcfg); /* pcfg freed inside */
        if (udev == NULL)
          continue;

        ubx_bus_probe_and_attach(udev, pci_drv_table);
      }
    }
  }
  return (0x0);
}
