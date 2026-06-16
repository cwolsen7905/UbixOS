/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 framebuffer via the Bochs/QEMU "std" VGA (the Bochs VBE DISPI
 * interface).  This is the display platform for the desktop: a live linear
 * framebuffer (B8G8R8X8, 0x00RRGGBB LE — the layout objGFX/views expect), exactly
 * like the i386 VESA LFB (sys/kern/fb.c).  Unlike aarch64's virtio-gpu (a
 * RAM-backed buffer pushed to the host with a transfer+flush), the LFB is the live
 * scanout: a write appears immediately, so presenting a frame is a no-op.
 *
 * The mode is programmed through the DISPI index/data I/O ports (no MMIO mapping
 * needed); the LFB's physical base is the device's PCI BAR0.  sys_mapfb maps that
 * BAR straight into the compositor's address space — the kernel itself never
 * touches the framebuffer, so the BAR (above the kernel's 1 GB physmap) needs no
 * kernel mapping.
 */

#include "../x86_64.h"
#include "pci.h"

/* QEMU/Bochs "std" VGA PCI identity (BAR0 = linear framebuffer). */
#define VGA_VENDOR 0x1234
#define VGA_DEVICE 0x1111

/* Bochs VBE DISPI registers (index port 0x1CE, data port 0x1CF). */
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF
#define VBE_DISPI_INDEX_XRES 1
#define VBE_DISPI_INDEX_YRES 2
#define VBE_DISPI_INDEX_BPP 3
#define VBE_DISPI_INDEX_ENABLE 4
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_LFB_ENABLED 0x40

/* Exported to the display syscalls (sys_mapfb / sys_fbpresent). */
u64 x86_64_fb_phys; /* physical base of the LFB (BAR0)        */
u32 x86_64_fb_width;
u32 x86_64_fb_height;
u32 x86_64_fb_pitch; /* bytes per row (width * 4)             */

static void dispi_write(u16 index, u16 val)
{
	outw(VBE_DISPI_IOPORT_INDEX, index);
	outw(VBE_DISPI_IOPORT_DATA, val);
}

/**
 * Present a frame.  The Bochs LFB is the live scanout (writes appear at once), so
 * there is nothing to push — this exists only to match the virtio-gpu/aarch64
 * sys_fbpresent contract the compositor calls each frame.
 */
void x86_64_fb_present(void)
{
}

/**
 * Find the std VGA, program a @w x @h x 32 LFB mode via DISPI, and record the
 * LFB physical base (BAR0) + geometry for sys_mapfb.  @return 0 on success, -1 if
 * the device is absent.
 */
int x86_64_fb_init(void)
{
	struct pci_dev d;
	u32 cmd;
	u32 w = 1280, h = 720; /* 16:9; 1280x720x4 = 3.7 MB, well within the 16 MB std-VGA LFB */

	if (!pci_find_device(VGA_VENDOR, VGA_DEVICE, &d))
	{
		serial_puts("vga-fb: no std VGA PCI device (1234:1111) — run with -vga std\n");
		return -1;
	}
	/* BAR0 is a 32-bit memory BAR: the linear framebuffer.  Mask the low 4 flag
	 * bits to get its physical base. */
	if (d.bar[0] & 0x1)
	{
		serial_puts("vga-fb: BAR0 is I/O, not the memory LFB\n");
		return -1;
	}
	x86_64_fb_phys = (u64)(d.bar[0] & ~0xFU);

	/* Enable memory-space decode + bus master so the LFB BAR responds (we boot via
	 * -kernel with no firmware to do PCI setup). */
	cmd = pci_config_read32(d.bus, d.dev, d.fn, 0x04);
	cmd |= 0x2 | 0x4; /* memory space | bus master */
	pci_config_write32(d.bus, d.dev, d.fn, 0x04, cmd);

	/* Program the mode: disable, set X/Y/BPP, re-enable with the linear FB. */
	dispi_write(VBE_DISPI_INDEX_ENABLE, 0);
	dispi_write(VBE_DISPI_INDEX_XRES, (u16)w);
	dispi_write(VBE_DISPI_INDEX_YRES, (u16)h);
	dispi_write(VBE_DISPI_INDEX_BPP, 32);
	dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

	x86_64_fb_width = w;
	x86_64_fb_height = h;
	x86_64_fb_pitch = w * 4;

	serial_puts("vga-fb: std VGA LFB ");
	serial_putdec(w);
	serial_puts("x");
	serial_putdec(h);
	serial_puts("x32 @phys=");
	serial_puthex(x86_64_fb_phys);
	serial_puts("\n");
	return 0;
}
