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

#ifndef _USB_UHCI_H
#define _USB_UHCI_H

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/dma_mem.h>

/* -----------------------------------------------------------------------
 * UHCI I/O register offsets (all relative to I/O BAR base)
 * --------------------------------------------------------------------- */
#define UHCI_USBCMD 0x00    /* 16-bit command */
#define UHCI_USBSTS 0x02    /* 16-bit status (write-1-to-clear) */
#define UHCI_USBINTR 0x04   /* 16-bit interrupt enable */
#define UHCI_FRNUM 0x06     /* 16-bit current frame number */
#define UHCI_FRBASEADD 0x08 /* 32-bit frame list physical address */
#define UHCI_SOFMOD 0x0C    /*  8-bit SOF modify */
#define UHCI_PORTSC1 0x10   /* 16-bit root port 1 status/control */
#define UHCI_PORTSC2 0x12   /* 16-bit root port 2 status/control */

/* USBCMD bits */
#define UHCI_CMD_RS 0x0001      /* Run/Stop */
#define UHCI_CMD_HCRESET 0x0002 /* Host Controller Reset */
#define UHCI_CMD_GRESET 0x0004  /* Global Reset */
#define UHCI_CMD_EGSM 0x0008    /* Enter Global Suspend Mode */
#define UHCI_CMD_FGR 0x0010     /* Force Global Resume */
#define UHCI_CMD_SWDBG 0x0020   /* Software Debug */
#define UHCI_CMD_CF 0x0040      /* Configure Flag */
#define UHCI_CMD_MAXP 0x0080    /* Max Packet (1=64 bytes) */

/* USBSTS bits (write-1-to-clear) */
#define UHCI_STS_USBINT 0x0001 /* USB Interrupt */
#define UHCI_STS_ERRINT 0x0002 /* USB Error Interrupt */
#define UHCI_STS_RESUME 0x0004 /* Resume Detect */
#define UHCI_STS_HSERR 0x0008  /* Host System Error */
#define UHCI_STS_HCERR 0x0010  /* Host Controller Process Error */
#define UHCI_STS_HCH 0x0020    /* HC Halted */

/* USBINTR bits */
#define UHCI_INTR_TOCRCIE 0x0001 /* Timeout/CRC interrupt enable */
#define UHCI_INTR_RIE 0x0002     /* Resume interrupt enable */
#define UHCI_INTR_IOC 0x0004     /* Interrupt on complete enable */
#define UHCI_INTR_SPIE 0x0008    /* Short packet interrupt enable */

/* PORTSC bits */
#define UHCI_PORTSC_CCS 0x0001  /* Current connect status */
#define UHCI_PORTSC_CSC 0x0002  /* Connect status change (w1c) */
#define UHCI_PORTSC_PE 0x0004   /* Port enabled */
#define UHCI_PORTSC_PEC 0x0008  /* Port enable change (w1c) */
#define UHCI_PORTSC_LS 0x0030   /* Line status */
#define UHCI_PORTSC_RD 0x0040   /* Resume detect */
#define UHCI_PORTSC_LSDA 0x0100 /* Low speed device attached */
#define UHCI_PORTSC_PR 0x0200   /* Port reset */
#define UHCI_PORTSC_SUSP 0x1000 /* Suspend */

/* Frame list / link pointer flags */
#define UHCI_PTR_T 0x00000001u  /* Terminate (no next element) */
#define UHCI_PTR_QH 0x00000002u /* Points to QH (else TD) */

/* TD status bits */
#define UHCI_TD_ACTIVE (1u << 23)     /* Hardware owns this TD */
#define UHCI_TD_SPD (1u << 29)        /* Short packet detect */
#define UHCI_TD_LS (1u << 26)         /* Low speed device */
#define UHCI_TD_IOC (1u << 24)        /* Interrupt on complete */
#define UHCI_TD_ERRCNT(n) ((n) << 27) /* Error counter (0-3) */
#define UHCI_TD_ERRMASK (3u << 27)
#define UHCI_TD_STATUS_MASK 0x00FF0000u /* NAK, STALL, Babble, etc. */

/* TD token helpers */
#define UHCI_PID_SETUP 0x2D
#define UHCI_PID_IN 0x69
#define UHCI_PID_OUT 0xE1
#define UHCI_TOKEN(maxlen, toggle, ep, addr, pid) (((uint32_t)((maxlen) - 1) << 21) | ((uint32_t)(toggle) << 19) | ((uint32_t)(ep) << 15) | ((uint32_t)(addr) << 8) | (pid))
/* MaxLen=0 encodes as 0x7FF in the token field */
#define UHCI_TOKEN_ZERO_LEN 0x7FFu

/* -----------------------------------------------------------------------
 * Hardware data structures
 * --------------------------------------------------------------------- */

/* Queue Head — 16-byte aligned */
struct uhci_qh
{
	uint32_t qh_link;           /* next QH/TD phys | flags */
	uint32_t qh_elt;            /* first TD phys | flags */
	struct uhci_qh *qh_next_sw; /* software list linkage */
	void *qh_softc;             /* owning transfer context */
} __attribute__((packed, aligned(16)));

/* Transfer Descriptor — 16-byte aligned */
struct uhci_td
{
	uint32_t td_link; /* next TD phys | flags */
	uint32_t td_status;
	uint32_t td_token;
	uint32_t td_buffer; /* data buffer physical address */
	struct uhci_td *td_next_sw;
	void *td_buf_vaddr;
} __attribute__((packed, aligned(16)));

/* -----------------------------------------------------------------------
 * Driver soft state
 * --------------------------------------------------------------------- */
#define UHCI_FRAMELIST_COUNT 1024
#define UHCI_TD_POOL_COUNT 64
#define UHCI_QH_POOL_COUNT 16

struct uhci_softc
{
	struct ubx_device *sc_dev;
	uint16_t sc_iobase; /* I/O BAR base (bar[4] & ~3u) */
	uint8_t sc_irq;
	struct dma_buf sc_fl_buf;   /* frame list page */
	uint32_t *sc_fl;            /* virtual frame list */
	struct dma_buf sc_qh_buf;   /* QH slab page */
	struct dma_buf sc_td_buf;   /* TD slab page */
	struct uhci_qh *sc_int_qh;  /* interrupt skeleton QH */
	struct uhci_qh *sc_ctl_qh;  /* control skeleton QH */
	struct uhci_qh *sc_bulk_qh; /* bulk skeleton QH */
	struct uhci_qh *sc_qh_pool; /* QH slab base */
	struct uhci_td *sc_td_pool; /* TD slab base */
	int sc_qh_used;
	int sc_td_used;
	int sc_running;
};

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */
extern struct ubx_driver uhci_ubx_driver;

int uhci_control_transfer(struct uhci_softc *sc, uint8_t addr, uint8_t ep, uint8_t *setup_pkt, void *data, uint16_t datalen, int direction);

int uhci_bulk_transfer(struct uhci_softc *sc, uint8_t addr, uint8_t ep, void *data, uint16_t datalen, int direction, uint8_t *toggle);

struct uhci_qh *uhci_schedule_intr(struct uhci_softc *sc, uint8_t addr, uint8_t ep, uint16_t maxpkt, uint32_t interval_ms, void (*callback)(void *arg, uint8_t *data, int len), void *arg);

void uhci_root_port_init(struct uhci_softc *sc);

#endif /* _USB_UHCI_H */
