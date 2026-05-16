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

#ifndef _SYS_ISA_BUS_H
#define _SYS_ISA_BUS_H

#include <sys/bus.h>

/*
 * Static resource declaration for an ISA device.
 * ISA has no config space; resources are declared by the driver itself.
 * Terminate the array with an entry whose ir_type == 0.
 */
struct isa_res_decl {
	int		 ir_type;	/* UBX_RES_IOPORT, UBX_RES_IRQ, or 0 (terminator) */
	uint32_t	 ir_start;	/* I/O base address or IRQ number */
	uint32_t	 ir_size;	/* range size in bytes; 0 for IRQ entries */
};

/*
 * ISA driver table entry.
 * isa_bus_init() allocates one ubx_device per entry, fills dev_res[] from
 * ide_res[], then calls ubx_bus_probe_and_attach() against the single driver.
 */
struct isa_drv_entry {
	struct ubx_driver		*ide_driver;	/* the driver; must be non-NULL */
	const struct isa_res_decl	*ide_res;	/* static resource list; may be NULL */
};

/*
 * Initialize the ISA pseudo-bus.
 * Iterates isa_drv_table[] (defined in sys/sys/isa_bus.c), allocates an
 * ubx_device per entry, and probe+attaches the associated driver.
 *
 * ORDERING NOTE: i8259_init and pit_init run directly from init_tasks[] before
 * this function.  isa_bus_init() must run after idt_init and sched_init so that
 * drivers which install ISRs (atkbd, mouse) find a valid interrupt descriptor table.
 */
int	isa_bus_init(void);

#endif /* _SYS_ISA_BUS_H */
