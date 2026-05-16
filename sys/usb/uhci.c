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

/*
 * UHCI (Universal Host Controller Interface) driver for UbixOS.
 * Supports QEMU's PIIX4 UHCI (vendor 0x8086, device 0x7020, class 0x0C/0x03/0x00).
 * Provides control, bulk, and interrupt transfer primitives for the USB core (Phase 5).
 */

#include <usb/uhci.h>
#include <usb/usb.h>
#include <sys/bus.h>
#include <sys/dma_mem.h>
#include <sys/io.h>
#include <sys/idt.h>
#include <sys/gdt.h>
#include <isa/8259.h>
#include <isa/irq.h>
#include <pci/pci.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * PCI identity for Intel PIIX4 UHCI
 * --------------------------------------------------------------------- */
#define UHCI_VENDOR_INTEL 0x8086
#define UHCI_DEVICE_PIIX4 0x7020
#define UHCI_CLASS 0x0C    /* Serial bus */
#define UHCI_SUBCLASS 0x03 /* USB */
#define UHCI_PROGIF 0x00   /* UHCI */

/* -----------------------------------------------------------------------
 * Module-level state (one controller; extend to array for multi-HC)
 * --------------------------------------------------------------------- */
static struct uhci_softc uhci_sc;

/* -----------------------------------------------------------------------
 * Register accessors
 * --------------------------------------------------------------------- */
static inline uint16_t uhci_rd16(struct uhci_softc *sc, uint16_t reg)
{
	return (inportWord(sc->sc_iobase + reg));
}

static inline void uhci_wr16(struct uhci_softc *sc, uint16_t reg, uint16_t val)
{
	outportWord(sc->sc_iobase + reg, val);
}

static inline uint32_t uhci_rd32(struct uhci_softc *sc, uint16_t reg)
{
	return (inportDWord(sc->sc_iobase + reg));
}

static inline void uhci_wr32(struct uhci_softc *sc, uint16_t reg, uint32_t val)
{
	outportDWord(sc->sc_iobase + reg, val);
}

/* -----------------------------------------------------------------------
 * Busy-wait ~N ms by burning I/O reads (each ~1 µs on QEMU)
 * --------------------------------------------------------------------- */
static void uhci_delay_ms(int ms)
{
	int i;

	for (i = 0; i < ms * 1000; i++)
		inportByte(0x80);
}

/* -----------------------------------------------------------------------
 * Slab helpers — allocate QH/TD from pre-allocated DMA page
 * --------------------------------------------------------------------- */
static struct uhci_qh *uhci_alloc_qh(struct uhci_softc *sc)
{
	struct uhci_qh *qh;

	if (sc->sc_qh_used >= UHCI_QH_POOL_COUNT)
		return (NULL);
	qh = &sc->sc_qh_pool[sc->sc_qh_used++];
	memset(qh, 0, sizeof(*qh));
	qh->qh_link = UHCI_PTR_T;
	qh->qh_elt = UHCI_PTR_T;
	return (qh);
}

static struct uhci_td *uhci_alloc_td(struct uhci_softc *sc)
{
	struct uhci_td *td;

	if (sc->sc_td_used >= UHCI_TD_POOL_COUNT)
		return (NULL);
	td = &sc->sc_td_pool[sc->sc_td_used++];
	memset(td, 0, sizeof(*td));
	td->td_link = UHCI_PTR_T;
	return (td);
}

static void uhci_free_td_chain(struct uhci_softc *sc, struct uhci_td *td)
{
	/* Simple slab: reset used count back (only safe for synchronous transfers) */
	(void)sc;
	(void)td;
	/* Phase 5 will use a proper free-list; for now the pool is reset per-transfer */
}

/* -----------------------------------------------------------------------
 * ISR trampoline (naked — saves/restores context; calls uhci_isr)
 * --------------------------------------------------------------------- */
void uhci_isr_handler(void);

asm(".globl uhci_isr_trampoline   \n"
    "uhci_isr_trampoline:         \n"
    "  pusha                      \n"
    "  push %ss                   \n"
    "  push %ds                   \n"
    "  push %es                   \n"
    "  push %fs                   \n"
    "  push %gs                   \n"
    "  call uhci_isr_handler      \n"
    "  pop  %gs                   \n"
    "  pop  %fs                   \n"
    "  pop  %es                   \n"
    "  pop  %ds                   \n"
    "  pop  %ss                   \n"
    "  popa                       \n"
    "  iret                       \n");

void uhci_isr_handler(void)
{
	struct uhci_softc *sc = &uhci_sc;
	uint16_t sts;

	/* Must read and clear status before any other work to prevent re-fire. */
	sts = uhci_rd16(sc, UHCI_USBSTS);
	uhci_wr16(sc, UHCI_USBSTS, sts);

	if (sts & (UHCI_STS_USBINT | UHCI_STS_ERRINT)) {
		int i;
		for (i = 0; i < UHCI_INTR_SLOTS; i++) {
			struct uhci_intr_slot *slot = &sc->sc_intr[i];
			uint32_t actlen_raw;
			int actual;

			if (!slot->is_used)
				continue;
			if (slot->is_td->td_status & UHCI_TD_ACTIVE)
				continue;

			/* TD completed — call driver callback if no error */
			if (!(slot->is_td->td_status & UHCI_TD_STATUS_MASK) &&
			    slot->is_cb != NULL) {
				actlen_raw = (slot->is_td->td_status >> 16) & 0x7FFu;
				actual = (actlen_raw == 0x7FFu) ? 0 : (int)(actlen_raw + 1);
				slot->is_cb(slot->is_arg,
				    (uint8_t *)slot->is_buf.db_vaddr, actual);
			}

			/* Re-arm: toggle DATA, mark active again */
			slot->is_toggle ^= 1;
			slot->is_td->td_token = UHCI_TOKEN(slot->is_maxpkt,
			    slot->is_toggle, slot->is_ep, slot->is_addr,
			    UHCI_PID_IN);
			slot->is_td->td_buffer = slot->is_buf.db_paddr;
			slot->is_td->td_link   = UHCI_PTR_T;
			slot->is_td->td_status = UHCI_TD_ACTIVE | UHCI_TD_IOC |
			    UHCI_TD_ERRCNT(3) | UHCI_TD_SPD;
			slot->is_qh->qh_elt = (uint32_t)slot->is_td;
		}
	}

	if (sts & UHCI_STS_HCERR)
		kprintf("uhci: HC process error\n");
	/* EOI is sent by irq_dispatch() */
}

/* -----------------------------------------------------------------------
 * Root port init — reset each port and detect devices
 * --------------------------------------------------------------------- */
void uhci_root_port_init(struct uhci_softc *sc)
{
	int port, i;
	uint16_t portsc;
	uint16_t portsc_regs[2] = {UHCI_PORTSC1, UHCI_PORTSC2};

	for (port = 0; port < 2; port++)
	{
		uint16_t reg = portsc_regs[port];

		portsc = uhci_rd16(sc, reg);
		if (!(portsc & UHCI_PORTSC_CCS))
		{
			kprintf("uhci: port %d empty\n", port + 1);
			continue;
		}

		kprintf("uhci: port %d device detected (portsc=0x%X)\n", port + 1, portsc);

		/* Assert reset for 50 ms */
		uhci_wr16(sc, reg, uhci_rd16(sc, reg) | UHCI_PORTSC_PR);
		uhci_delay_ms(50);
		uhci_wr16(sc, reg, uhci_rd16(sc, reg) & ~UHCI_PORTSC_PR);
		uhci_delay_ms(10);

		/* Clear status-change bits then enable port */
		uhci_wr16(sc, reg, UHCI_PORTSC_CSC | UHCI_PORTSC_PEC);
		uhci_wr16(sc, reg, uhci_rd16(sc, reg) | UHCI_PORTSC_PE);
		uhci_delay_ms(10);

		for (i = 0; i < 10; i++)
		{
			portsc = uhci_rd16(sc, reg);
			if (portsc & UHCI_PORTSC_PE)
				break;
			uhci_delay_ms(10);
		}

		if (!(portsc & UHCI_PORTSC_PE))
		{
			kprintf("uhci: port %d enable failed\n", port + 1);
			continue;
		}

		kprintf("uhci: port %d enabled (%s speed)\n", port + 1, (portsc & UHCI_PORTSC_LSDA) ? "low" : "full");

		usb_new_device(sc, port,
		    (portsc & UHCI_PORTSC_LSDA) ? USB_SPEED_LOW : USB_SPEED_FULL);
	}
}

/* -----------------------------------------------------------------------
 * Control transfer — synchronous polled
 *
 * Builds SETUP + (optional DATA) + STATUS TD chain, appends to ctl QH,
 * polls until complete, then reclaims TDs.
 * direction: 0=host-to-device (OUT data), 1=device-to-host (IN data).
 * --------------------------------------------------------------------- */
int uhci_control_transfer(struct uhci_softc *sc, uint8_t addr, uint8_t ep, uint8_t *setup_pkt, void *data, uint16_t datalen, int direction)
{
	struct uhci_td *setup_td, *data_td, *status_td, *td;
	struct uhci_qh *qh;
	struct dma_buf setup_buf, data_buf;
	uint32_t qh_phys, setup_phys, data_phys, status_phys;
	uint8_t toggle;
	int i, timeout;

	/* Allocate DMA buffers for SETUP packet (8 bytes) and data stage */
	if (dma_alloc(8, 16, &setup_buf) != 0)
		return (-1);
	memcpy(setup_buf.db_vaddr, setup_pkt, 8);
	setup_phys = setup_buf.db_paddr;

	data_phys = 0;
	memset(&data_buf, 0, sizeof(data_buf));
	if (datalen > 0)
	{
		if (dma_alloc(datalen < 64 ? 64 : datalen, 64, &data_buf) != 0)
		{
			dma_free(&setup_buf);
			return (-1);
		}
		data_phys = data_buf.db_paddr;
		if (!direction)
			memcpy(data_buf.db_vaddr, data, datalen);
	}

	/* Build TD chain */
	sc->sc_td_used = 0; /* reset slab for this synchronous transfer */

	setup_td = uhci_alloc_td(sc);
	if (setup_td == NULL)
	{
		dma_free(&setup_buf);
		dma_free(&data_buf);
		return (-1);
	}
	setup_phys = (uint32_t)setup_td; /* physical == virtual on UbixOS */
	setup_td->td_status = UHCI_TD_ACTIVE | UHCI_TD_ERRCNT(3);
	setup_td->td_token = UHCI_TOKEN(8, 0, ep, addr, UHCI_PID_SETUP);
	setup_td->td_buffer = setup_buf.db_paddr;

	toggle = 1;
	data_td = NULL;
	if (datalen > 0)
	{
		data_td = uhci_alloc_td(sc);
		if (data_td == NULL)
		{
			dma_free(&setup_buf);
			dma_free(&data_buf);
			return (-1);
		}
		data_td->td_status = UHCI_TD_ACTIVE | UHCI_TD_ERRCNT(3);
		data_td->td_token = UHCI_TOKEN(datalen, toggle, ep, addr, direction ? UHCI_PID_IN : UHCI_PID_OUT);
		data_td->td_buffer = data_phys;
		toggle ^= 1;

		setup_td->td_link = (uint32_t)data_td; /* phys == virt */
	}

	status_td = uhci_alloc_td(sc);
	if (status_td == NULL)
	{
		dma_free(&setup_buf);
		dma_free(&data_buf);
		return (-1);
	}
	/* STATUS stage: opposite direction, DATA1, zero length */
	status_td->td_status = UHCI_TD_ACTIVE | UHCI_TD_ERRCNT(3) | UHCI_TD_IOC;
	status_td->td_token = UHCI_TOKEN(0, 1, ep, addr, direction ? UHCI_PID_OUT : UHCI_PID_IN);
	status_td->td_token |= (UHCI_TOKEN_ZERO_LEN << 21);
	status_td->td_buffer = 0;
	status_td->td_link = UHCI_PTR_T;

	if (data_td != NULL)
		data_td->td_link = (uint32_t)status_td;
	else
		setup_td->td_link = (uint32_t)status_td;

	/* Allocate a QH for this transfer and wire it in */
	sc->sc_qh_used = UHCI_CTRL_BASE; /* preserve skeleton + intr slot QHs */
	qh = uhci_alloc_qh(sc);
	if (qh == NULL)
	{
		dma_free(&setup_buf);
		dma_free(&data_buf);
		return (-1);
	}
	qh->qh_elt = (uint32_t)setup_td;
	qh->qh_link = sc->sc_ctl_qh->qh_link; /* insert before bulk QH */
	sc->sc_ctl_qh->qh_link = (uint32_t)qh | UHCI_PTR_QH;

	/* Poll for completion (max 500 ms) */
	timeout = 500;
	while (timeout-- > 0)
	{
		if (!(status_td->td_status & UHCI_TD_ACTIVE))
			break;
		uhci_delay_ms(1);
	}

	/* Remove from ctl QH */
	sc->sc_ctl_qh->qh_link = qh->qh_link;

	if (timeout < 0)
	{
		kprintf("uhci: control transfer timeout (addr=%d ep=%d)\n", addr, ep);
		dma_free(&setup_buf);
		dma_free(&data_buf);
		return (-1);
	}

	if (status_td->td_status & UHCI_TD_STATUS_MASK)
	{
		kprintf("uhci: control transfer error (status=0x%08X)\n", status_td->td_status);
		dma_free(&setup_buf);
		dma_free(&data_buf);
		return (-1);
	}

	if (direction && datalen > 0 && data != NULL)
		memcpy(data, data_buf.db_vaddr, datalen);

	dma_free(&setup_buf);
	dma_free(&data_buf);
	return (0);
}

/* -----------------------------------------------------------------------
 * Bulk transfer — synchronous polled
 * --------------------------------------------------------------------- */
int uhci_bulk_transfer(struct uhci_softc *sc, uint8_t addr, uint8_t ep, void *data, uint16_t datalen, int direction, uint8_t *toggle)
{
	struct uhci_td *td, *first_td, *prev_td;
	struct uhci_qh *qh;
	struct dma_buf buf;
	uint16_t remaining, chunk, offset;
	int timeout;

	if (dma_alloc(datalen < 64 ? 64 : datalen, 64, &buf) != 0)
		return (-1);

	if (!direction)
		memcpy(buf.db_vaddr, data, datalen);

	sc->sc_td_used = UHCI_CTRL_BASE; /* preserve skeleton + intr slot TDs */
	first_td = prev_td = NULL;
	remaining = datalen;
	offset = 0;

	while (remaining > 0)
	{
		chunk = remaining > 64 ? 64 : remaining;
		td = uhci_alloc_td(sc);
		if (td == NULL)
		{
			dma_free(&buf);
			return (-1);
		}
		td->td_status = UHCI_TD_ACTIVE | UHCI_TD_ERRCNT(3);
		if (remaining <= 64)
			td->td_status |= UHCI_TD_IOC;
		td->td_token = UHCI_TOKEN(chunk, *toggle, ep, addr, direction ? UHCI_PID_IN : UHCI_PID_OUT);
		td->td_buffer = buf.db_paddr + offset;
		td->td_link = UHCI_PTR_T;
		*toggle ^= 1;

		if (prev_td != NULL)
			prev_td->td_link = (uint32_t)td;
		else
			first_td = td;
		prev_td = td;
		offset += chunk;
		remaining -= chunk;
	}

	/* Insert in bulk QH */
	sc->sc_qh_used = UHCI_CTRL_BASE;
	qh = uhci_alloc_qh(sc);
	if (qh == NULL)
	{
		dma_free(&buf);
		return (-1);
	}
	qh->qh_elt = (uint32_t)first_td;
	qh->qh_link = sc->sc_bulk_qh->qh_link;
	sc->sc_bulk_qh->qh_link = (uint32_t)qh | UHCI_PTR_QH;

	timeout = 500;
	while (timeout-- > 0)
	{
		if (!(prev_td->td_status & UHCI_TD_ACTIVE))
			break;
		uhci_delay_ms(1);
	}

	sc->sc_bulk_qh->qh_link = qh->qh_link;

	if (timeout < 0)
	{
		kprintf("uhci: bulk transfer timeout\n");
		dma_free(&buf);
		return (-1);
	}

	if (prev_td->td_status & UHCI_TD_STATUS_MASK)
	{
		kprintf("uhci: bulk transfer error (status=0x%08X)\n", prev_td->td_status);
		dma_free(&buf);
		return (-1);
	}

	if (direction && data != NULL)
		memcpy(data, buf.db_vaddr, datalen);

	dma_free(&buf);
	return (0);
}

/* -----------------------------------------------------------------------
 * Interrupt transfer scheduling — periodic IN from a HID/interrupt endpoint.
 * Allocates pool QH+TD at index (3+slot), inserts QH before int_qh in all
 * 1024 frame entries, marks TD active.  ISR re-arms on each completion.
 * --------------------------------------------------------------------- */
struct uhci_qh *
uhci_schedule_intr(struct uhci_softc *sc, uint8_t addr, uint8_t ep,
    uint16_t maxpkt, uint32_t interval_ms, void (*callback)(void *arg,
    uint8_t *data, int len), void *arg)
{
	struct uhci_intr_slot *slot;
	struct uhci_qh *qh;
	struct uhci_td *td;
	int i;

	(void)interval_ms; /* all slots run every frame for simplicity */

	slot = NULL;
	for (i = 0; i < UHCI_INTR_SLOTS; i++) {
		if (!sc->sc_intr[i].is_used) {
			slot = &sc->sc_intr[i];
			break;
		}
	}
	if (slot == NULL) {
		kprintf("uhci: no free interrupt slots\n");
		return (NULL);
	}

	if (dma_alloc(maxpkt < 4 ? 4 : maxpkt, 4, &slot->is_buf) != 0)
		return (NULL);

	/* Use reserved pool entries for this slot */
	qh = &sc->sc_qh_pool[3 + i];
	td = &sc->sc_td_pool[3 + i];
	memset(qh, 0, sizeof(*qh));
	memset(td, 0, sizeof(*td));

	td->td_status = UHCI_TD_ACTIVE | UHCI_TD_IOC | UHCI_TD_ERRCNT(3) |
	    UHCI_TD_SPD;
	td->td_token  = UHCI_TOKEN(maxpkt, 0, ep, addr, UHCI_PID_IN);
	td->td_buffer = slot->is_buf.db_paddr;
	td->td_link   = UHCI_PTR_T;

	qh->qh_elt    = (uint32_t)td;
	/* Link new QH → whatever the frame list currently points to */
	qh->qh_link   = sc->sc_fl[0];
	qh->qh_softc  = slot;

	/* Insert before existing head in all frame entries */
	for (i = 0; i < UHCI_FRAMELIST_COUNT; i++)
		sc->sc_fl[i] = (uint32_t)qh | UHCI_PTR_QH;

	slot->is_qh      = qh;
	slot->is_td      = td;
	slot->is_cb      = callback;
	slot->is_arg     = arg;
	slot->is_maxpkt  = maxpkt;
	slot->is_addr    = addr;
	slot->is_ep      = ep;
	slot->is_toggle  = 0;
	slot->is_used    = 1;

	kprintf("uhci: intr scheduled addr=%d ep=%d maxpkt=%d\n",
	    addr, ep, maxpkt);
	return (qh);
}

/* -----------------------------------------------------------------------
 * Probe / Attach
 * --------------------------------------------------------------------- */
static int uhci_ubx_probe(struct ubx_device *dev)
{
	if (dev->dev_vendor == UHCI_VENDOR_INTEL && dev->dev_device_id == UHCI_DEVICE_PIIX4)
		return (0);
	/* Also match by class for other UHCI controllers */
	if (dev->dev_class == UHCI_CLASS && dev->dev_subclass == UHCI_SUBCLASS && dev->dev_progif == UHCI_PROGIF)
		return (0);
	return (-1);
}

static int uhci_ubx_attach(struct ubx_device *dev)
{
	struct uhci_softc *sc = &uhci_sc;
	struct ubx_resource *res;
	uint32_t i;

	kprintf("uhci: attaching (vendor=0x%X dev=0x%X)\n", dev->dev_vendor, dev->dev_device_id);

	memset(sc, 0, sizeof(*sc));
	sc->sc_dev = dev;

	/* Extract I/O BAR and IRQ from device resources */
	for (i = 0; i < (uint32_t)dev->dev_nres; i++)
	{
		res = &dev->dev_res[i];
		if (res->r_type == UBX_RES_IOPORT && sc->sc_iobase == 0)
			sc->sc_iobase = (uint16_t)res->r_start;
		else if (res->r_type == UBX_RES_IRQ && sc->sc_irq == 0)
			sc->sc_irq = (uint8_t)res->r_start;
	}

	if (sc->sc_iobase == 0)
	{
		kprintf("uhci: no I/O BAR found\n");
		return (-1);
	}

	kprintf("uhci: iobase=0x%X irq=%d\n", sc->sc_iobase, sc->sc_irq);

	/* Ensure PCI I/O Space Enable (bit 0) and Bus Master (bit 2) are set. */
	{
		uInt32 cmd = pciRead(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04, 2);
		pciWrite(dev->dev_bus, dev->dev_slot, dev->dev_func, 0x04,
		    cmd | 0x05, 2);
	}

	/* --- Reset sequence --- */

	/* GRESET for 10 ms */
	uhci_wr16(sc, UHCI_USBCMD, UHCI_CMD_GRESET);
	uhci_delay_ms(10);
	uhci_wr16(sc, UHCI_USBCMD, 0);

	/* HCRESET — poll until clear (max 50 ms) */
	uhci_wr16(sc, UHCI_USBCMD, UHCI_CMD_HCRESET);
	for (i = 0; i < 50; i++)
	{
		if (!(uhci_rd16(sc, UHCI_USBCMD) & UHCI_CMD_HCRESET))
			break;
		uhci_delay_ms(1);
	}
	if (uhci_rd16(sc, UHCI_USBCMD) & UHCI_CMD_HCRESET)
	{
		kprintf("uhci: HCRESET timeout\n");
		return (-1);
	}

	/* --- Frame list --- */
	if (dma_alloc(4096, 4096, &sc->sc_fl_buf) != 0)
	{
		kprintf("uhci: frame list alloc failed\n");
		return (-1);
	}
	sc->sc_fl = (uint32_t *)sc->sc_fl_buf.db_vaddr;
	for (i = 0; i < UHCI_FRAMELIST_COUNT; i++)
		sc->sc_fl[i] = UHCI_PTR_T;

	/* --- QH slab (one page, fits 16 QHs of 32 bytes each) --- */
	if (dma_alloc(4096, 4096, &sc->sc_qh_buf) != 0)
	{
		kprintf("uhci: QH slab alloc failed\n");
		dma_free(&sc->sc_fl_buf);
		return (-1);
	}
	sc->sc_qh_pool = (struct uhci_qh *)sc->sc_qh_buf.db_vaddr;
	sc->sc_qh_used = 0;

	/* --- TD slab (one page, fits 64 TDs of 32 bytes each) --- */
	if (dma_alloc(4096, 4096, &sc->sc_td_buf) != 0)
	{
		kprintf("uhci: TD slab alloc failed\n");
		dma_free(&sc->sc_fl_buf);
		dma_free(&sc->sc_qh_buf);
		return (-1);
	}
	sc->sc_td_pool = (struct uhci_td *)sc->sc_td_buf.db_vaddr;
	sc->sc_td_used = 0;

	/* --- Skeleton QHs: intr → ctl → bulk → T --- */
	sc->sc_int_qh = uhci_alloc_qh(sc);
	sc->sc_ctl_qh = uhci_alloc_qh(sc);
	sc->sc_bulk_qh = uhci_alloc_qh(sc);

	sc->sc_bulk_qh->qh_link = UHCI_PTR_T;
	sc->sc_ctl_qh->qh_link = (uint32_t)sc->sc_bulk_qh | UHCI_PTR_QH;
	sc->sc_int_qh->qh_link = (uint32_t)sc->sc_ctl_qh | UHCI_PTR_QH;

	/* Point every frame list entry at the interrupt QH */
	for (i = 0; i < UHCI_FRAMELIST_COUNT; i++)
		sc->sc_fl[i] = (uint32_t)sc->sc_int_qh | UHCI_PTR_QH;

	/* --- Program hardware --- */
	uhci_wr32(sc, UHCI_FRBASEADD, sc->sc_fl_buf.db_paddr);
	uhci_wr16(sc, UHCI_FRNUM, 0);
	uhci_wr16(sc, UHCI_SOFMOD, 0x40); /* default SOF value */

	/* Enable IOC, resume, short-packet interrupts */
	uhci_wr16(sc, UHCI_USBINTR, UHCI_INTR_IOC | UHCI_INTR_RIE | UHCI_INTR_TOCRCIE);

	/* --- Install ISR --- */
	irq_register(sc->sc_irq, uhci_isr_handler);

	/* --- Run! --- */
	uhci_wr16(sc, UHCI_USBCMD, UHCI_CMD_RS | UHCI_CMD_CF | UHCI_CMD_MAXP);
	sc->sc_running = 1;

	kprintf("uhci: running (cmd=0x%X sts=0x%X)\n", uhci_rd16(sc, UHCI_USBCMD), uhci_rd16(sc, UHCI_USBSTS));

	uhci_root_port_init(sc);

	return (0);
}

/* -----------------------------------------------------------------------
 * Driver registration
 * --------------------------------------------------------------------- */
struct ubx_driver uhci_ubx_driver = {
    .drv_name = "uhci",
    .drv_probe = uhci_ubx_probe,
    .drv_attach = uhci_ubx_attach,
    .drv_detach = NULL,
};
