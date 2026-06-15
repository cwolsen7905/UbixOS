/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 minimal PCI enumeration (Phase 5c).  Just enough of legacy PCI
 * configuration mechanism #1 (the 0xCF8/0xCFC port pair) to find a device by
 * vendor/device id and read its BARs — the x86_64 transport discovery the
 * virtio-blk-pci driver needs (aarch64's virtio lives at a fixed MMIO slot, so it
 * has no PCI; on the PC, virtio is a PCI device).  Full bus enumeration + a real
 * newbus-style attach come later.
 */

#include "../x86_64.h"
#include "pci.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

/**
 * Read a 32-bit PCI config-space dword.  @off must be dword-aligned.  Mechanism
 * #1: write the (enable|bus|dev|fn|off) selector to 0xCF8, read 0xCFC.
 */
u32 pci_config_read32(u8 bus, u8 dev, u8 fn, u8 off)
{
	u32 addr = 0x80000000U | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)fn << 8) | (off & 0xFC);
	outl(PCI_CONFIG_ADDR, addr);
	return inl(PCI_CONFIG_DATA);
}

/** Write a 32-bit PCI config-space dword (@off dword-aligned). */
void pci_config_write32(u8 bus, u8 dev, u8 fn, u8 off, u32 val)
{
	u32 addr = 0x80000000U | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)fn << 8) | (off & 0xFC);
	outl(PCI_CONFIG_ADDR, addr);
	outl(PCI_CONFIG_DATA, val);
}

/**
 * Scan bus 0..255 / dev 0..31 / fn 0 for the first device matching @vendor /
 * @device, filling @out (location, BAR0, IRQ line, command register).
 *
 * @return 1 if found, 0 otherwise.
 */
int pci_find_device(u16 vendor, u16 device, struct pci_dev *out)
{
	for (unsigned bus = 0; bus < 256; bus++)
	{
		for (unsigned dev = 0; dev < 32; dev++)
		{
			u32 id = pci_config_read32((u8)bus, (u8)dev, 0, 0x00);
			u16 ven = (u16)(id & 0xFFFF);
			u16 did = (u16)(id >> 16);

			if (ven == 0xFFFF)
				continue; /* no device in this slot */
			if (ven != vendor)
				continue;
			if (device != 0xFFFF && did != device)
				continue;

			out->bus = (u8)bus;
			out->dev = (u8)dev;
			out->fn = 0;
			out->vendor = ven;
			out->device = did;
			out->bar[0] = pci_config_read32((u8)bus, (u8)dev, 0, 0x10);
			out->bar[1] = pci_config_read32((u8)bus, (u8)dev, 0, 0x14);
			out->irq_line = (u8)(pci_config_read32((u8)bus, (u8)dev, 0, 0x3C) & 0xFF);
			out->command = (u16)(pci_config_read32((u8)bus, (u8)dev, 0, 0x04) & 0xFFFF);
			return 1;
		}
	}
	return 0;
}

/** Set the bus-master + I/O-space enable bits in the device's command register. */
void pci_enable_io_busmaster(struct pci_dev *d)
{
	u32 cmd = pci_config_read32(d->bus, d->dev, d->fn, 0x04);
	cmd |= 0x1 | 0x4; /* I/O space enable | bus master enable */
	pci_config_write32(d->bus, d->dev, d->fn, 0x04, cmd);
}
