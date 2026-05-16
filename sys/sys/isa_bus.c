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

#include <sys/isa_bus.h>
#include <isa/atkbd.h>
#include <isa/mouse.h>

/*
 * Static resource declarations.
 * i8259 and PIT are NOT listed here — they run directly from init_tasks[] before
 * idt_init and sched_init.  Drivers below require an operational IDT and scheduler.
 */
static const struct isa_res_decl atkbd_isa_res[] = {
	{ UBX_RES_IOPORT, 0x60, 4 },
	{ UBX_RES_IRQ,    1,    0 },
	{ 0 }
};

static const struct isa_res_decl mouse_isa_res[] = {
	/* PS/2 mouse shares I/O port 0x60 with the keyboard — hardware reality */
	{ UBX_RES_IOPORT, 0x60, 4 },
	{ UBX_RES_IRQ,    12,   0 },
	{ 0 }
};

/*
 * ISA driver table.  Order is initialization order.
 * fdc and ne2k are compiled out but wired up in their own source files;
 * uncomment here when the hardware is present.
 */
static const struct isa_drv_entry isa_drv_table[] = {
	{ &atkbd_ubx_driver, atkbd_isa_res },
	{ &mouse_ubx_driver, mouse_isa_res },
	/* { &fdc_ubx_driver,  fdc_isa_res  }, */
	/* { &ne2k_ubx_driver, ne2k_isa_res }, */
	{ NULL, NULL }
};

int
isa_bus_init(void)
{
	const struct isa_drv_entry	*ent;
	const struct isa_res_decl	*res;
	struct ubx_driver		*single[2];
	struct ubx_device		*dev;
	int				 i;

	for (ent = isa_drv_table; ent->ide_driver != NULL; ent++) {
		dev = ubx_device_alloc(NULL, ent->ide_driver->drv_name);
		if (dev == NULL)
			continue;

		/* Populate dev_res[] from the static declaration array */
		if (ent->ide_res != NULL) {
			for (res = ent->ide_res; res->ir_type != 0; res++) {
				i = dev->dev_nres;
				if (i >= UBX_MAX_RESOURCES)
					break;
				dev->dev_res[i].r_type  = res->ir_type;
				dev->dev_res[i].r_start = res->ir_start;
				dev->dev_res[i].r_size  = res->ir_size;
				dev->dev_nres++;
			}
		}

		/* Each table entry has exactly one driver; wrap it for the generic API */
		single[0] = ent->ide_driver;
		single[1] = NULL;
		ubx_bus_probe_and_attach(dev, single);
	}

	return (0);
}
