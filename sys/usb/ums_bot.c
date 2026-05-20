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
 * USB Mass Storage — Bulk-Only Transport (BOT) class driver.
 * Matches class 0x08 (mass storage) / subclass 0x06 (SCSI transparent) /
 * protocol 0x50 (bulk-only).
 *
 * Exports block device major=5 via ubx_device_register_block().
 */

#include <usb/ums.h>
#include <usb/usb.h>
#include <usb/usb_driver.h>
#include <usb/uhci.h>
#include <sys/bus.h>
#include <sys/klog.h>
#include <fs/vfs/mount.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

#define UMS_MAJOR	5

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

/*
 * ums_reset — Bulk-Only Mass Storage Reset (BOT class request).
 * Resets both DATA0/DATA1 toggles to DATA0 per the BOT spec.
 */
static int
ums_reset(struct ums_softc *sc)
{
	int rc;

	rc = usb_control(sc->um_dev,
	    USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
	    UMS_REQ_RESET,
	    0, sc->um_dev->ud_iface_desc.bInterfaceNumber,
	    NULL, 0);

	sc->um_toggle_in  = 0;
	sc->um_toggle_out = 0;
	return (rc);
}

/*
 * ums_send_cbw — send a 31-byte Command Block Wrapper via bulk OUT.
 */
static int
ums_send_cbw(struct ums_softc *sc, struct ums_cbw *cbw)
{
	return (uhci_bulk_transfer(sc->um_dev->ud_hc,
	    sc->um_dev->ud_addr,
	    sc->um_bulk_out_ep,
	    cbw, sizeof(*cbw),
	    0 /* OUT */,
	    &sc->um_toggle_out));
}

/*
 * ums_recv_csw — receive a 13-byte Command Status Wrapper via bulk IN.
 */
static int
ums_recv_csw(struct ums_softc *sc, struct ums_csw *csw)
{
	return (uhci_bulk_transfer(sc->um_dev->ud_hc,
	    sc->um_dev->ud_addr,
	    sc->um_bulk_in_ep,
	    csw, sizeof(*csw),
	    1 /* IN */,
	    &sc->um_toggle_in));
}

/*
 * ums_command — full BOT transaction: CBW → optional data → CSW.
 *
 * cdb/cdblen:  SCSI command descriptor block
 * data/datalen: data buffer (may be NULL for no-data commands)
 * direction:   1 = device→host (IN), 0 = host→device (OUT)
 *
 * Returns 0 on success, -1 on transport error, >0 on SCSI check condition.
 */
static int
ums_command(struct ums_softc *sc,
    const uint8_t *cdb, uint8_t cdblen,
    void *data, uint32_t datalen, int direction)
{
	struct ums_cbw cbw;
	struct ums_csw csw;
	int rc;

	memset(&cbw, 0, sizeof(cbw));
	cbw.dCBWSignature         = UMS_CBW_SIGNATURE;
	cbw.dCBWTag               = ++sc->um_tag;
	cbw.dCBWDataTransferLength = datalen;
	cbw.bmCBWFlags            = direction ? UMS_CBW_FLAGS_IN : UMS_CBW_FLAGS_OUT;
	cbw.bCBWLUN               = 0;
	cbw.bCBWCBLength          = cdblen;
	memcpy(cbw.CBWCB, cdb, cdblen);

	rc = ums_send_cbw(sc, &cbw);
	if (rc != 0) {
		kprintf("ums: CBW send failed\n");
		klog(KLOG_ERR, "ums: CBW send failed");
		return (-1);
	}

	/* Optional data phase */
	if (datalen > 0 && data != NULL) {
		rc = uhci_bulk_transfer(sc->um_dev->ud_hc,
		    sc->um_dev->ud_addr,
		    direction ? sc->um_bulk_in_ep : sc->um_bulk_out_ep,
		    data, (uint16_t)datalen,
		    direction,
		    direction ? &sc->um_toggle_in : &sc->um_toggle_out);
		if (rc != 0) {
			kprintf("ums: data phase failed (dir=%d len=%u)\n",
			    direction, datalen);
			klog(KLOG_ERR, "ums: data phase failed dir=%d len=%u",
			    direction, datalen);
			return (-1);
		}
	}

	rc = ums_recv_csw(sc, &csw);
	if (rc != 0) {
		kprintf("ums: CSW recv failed\n");
		klog(KLOG_ERR, "ums: CSW recv failed");
		return (-1);
	}

	if (csw.dCSWSignature != UMS_CSW_SIGNATURE || csw.dCSWTag != cbw.dCBWTag) {
		kprintf("ums: CSW signature/tag mismatch (sig=0x%08X tag=%u/%u)\n",
		    csw.dCSWSignature, csw.dCSWTag, cbw.dCBWTag);
		klog(KLOG_ERR, "ums: CSW sig/tag mismatch sig=0x%08X tag=%u/%u",
		    csw.dCSWSignature, csw.dCSWTag, cbw.dCBWTag);
		ums_reset(sc);
		return (-1);
	}

	if (csw.bCSWStatus == UMS_CSW_STATUS_PHASE) {
		kprintf("ums: phase error — resetting\n");
		klog(KLOG_ERR, "ums: phase error, resetting");
		ums_reset(sc);
		return (-1);
	}

	return ((int)csw.bCSWStatus);
}

/* -----------------------------------------------------------------------
 * SCSI helpers
 * --------------------------------------------------------------------- */

static int
ums_test_unit_ready(struct ums_softc *sc)
{
	uint8_t cdb[6];

	memset(cdb, 0, sizeof(cdb));
	cdb[0] = SCSI_TEST_UNIT_READY;
	return (ums_command(sc, cdb, sizeof(cdb), NULL, 0, 1));
}

static int
ums_read_capacity(struct ums_softc *sc)
{
	uint8_t  cdb[10];
	uint8_t  buf[8];
	int      rc;

	memset(cdb, 0, sizeof(cdb));
	cdb[0] = SCSI_READ_CAPACITY10;

	rc = ums_command(sc, cdb, sizeof(cdb), buf, sizeof(buf), 1);
	if (rc != 0)
		return (rc);

	/* Big-endian 32-bit values */
	sc->um_blocks   = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
	    ((uint32_t)buf[2] << 8) | buf[3];
	sc->um_blk_size = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
	    ((uint32_t)buf[6] << 8) | buf[7];

	/* READ CAPACITY(10) returns the address of the *last* block */
	sc->um_blocks += 1;

	kprintf("ums: %u blocks x %u bytes = %u MB\n",
	    sc->um_blocks, sc->um_blk_size,
	    (sc->um_blocks / 2048));
	klog(KLOG_INFO, "ums: %u blocks x %u bytes = %u MB",
	    sc->um_blocks, sc->um_blk_size,
	    (sc->um_blocks / 2048));
	return (0);
}

/* -----------------------------------------------------------------------
 * Block device ops — called from VFS/FAT with sector LBA
 * --------------------------------------------------------------------- */

static int
ums_blk_read(struct ubx_device *dev, uint32_t lba, uint32_t count, void *buf)
{
	struct ums_softc *sc = (struct ums_softc *)dev->dev_softc;
	uint8_t  cdb[10];
	uint32_t i;
	int      rc;
	uint8_t *p = (uint8_t *)buf;

	for (i = 0; i < count; i++) {
		uint32_t blk = lba + i;

		memset(cdb, 0, sizeof(cdb));
		cdb[0] = SCSI_READ10;
		cdb[2] = (uint8_t)(blk >> 24);
		cdb[3] = (uint8_t)(blk >> 16);
		cdb[4] = (uint8_t)(blk >> 8);
		cdb[5] = (uint8_t)(blk);
		cdb[7] = 0;
		cdb[8] = 1;	/* transfer length: 1 block */

		rc = ums_command(sc, cdb, sizeof(cdb),
		    p + (size_t)i * sc->um_blk_size,
		    sc->um_blk_size, 1);
		if (rc != 0) {
			kprintf("ums: READ(10) lba=%u failed rc=%d\n", blk, rc);
			klog(KLOG_ERR, "ums: READ(10) lba=%u failed rc=%d",
			    blk, rc);
			return (-1);
		}
	}
	return (0);
}

static int
ums_blk_write(struct ubx_device *dev, uint32_t lba, uint32_t count, void *buf)
{
	struct ums_softc *sc = (struct ums_softc *)dev->dev_softc;
	uint8_t  cdb[10];
	uint32_t i;
	int      rc;
	uint8_t *p = (uint8_t *)buf;

	for (i = 0; i < count; i++) {
		uint32_t blk = lba + i;

		memset(cdb, 0, sizeof(cdb));
		cdb[0] = SCSI_WRITE10;
		cdb[2] = (uint8_t)(blk >> 24);
		cdb[3] = (uint8_t)(blk >> 16);
		cdb[4] = (uint8_t)(blk >> 8);
		cdb[5] = (uint8_t)(blk);
		cdb[7] = 0;
		cdb[8] = 1;	/* transfer length: 1 block */

		rc = ums_command(sc, cdb, sizeof(cdb),
		    p + (size_t)i * sc->um_blk_size,
		    sc->um_blk_size, 0);
		if (rc != 0) {
			kprintf("ums: WRITE(10) lba=%u failed rc=%d\n", blk, rc);
			klog(KLOG_ERR, "ums: WRITE(10) lba=%u failed rc=%d",
			    blk, rc);
			return (-1);
		}
	}
	return (0);
}

static struct ubx_blk_ops ums_blk_ops = {
	.read  = ums_blk_read,
	.write = ums_blk_write,
};

/* -----------------------------------------------------------------------
 * USB class driver ops
 * --------------------------------------------------------------------- */

static int
ums_bot_probe(struct usb_device *dev)
{
	int i, has_in = 0, has_out = 0;

	for (i = 0; i < dev->ud_nep; i++) {
		if (USB_EP_TYPE(&dev->ud_ep[i]) != USB_EP_TYPE_BULK)
			continue;
		if (USB_EP_DIR_IN(&dev->ud_ep[i]))
			has_in = 1;
		else
			has_out = 1;
	}
	return ((has_in && has_out) ? 0 : -1);
}

static int
ums_bot_attach(struct usb_device *dev)
{
	struct ums_softc *sc;
	int i;

	sc = (struct ums_softc *)kmalloc(sizeof(*sc));
	if (sc == NULL)
		return (-1);
	memset(sc, 0, sizeof(*sc));
	sc->um_dev = dev;

	/* Find bulk IN and OUT endpoints */
	for (i = 0; i < dev->ud_nep; i++) {
		if (USB_EP_TYPE(&dev->ud_ep[i]) != USB_EP_TYPE_BULK)
			continue;
		if (USB_EP_DIR_IN(&dev->ud_ep[i])) {
			sc->um_bulk_in_ep  = USB_EP_NUM(&dev->ud_ep[i]);
			sc->um_max_pkt_in  = dev->ud_ep[i].wMaxPacketSize;
		} else {
			sc->um_bulk_out_ep = USB_EP_NUM(&dev->ud_ep[i]);
			sc->um_max_pkt_out = dev->ud_ep[i].wMaxPacketSize;
		}
	}

	kprintf("ums: bulk IN ep=%d (mps=%d) OUT ep=%d (mps=%d)\n",
	    sc->um_bulk_in_ep, sc->um_max_pkt_in,
	    sc->um_bulk_out_ep, sc->um_max_pkt_out);
	klog(KLOG_DEBUG, "ums: bulk IN ep=%d mps=%d OUT ep=%d mps=%d",
	    sc->um_bulk_in_ep, sc->um_max_pkt_in,
	    sc->um_bulk_out_ep, sc->um_max_pkt_out);

	/* Issue a BOT reset to get into a known state */
	ums_reset(sc);

	/* Probe medium */
	if (ums_test_unit_ready(sc) != 0) {
		kprintf("ums: TEST UNIT READY failed (continuing)\n");
		klog(KLOG_WARNING, "ums: TEST UNIT READY failed (continuing)");
	}

	if (ums_read_capacity(sc) != 0) {
		kprintf("ums: READ CAPACITY failed — no media?\n");
		klog(KLOG_ERR, "ums: READ CAPACITY failed, no media");
		kfree(sc);
		return (-1);
	}

	dev->ud_drv_softc = sc;

	/*
	 * Allocate a newbus-lite device node for the UMS device (child of the
	 * UHCI host controller) and register it as a block device.  The blk ops
	 * callbacks retrieve sc via udev->dev_softc.
	 */
	{
		struct ubx_device *udev;

		udev = ubx_device_alloc(dev->ud_hc->sc_dev, "ums0");
		if (udev == NULL) {
			kprintf("ums: ubx_device_alloc failed\n");
			klog(KLOG_ERR, "ums: ubx_device_alloc failed");
			kfree(sc);
			return (-1);
		}
		udev->dev_softc = sc;
		sc->um_udev = udev;

		if (ubx_device_register_block(udev, UMS_MAJOR, 0,
		    &ums_blk_ops) != 0) {
			kprintf("ums: block device registration failed\n");
			klog(KLOG_ERR, "ums: block device registration failed");
			ubx_device_free(udev);
			kfree(sc);
			return (-1);
		}
	}

	kprintf("ums: attached as block device major=%d minor=0\n", UMS_MAJOR);
	klog(KLOG_INFO, "ums: attached major=%d minor=0 blocks=%u blksz=%u",
	    UMS_MAJOR, sc->um_blocks, sc->um_blk_size);

	/*
	 * Auto-mount the FAT filesystem.  Pass "usb0" as a placeholder mount
	 * point name — fat_bpb_parse will override it with the volume label
	 * (e.g. "ubix" for a disk formatted with label UBIX).
	 */
	if (vfs_mount(UMS_MAJOR, 0, 0, 0xFA, "usb0", "rw") != 0) {
		kprintf("ums: vfs_mount failed\n");
		klog(KLOG_WARNING, "ums: vfs_mount failed (no FAT filesystem?)");
	} else {
		kprintf("ums: FAT filesystem mounted\n");
		klog(KLOG_INFO, "ums: FAT filesystem mounted");
	}

	return (0);
}

static int
ums_bot_detach(struct usb_device *dev)
{
	(void)dev;
	kprintf("ums: detached\n");
	klog(KLOG_INFO, "ums: detached");
	return (0);
}

struct usb_driver ums_bot_driver = {
	.drv_class    = 0x08,	/* Mass Storage */
	.drv_subclass = 0x06,	/* SCSI transparent command set */
	.drv_protocol = 0x50,	/* Bulk-Only Transport */
	.drv_probe    = ums_bot_probe,
	.drv_attach   = ums_bot_attach,
	.drv_detach   = ums_bot_detach,
};
