/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 minimal PCI interface (Phase 5c).  See pci.c.
 */
#ifndef _X86_64_DEV_PCI_H
#define _X86_64_DEV_PCI_H

#include "../x86_64.h"

struct pci_dev
{
	u8 bus, dev, fn;
	u16 vendor, device;
	u32 bar[2]; /* BAR0/BAR1 raw values (low bit set => I/O space) */
	u8 irq_line;
	u16 command;
};

u32 pci_config_read32(u8 bus, u8 dev, u8 fn, u8 off);
void pci_config_write32(u8 bus, u8 dev, u8 fn, u8 off, u32 val);
int pci_find_device(u16 vendor, u16 device, struct pci_dev *out);
void pci_enable_io_busmaster(struct pci_dev *d);

#endif /* _X86_64_DEV_PCI_H */
