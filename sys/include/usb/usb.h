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

#ifndef _USB_USB_H
#define _USB_USB_H

#include <sys/types.h>
#include <usb/uhci.h>

/* -----------------------------------------------------------------------
 * USB speeds
 * --------------------------------------------------------------------- */
#define USB_SPEED_FULL  0
#define USB_SPEED_LOW   1

/* -----------------------------------------------------------------------
 * Standard USB descriptor types
 * --------------------------------------------------------------------- */
#define USB_DT_DEVICE           0x01
#define USB_DT_CONFIG           0x02
#define USB_DT_STRING           0x03
#define USB_DT_INTERFACE        0x04
#define USB_DT_ENDPOINT         0x05

/* -----------------------------------------------------------------------
 * Standard USB request codes
 * --------------------------------------------------------------------- */
#define USB_REQ_GET_STATUS      0x00
#define USB_REQ_SET_ADDRESS     0x05
#define USB_REQ_GET_DESCRIPTOR  0x06
#define USB_REQ_SET_CONFIG      0x09
#define USB_REQ_SET_INTERFACE   0x0B
#define USB_REQ_SET_PROTOCOL    0x0B  /* HID — same value, different type */
#define USB_REQ_SET_IDLE        0x0A

/* bmRequestType direction/type/recipient */
#define USB_DIR_OUT             0x00
#define USB_DIR_IN              0x80
#define USB_TYPE_STANDARD       0x00
#define USB_TYPE_CLASS          0x20
#define USB_RECIP_DEVICE        0x00
#define USB_RECIP_INTERFACE     0x01
#define USB_RECIP_ENDPOINT      0x02

/* Standard feature selectors */
#define USB_FEATURE_ENDPOINT_HALT  0x00

/* Additional standard request codes */
#define USB_REQ_CLEAR_FEATURE   0x01

/* -----------------------------------------------------------------------
 * Standard USB descriptors (all little-endian; native on i386)
 * --------------------------------------------------------------------- */

struct usb_device_desc {
	u_int8_t   bLength;
	u_int8_t   bDescriptorType;
	u_int16_t  bcdUSB;
	u_int8_t   bDeviceClass;
	u_int8_t   bDeviceSubClass;
	u_int8_t   bDeviceProtocol;
	u_int8_t   bMaxPacketSize0;
	u_int16_t  idVendor;
	u_int16_t  idProduct;
	u_int16_t  bcdDevice;
	u_int8_t   iManufacturer;
	u_int8_t   iProduct;
	u_int8_t   iSerialNumber;
	u_int8_t   bNumConfigurations;
} __attribute__((packed));

struct usb_config_desc {
	u_int8_t   bLength;
	u_int8_t   bDescriptorType;
	u_int16_t  wTotalLength;
	u_int8_t   bNumInterfaces;
	u_int8_t   bConfigurationValue;
	u_int8_t   iConfiguration;
	u_int8_t   bmAttributes;
	u_int8_t   bMaxPower;
} __attribute__((packed));

struct usb_iface_desc {
	u_int8_t   bLength;
	u_int8_t   bDescriptorType;
	u_int8_t   bInterfaceNumber;
	u_int8_t   bAlternateSetting;
	u_int8_t   bNumEndpoints;
	u_int8_t   bInterfaceClass;
	u_int8_t   bInterfaceSubClass;
	u_int8_t   bInterfaceProtocol;
	u_int8_t   iInterface;
} __attribute__((packed));

struct usb_ep_desc {
	u_int8_t   bLength;
	u_int8_t   bDescriptorType;
	u_int8_t   bEndpointAddress;  /* bit 7: direction (1=IN) */
	u_int8_t   bmAttributes;      /* bits 1:0 = transfer type */
	u_int16_t  wMaxPacketSize;
	u_int8_t   bInterval;
} __attribute__((packed));

#define USB_EP_DIR_IN(ep)   ((ep)->bEndpointAddress & 0x80)
#define USB_EP_NUM(ep)      ((ep)->bEndpointAddress & 0x0F)
#define USB_EP_TYPE(ep)     ((ep)->bmAttributes & 0x03)
#define USB_EP_TYPE_CTRL    0x00
#define USB_EP_TYPE_ISO     0x01
#define USB_EP_TYPE_BULK    0x02
#define USB_EP_TYPE_INTR    0x03

/* -----------------------------------------------------------------------
 * USB setup packet (8 bytes, sent in SETUP token)
 * --------------------------------------------------------------------- */
struct usb_setup_pkt {
	u_int8_t   bmRequestType;
	u_int8_t   bRequest;
	u_int16_t  wValue;
	u_int16_t  wIndex;
	u_int16_t  wLength;
} __attribute__((packed));

/* -----------------------------------------------------------------------
 * Per-device state
 * --------------------------------------------------------------------- */
#define USB_MAX_EP  16

struct usb_device {
	struct uhci_softc     *ud_hc;
	u_int8_t                ud_addr;
	u_int8_t                ud_speed;
	struct usb_device_desc ud_dev_desc;
	struct usb_config_desc ud_cfg_desc;
	struct usb_iface_desc  ud_iface_desc;
	struct usb_ep_desc     ud_ep[USB_MAX_EP];
	int                    ud_nep;
	struct usb_driver     *ud_driver;
	void                  *ud_drv_softc;
};

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */
int  usb_new_device(struct uhci_softc *hc, int port, int speed);

/* Helper: build and issue a standard control transfer */
int  usb_control(struct usb_device *dev, u_int8_t bmRequestType, u_int8_t bRequest,
         u_int16_t wValue, u_int16_t wIndex, void *data, u_int16_t wLength);

#endif /* _USB_USB_H */
