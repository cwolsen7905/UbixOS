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
 * USB core — device enumeration and class driver dispatch.
 * Implements the full USB enumeration sequence for UHCI-attached devices.
 */

#include <usb/usb.h>
#include <usb/usb_driver.h>
#include <usb/uhci.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Class driver table — populated in hid_kbd.c (Phase 6) and ums_bot.c (Phase 7)
 * --------------------------------------------------------------------- */
extern struct usb_driver hid_kbd_driver;
extern struct usb_driver ums_bot_driver;

static struct usb_driver *const usb_driver_table[] = {
	&hid_kbd_driver,
	&ums_bot_driver,
	NULL,
};

/* -----------------------------------------------------------------------
 * USB address allocator (addresses 1–127; 0 reserved for enumeration)
 * --------------------------------------------------------------------- */
static u_int8_t usb_next_addr = 1;

static u_int8_t
usb_alloc_addr(void)
{
	if (usb_next_addr > 127)
		return (0);
	return (usb_next_addr++);
}

/* -----------------------------------------------------------------------
 * usb_control — build setup packet and issue control transfer
 * --------------------------------------------------------------------- */
int
usb_control(struct usb_device *dev, u_int8_t bmRequestType, u_int8_t bRequest,
    u_int16_t wValue, u_int16_t wIndex, void *data, u_int16_t wLength)
{
	struct usb_setup_pkt setup;
	int direction;

	setup.bmRequestType = bmRequestType;
	setup.bRequest      = bRequest;
	setup.wValue        = wValue;
	setup.wIndex        = wIndex;
	setup.wLength       = wLength;

	direction = (bmRequestType & USB_DIR_IN) ? 1 : 0;

	return (uhci_control_transfer(dev->ud_hc, dev->ud_addr, 0,
	    (u_int8_t *)&setup, data, wLength, direction));
}

/* -----------------------------------------------------------------------
 * usb_parse_config — walk config blob extracting iface + endpoint descs
 * --------------------------------------------------------------------- */
static void
usb_parse_config(struct usb_device *dev, u_int8_t *buf, u_int16_t len)
{
	u_int16_t off = 0;
	u_int8_t  blen, dtype;

	dev->ud_nep = 0;
	memset(&dev->ud_iface_desc, 0, sizeof(dev->ud_iface_desc));

	while (off + 2 <= len) {
		blen  = buf[off];
		dtype = buf[off + 1];

		if (blen == 0)
			break;

		if (dtype == USB_DT_INTERFACE && blen >= sizeof(struct usb_iface_desc))
			memcpy(&dev->ud_iface_desc, buf + off,
			    sizeof(dev->ud_iface_desc));
		else if (dtype == USB_DT_ENDPOINT &&
		         blen >= sizeof(struct usb_ep_desc) &&
		         dev->ud_nep < USB_MAX_EP)
			memcpy(&dev->ud_ep[dev->ud_nep++], buf + off,
			    sizeof(struct usb_ep_desc));

		off += blen;
	}
}

/* -----------------------------------------------------------------------
 * usb_new_device — full enumeration sequence
 * Called from uhci_root_port_init() once a device is port-reset and enabled.
 * --------------------------------------------------------------------- */
int
usb_new_device(struct uhci_softc *hc, int port, int speed)
{
	struct usb_device *dev;
	struct usb_driver *drv;
	u_int8_t  addr, i;
	u_int8_t *cfgbuf;
	u_int16_t cfglen;
	int      rc;

	(void)port;

	dev = (struct usb_device *)kmalloc(sizeof(*dev));
	if (dev == NULL)
		return (-1);
	memset(dev, 0, sizeof(*dev));
	dev->ud_hc    = hc;
	dev->ud_addr  = 0;   /* address 0 during enumeration */
	dev->ud_speed = (u_int8_t)speed;

	kprintf("usb: enumerating %s-speed device on port %d\n",
	    speed == USB_SPEED_LOW ? "low" : "full", port + 1);

	/* Step 1: GET_DESCRIPTOR(Device, 8) at addr 0 to learn bMaxPacketSize0 */
	rc = usb_control(dev,
	    USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
	    USB_REQ_GET_DESCRIPTOR,
	    (u_int16_t)(USB_DT_DEVICE << 8), 0,
	    &dev->ud_dev_desc, 8);
	if (rc != 0) {
		kprintf("usb: GET_DESCRIPTOR(Device,8) failed\n");
		kfree(dev);
		return (-1);
	}

	kprintf("usb: bMaxPacketSize0=%d\n", dev->ud_dev_desc.bMaxPacketSize0);

	/* Step 2: SET_ADDRESS */
	addr = usb_alloc_addr();
	if (addr == 0) {
		kprintf("usb: no free addresses\n");
		kfree(dev);
		return (-1);
	}
	rc = usb_control(dev,
	    USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
	    USB_REQ_SET_ADDRESS,
	    addr, 0, NULL, 0);
	if (rc != 0) {
		kprintf("usb: SET_ADDRESS(%d) failed\n", addr);
		kfree(dev);
		return (-1);
	}
	dev->ud_addr = addr;
	kprintf("usb: assigned address %d\n", addr);

	/* Step 3: GET_DESCRIPTOR(Device, 18) at new address */
	rc = usb_control(dev,
	    USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
	    USB_REQ_GET_DESCRIPTOR,
	    (u_int16_t)(USB_DT_DEVICE << 8), 0,
	    &dev->ud_dev_desc, sizeof(struct usb_device_desc));
	if (rc != 0) {
		kprintf("usb: GET_DESCRIPTOR(Device,18) failed\n");
		kfree(dev);
		return (-1);
	}

	kprintf("usb: vid=0x%04X pid=0x%04X class=%d/%d/%d\n",
	    dev->ud_dev_desc.idVendor, dev->ud_dev_desc.idProduct,
	    dev->ud_dev_desc.bDeviceClass,
	    dev->ud_dev_desc.bDeviceSubClass,
	    dev->ud_dev_desc.bDeviceProtocol);

	/* Step 4: GET_DESCRIPTOR(Config, 9) to learn wTotalLength */
	rc = usb_control(dev,
	    USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
	    USB_REQ_GET_DESCRIPTOR,
	    (u_int16_t)(USB_DT_CONFIG << 8), 0,
	    &dev->ud_cfg_desc, sizeof(struct usb_config_desc));
	if (rc != 0) {
		kprintf("usb: GET_DESCRIPTOR(Config,9) failed\n");
		kfree(dev);
		return (-1);
	}

	cfglen = dev->ud_cfg_desc.wTotalLength;
	if (cfglen < sizeof(struct usb_config_desc) || cfglen > 512) {
		kprintf("usb: bad wTotalLength=%d\n", cfglen);
		kfree(dev);
		return (-1);
	}

	/* Step 5: GET_DESCRIPTOR(Config, wTotalLength) — full config blob */
	cfgbuf = (u_int8_t *)kmalloc(cfglen);
	if (cfgbuf == NULL) {
		kfree(dev);
		return (-1);
	}
	rc = usb_control(dev,
	    USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
	    USB_REQ_GET_DESCRIPTOR,
	    (u_int16_t)(USB_DT_CONFIG << 8), 0,
	    cfgbuf, cfglen);
	if (rc != 0) {
		kprintf("usb: GET_DESCRIPTOR(Config,full) failed\n");
		kfree(cfgbuf);
		kfree(dev);
		return (-1);
	}

	/* Step 6: Parse interface and endpoint descriptors */
	usb_parse_config(dev, cfgbuf, cfglen);
	kfree(cfgbuf);

	kprintf("usb: iface class=%d/%d/%d nep=%d\n",
	    dev->ud_iface_desc.bInterfaceClass,
	    dev->ud_iface_desc.bInterfaceSubClass,
	    dev->ud_iface_desc.bInterfaceProtocol,
	    dev->ud_nep);

	for (i = 0; i < dev->ud_nep; i++)
		kprintf("usb:   ep%d addr=0x%02X type=%d mps=%d interval=%d\n",
		    i,
		    dev->ud_ep[i].bEndpointAddress,
		    USB_EP_TYPE(&dev->ud_ep[i]),
		    dev->ud_ep[i].wMaxPacketSize,
		    dev->ud_ep[i].bInterval);

	/* Step 7: SET_CONFIGURATION */
	rc = usb_control(dev,
	    USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
	    USB_REQ_SET_CONFIG,
	    dev->ud_cfg_desc.bConfigurationValue, 0, NULL, 0);
	if (rc != 0) {
		kprintf("usb: SET_CONFIGURATION failed\n");
		kfree(dev);
		return (-1);
	}

	/* Step 8: Walk class driver table */
	for (i = 0; usb_driver_table[i] != NULL; i++) {
		drv = usb_driver_table[i];

		if (drv->drv_class != USB_DRIVER_WILDCARD &&
		    drv->drv_class != dev->ud_iface_desc.bInterfaceClass)
			continue;
		if (drv->drv_subclass != USB_DRIVER_WILDCARD &&
		    drv->drv_subclass != dev->ud_iface_desc.bInterfaceSubClass)
			continue;
		if (drv->drv_protocol != USB_DRIVER_WILDCARD &&
		    drv->drv_protocol != dev->ud_iface_desc.bInterfaceProtocol)
			continue;

		if (drv->drv_probe != NULL && drv->drv_probe(dev) != 0)
			continue;

		dev->ud_driver = drv;
		if (drv->drv_attach != NULL && drv->drv_attach(dev) == 0) {
			kprintf("usb: attached driver for class %d/%d/%d\n",
			    drv->drv_class, drv->drv_subclass, drv->drv_protocol);
			return (0);
		}
		dev->ud_driver = NULL;
	}

	kprintf("usb: no driver for class %d/%d/%d vid=0x%04X pid=0x%04X\n",
	    dev->ud_iface_desc.bInterfaceClass,
	    dev->ud_iface_desc.bInterfaceSubClass,
	    dev->ud_iface_desc.bInterfaceProtocol,
	    dev->ud_dev_desc.idVendor,
	    dev->ud_dev_desc.idProduct);

	kfree(dev);
	return (0);
}
