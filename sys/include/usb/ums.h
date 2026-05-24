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

#ifndef _USB_UMS_H
#define _USB_UMS_H

#include <sys/types.h>
#include <usb/usb.h>

/* -----------------------------------------------------------------------
 * Bulk-Only Transport (BOT) constants
 * --------------------------------------------------------------------- */
#define UMS_CBW_SIGNATURE	0x43425355u	/* "USBC" little-endian */
#define UMS_CSW_SIGNATURE	0x53425355u	/* "USBS" little-endian */

#define UMS_CBW_FLAGS_IN	0x80		/* device-to-host data phase */
#define UMS_CBW_FLAGS_OUT	0x00		/* host-to-device data phase */

#define UMS_CSW_STATUS_PASS	0
#define UMS_CSW_STATUS_FAIL	1
#define UMS_CSW_STATUS_PHASE	2		/* phase error — requires reset */

/* BOT class control requests */
#define UMS_REQ_RESET		0xFF		/* Bulk-Only Mass Storage Reset */
#define UMS_REQ_GET_MAX_LUN	0xFE

/* SCSI opcodes */
#define SCSI_TEST_UNIT_READY	0x00
#define SCSI_INQUIRY		0x12
#define SCSI_READ_CAPACITY10	0x25
#define SCSI_READ10		0x28
#define SCSI_WRITE10		0x2A

/* -----------------------------------------------------------------------
 * On-wire structures (packed, no padding)
 * --------------------------------------------------------------------- */

/* Command Block Wrapper — 31 bytes, sent as a single bulk OUT */
struct ums_cbw {
	uint32_t dCBWSignature;		/* UMS_CBW_SIGNATURE */
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t  bmCBWFlags;		/* UMS_CBW_FLAGS_IN / _OUT */
	uint8_t  bCBWLUN;
	uint8_t  bCBWCBLength;		/* 1–16 */
	uint8_t  CBWCB[16];		/* SCSI CDB */
} __attribute__((packed));

/* Command Status Wrapper — 13 bytes, received as a single bulk IN */
struct ums_csw {
	uint32_t dCSWSignature;		/* UMS_CSW_SIGNATURE */
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t  bCSWStatus;		/* UMS_CSW_STATUS_* */
} __attribute__((packed));

/* Forward declaration — avoid pulling in bus.h from usb headers */
struct ubx_device;

/* -----------------------------------------------------------------------
 * Driver soft state
 * --------------------------------------------------------------------- */
struct ums_softc {
	struct usb_device *um_dev;
	struct ubx_device *um_udev;		/* newbus-lite block device node */
	uint8_t            um_bulk_in_ep;
	uint8_t            um_bulk_out_ep;
	uint16_t           um_max_pkt_in;
	uint16_t           um_max_pkt_out;
	uint8_t            um_toggle_in;
	uint8_t            um_toggle_out;
	uint32_t           um_tag;
	uint32_t           um_blocks;		/* total block count */
	uint32_t           um_blk_size;		/* bytes per block */
	char               um_mountpath[96];	/* path automountd mounted us at */
};

/* -----------------------------------------------------------------------
 * Public API (used by usb.c driver table only)
 * --------------------------------------------------------------------- */
extern struct usb_driver ums_bot_driver;

#endif /* _USB_UMS_H */
