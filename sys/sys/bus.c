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

#include <sys/bus.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <isa/8259.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

struct ubx_device *
ubx_device_alloc(struct ubx_device *parent, const char *nameunit)
{
	struct ubx_device *dev;

	dev = kmalloc(sizeof(*dev));
	if (dev == NULL) {
		kprintf("ubx_device_alloc: out of memory\n");
		return (NULL);
	}

	memset(dev, 0, sizeof(*dev));
	dev->dev_parent = parent;

	if (nameunit != NULL) {
		strncpy(dev->dev_nameunit, nameunit,
		    sizeof(dev->dev_nameunit) - 1);
	}

	/* Link as first child of parent. */
	if (parent != NULL) {
		dev->dev_sibling = parent->dev_children;
		parent->dev_children = dev;
	}

	return (dev);
}

void
ubx_device_free(struct ubx_device *dev)
{
	if (dev != NULL)
		kfree(dev);
}

volatile void *
ubx_alloc_memory(struct ubx_device *dev, u_int32_t phys_base, u_int32_t size)
{
	struct ubx_resource *res;
	u_int32_t pages, i, phys, virt;

	if (dev->dev_nres >= UBX_MAX_RESOURCES) {
		kprintf("%s: ubx_alloc_memory: resource table full\n",
		    dev->dev_nameunit);
		return (NULL);
	}

	pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	for (i = 0; i < pages; i++) {
		phys = phys_base + i * PAGE_SIZE;
		if (vmm_remap_io_page(phys,
		    KERNEL_PAGE_DEFAULT | PAGE_CACHE_DISABLED, sysID) != 0) {
			kprintf("%s: ubx_alloc_memory: remap failed phys=0x%X\n",
			    dev->dev_nameunit, phys);
			return (NULL);
		}
	}

	/* On UbixOS, vmm_remap_io_page identity-maps phys == virt. */
	virt = phys_base;

	res = &dev->dev_res[dev->dev_nres++];
	res->r_type   = UBX_RES_MEMORY;
	res->r_start  = phys_base;
	res->r_size   = size;
	res->r_vaddr  = (volatile void *)(uintptr_t)virt;

	return (res->r_vaddr);
}

int
ubx_alloc_ioport(struct ubx_device *dev, u_int32_t base, u_int32_t size)
{
	struct ubx_resource *res;

	if (dev->dev_nres >= UBX_MAX_RESOURCES) {
		kprintf("%s: ubx_alloc_ioport: resource table full\n",
		    dev->dev_nameunit);
		return (-1);
	}

	res = &dev->dev_res[dev->dev_nres++];
	res->r_type  = UBX_RES_IOPORT;
	res->r_start = base;
	res->r_size  = size;
	res->r_vaddr = NULL;

	return (0);
}

int
ubx_alloc_irq(struct ubx_device *dev, u_int8_t irq, void (*isr)(void))
{
	struct ubx_resource *res;
	u_int8_t vec;

	if (dev->dev_nres >= UBX_MAX_RESOURCES) {
		kprintf("%s: ubx_alloc_irq: resource table full\n",
		    dev->dev_nameunit);
		return (-1);
	}

	/* Map IRQ number to IDT vector: IRQ 0-7 on master (mVec), 8-15 on slave (sVec). */
	if (irq < 8)
		vec = mVec + irq;
	else
		vec = sVec + (irq - 8);

	setVector(isr, vec, dPresent + dInt + dDpl0);
	irqEnable(irq);

	res = &dev->dev_res[dev->dev_nres++];
	res->r_type  = UBX_RES_IRQ;
	res->r_start = irq;
	res->r_size  = 0;
	res->r_vaddr = NULL;

	return (0);
}

#define UBX_BLK_MAXDEV 32

struct ubx_blk_entry {
	int			 major;
	int			 minor;
	struct ubx_device	*dev;
};

static struct ubx_blk_entry	blk_table[UBX_BLK_MAXDEV];
static int			blk_ndev;

int
ubx_device_register_block(struct ubx_device *dev, int major, int minor,
    struct ubx_blk_ops *ops)
{
	if (dev == NULL || blk_ndev >= UBX_BLK_MAXDEV)
		return (-1);
	dev->dev_blk_ops = ops;
	blk_table[blk_ndev].major = major;
	blk_table[blk_ndev].minor = minor;
	blk_table[blk_ndev].dev  = dev;
	blk_ndev++;
	return (0);
}

struct ubx_device *
ubx_device_find(int major, int minor)
{
	int i;

	for (i = 0; i < blk_ndev; i++) {
		if (blk_table[i].major == major && blk_table[i].minor == minor)
			return (blk_table[i].dev);
	}
	return (NULL);
}

int
ubx_bus_probe_and_attach(struct ubx_device *dev,
    struct ubx_driver * const *drivers)
{
	int i;

	for (i = 0; drivers[i] != NULL; i++) {
		if (drivers[i]->drv_probe(dev) == 0) {
			dev->dev_driver = drivers[i];
			if (drivers[i]->drv_attach != NULL)
				return (drivers[i]->drv_attach(dev));
			return (0);
		}
	}

	return (-1);
}
