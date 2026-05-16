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

#ifndef _SYS_BUS_H
#define _SYS_BUS_H

#include <sys/types.h>

/*
 * Resource types for ubx_resource.r_type.
 */
#define UBX_RES_IRQ    1
#define UBX_RES_MEMORY 2
#define UBX_RES_IOPORT 3

#define UBX_MAX_RESOURCES 6

/*
 * One claimed resource (IRQ, MMIO BAR, or I/O port range) attached to a device.
 * Analogous to FreeBSD's struct resource.
 */
struct ubx_resource {
	int		 r_type;	/* UBX_RES_IRQ / _MEMORY / _IOPORT */
	uint32_t	 r_start;	/* physical base address or IRQ number */
	uint32_t	 r_size;	/* byte size; 0 for IRQ entries */
	volatile void	*r_vaddr;	/* kernel virtual address (MEMORY only) */
};

/*
 * Forward declaration — ubx_driver references ubx_device and vice versa.
 */
struct ubx_device;

/*
 * Driver descriptor registered per bus.
 * Analogous to FreeBSD's driver_t.
 *
 * drv_probe:  examine dev and return 0 on match, -1 on no match.
 * drv_attach: finish initialisation; claim resources here.
 * drv_detach: optional teardown; may be NULL.
 */
struct ubx_driver {
	const char	*drv_name;
	int		(*drv_probe)(struct ubx_device *);
	int		(*drv_attach)(struct ubx_device *);
	int		(*drv_detach)(struct ubx_device *);
};

/*
 * One device instance on any bus.
 * Analogous to FreeBSD's struct device (device_t is struct device *).
 *
 * dev_blk_ops is reserved for Phase 2d block-device registration; NULL until then.
 */
struct ubx_device {
	struct ubx_device	*dev_parent;		/* owning bus; NULL for root */
	struct ubx_driver	*dev_driver;		/* matched driver; NULL before probe */
	void			*dev_softc;		/* driver private state */
	void			*dev_blk_ops;		/* block ops (Phase 2d); NULL for now */
	char			 dev_nameunit[32];	/* e.g. "uhci0", "ata0" */

	struct ubx_resource	 dev_res[UBX_MAX_RESOURCES];
	int			 dev_nres;

	/* Bus-specific identification used during probe/match. */
	uint16_t		 dev_vendor;
	uint16_t		 dev_device_id;
	uint8_t			 dev_class;
	uint8_t			 dev_subclass;
	uint8_t			 dev_progif;
	uint8_t			 dev_bus;
	uint8_t			 dev_slot;
	uint8_t			 dev_func;
	uint8_t			 _pad[2];

	/* Linked list of children of this bus device. */
	struct ubx_device	*dev_children;
	struct ubx_device	*dev_sibling;
};

/*
 * Allocate and zero a new device node.
 * parent may be NULL for root/platform devices.
 * nameunit is copied into dev_nameunit (truncated at 31 chars).
 */
struct ubx_device	*ubx_device_alloc(struct ubx_device *parent,
			    const char *nameunit);

/*
 * Release a device allocated by ubx_device_alloc.
 * Does not recurse into children.
 */
void			 ubx_device_free(struct ubx_device *dev);

/*
 * Claim an MMIO region for dev.
 * Maps [phys_base, phys_base+size) into kernel virtual space using
 * vmm_remapPage with PAGE_CACHE_DISABLED, records in dev->dev_res[].
 * Returns the kernel virtual base address, or NULL on failure.
 */
volatile void		*ubx_alloc_memory(struct ubx_device *dev,
			    uint32_t phys_base, uint32_t size);

/*
 * Claim an I/O port range for dev.
 * Records in dev->dev_res[]; no hardware operation needed on x86.
 * Returns 0 on success, -1 if dev_res[] is full.
 */
int			 ubx_alloc_ioport(struct ubx_device *dev,
			    uint32_t base, uint32_t size);

/*
 * Claim an IRQ for dev.
 * Calls setVector(irq, isr) and irqEnable(irq).
 * Records in dev->dev_res[].
 * Returns 0 on success, -1 on failure.
 */
int			 ubx_alloc_irq(struct ubx_device *dev, uint8_t irq,
			    void (*isr)(void));

/*
 * Walk a NULL-terminated driver table, calling drv_probe() on each entry
 * for the supplied device.  On the first match, calls drv_attach().
 * Returns 0 if a driver was attached, -1 if no driver matched.
 */
int			 ubx_bus_probe_and_attach(struct ubx_device *dev,
			    struct ubx_driver * const *drivers);

#endif /* _SYS_BUS_H */
