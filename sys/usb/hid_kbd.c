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
 * USB HID boot-protocol keyboard driver.
 * Matches class 0x03 / subclass 0x01 / protocol 0x01.
 * Feeds kbd_ring via kbd_ring_push() — coexists with the PS/2 driver.
 */

#include <usb/hid.h>
#include <usb/usb.h>
#include <usb/usb_driver.h>
#include <usb/uhci.h>
#include <isa/kbd.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Soft state
 * --------------------------------------------------------------------- */
struct hid_kbd_softc
{
	struct usb_device *kd_dev;
	struct uhci_qh *kd_intr_qh;
	struct hid_kbd_report kd_last;
};

/* -----------------------------------------------------------------------
 * HID Usage Page 0x07 keycode → ASCII/KEY_* translation
 * --------------------------------------------------------------------- */
static uint32_t hid_to_key(uint8_t hid, int shifted)
{
	if (hid >= 0x04 && hid <= 0x1D)
		return ((uint32_t)(shifted ? 'A' : 'a') + (hid - 0x04));

	switch (hid)
	{
	case 0x1E:
		return (shifted ? '!' : '1');
	case 0x1F:
		return (shifted ? '@' : '2');
	case 0x20:
		return (shifted ? '#' : '3');
	case 0x21:
		return (shifted ? '$' : '4');
	case 0x22:
		return (shifted ? '%' : '5');
	case 0x23:
		return (shifted ? '^' : '6');
	case 0x24:
		return (shifted ? '&' : '7');
	case 0x25:
		return (shifted ? '*' : '8');
	case 0x26:
		return (shifted ? '(' : '9');
	case 0x27:
		return (shifted ? ')' : '0');
	case 0x28:
		return ('\n');
	case 0x29:
		return (KEY_ESC);
	case 0x2A:
		return ('\b');
	case 0x2B:
		return ('\t');
	case 0x2C:
		return (' ');
	case 0x2D:
		return (shifted ? '_' : '-');
	case 0x2E:
		return (shifted ? '+' : '=');
	case 0x2F:
		return (shifted ? '{' : '[');
	case 0x30:
		return (shifted ? '}' : ']');
	case 0x31:
		return (shifted ? '|' : '\\');
	case 0x33:
		return (shifted ? ':' : ';');
	case 0x34:
		return (shifted ? '"' : '\'');
	case 0x35:
		return (shifted ? '~' : '`');
	case 0x36:
		return (shifted ? '<' : ',');
	case 0x37:
		return (shifted ? '>' : '.');
	case 0x38:
		return (shifted ? '?' : '/');
	case 0x3A:
		return (KEY_F1);
	case 0x3B:
		return (KEY_F2);
	case 0x3C:
		return (KEY_F3);
	case 0x3D:
		return (KEY_F4);
	case 0x3E:
		return (KEY_F5);
	case 0x3F:
		return (KEY_F6);
	case 0x40:
		return (KEY_F7);
	case 0x41:
		return (KEY_F8);
	case 0x42:
		return (KEY_F9);
	case 0x43:
		return (KEY_F10);
	case 0x44:
		return (KEY_F11);
	case 0x45:
		return (KEY_F12);
	case 0x49:
		return (KEY_INS);
	case 0x4A:
		return (KEY_HOME);
	case 0x4B:
		return (KEY_PGUP);
	case 0x4C:
		return (KEY_DEL);
	case 0x4D:
		return (KEY_END);
	case 0x4E:
		return (KEY_PGDN);
	case 0x4F:
		return (KEY_RIGHT);
	case 0x50:
		return (KEY_LEFT);
	case 0x51:
		return (KEY_DOWN);
	case 0x52:
		return (KEY_UP);
	default:
		return (0);
	}
}

/* -----------------------------------------------------------------------
 * Interrupt callback — runs in ISR context; no kmalloc, no blocking
 * --------------------------------------------------------------------- */
static void hid_kbd_callback(void *arg, uint8_t *data, int len)
{
	struct hid_kbd_softc *sc = (struct hid_kbd_softc *)arg;
	struct hid_kbd_report *cur, *last;
	int shifted, ctrl, alt, i, j, found;
	uint32_t kc;

	if (len < 3)
		return;

	cur = (struct hid_kbd_report *)data;
	last = &sc->kd_last;
	shifted = (cur->modifier & HID_MOD_SHIFT) ? 1 : 0;
	ctrl    = (cur->modifier & (HID_MOD_LCTRL | HID_MOD_RCTRL)) ? 1 : 0;
	alt     = (cur->modifier & (HID_MOD_LALT  | HID_MOD_RALT))  ? 1 : 0;

	/* Key-press: keycodes in cur but not in last */
	for (i = 0; i < 6; i++)
	{
		if (cur->keycode[i] == 0)
			continue;
		found = 0;
		for (j = 0; j < 6; j++)
		{
			if (last->keycode[j] == cur->keycode[i])
			{
				found = 1;
				break;
			}
		}
		if (!found)
		{
			/* Ctrl+letter: produce control character (^A=1 … ^Z=26) */
			if (ctrl && cur->keycode[i] >= 0x04 && cur->keycode[i] <= 0x1D)
				kc = cur->keycode[i] - 0x04 + 1;
			else
				kc = hid_to_key(cur->keycode[i], shifted);

			if (kc == 0)
				continue;

			/* Ctrl+Alt+F1..F4: deferred GUI→text VTY switch */
			if (ctrl && alt && cur->keycode[i] >= 0x3A && cur->keycode[i] <= 0x3D) {
				vesa_text_slot = cur->keycode[i] - 0x3A;
				continue;
			}

			/* Alt+F1..F4: deferred VTY switch — mirrors AT keyboard ISR */
			if (alt && cur->keycode[i] >= 0x3A && cur->keycode[i] <= 0x3D) {
				tty_switch_slot = cur->keycode[i] - 0x3A;
				continue;
			}

			/* Ctrl-C: kill foreground task — mirrors AT keyboard ISR */
			if (kc == 0x03) {
				if (tty_foreground != NULL) {
					kTask_t *victim = schedFindTask(tty_foreground->owner);
					if (victim != NULL) {
						if (victim->parent != NULL)
							tty_foreground->owner = victim->parent->id;
						sched_killTree(victim->id);
					}
				}
				continue; /* do not push to ring */
			}

			kbd_ring_push(kc, 1);
		}
	}

	/* Key-release: only special keys need up events (in kbd_ring for GUI) */
	for (i = 0; i < 6; i++)
	{
		if (last->keycode[i] == 0)
			continue;
		found = 0;
		for (j = 0; j < 6; j++)
		{
			if (cur->keycode[j] == last->keycode[i])
			{
				found = 1;
				break;
			}
		}
		if (!found)
		{
			kc = hid_to_key(last->keycode[i], shifted);
			if (kc >= 0x100) /* only push key-up for special keys */
				kbd_ring_push(kc, 0);
		}
	}

	memcpy(last, cur, sizeof(*last));
}

/* -----------------------------------------------------------------------
 * Driver ops
 * --------------------------------------------------------------------- */
static int hid_kbd_probe(struct usb_device *dev)
{
	int i;

	for (i = 0; i < dev->ud_nep; i++)
	{
		if (USB_EP_DIR_IN(&dev->ud_ep[i]) && USB_EP_TYPE(&dev->ud_ep[i]) == USB_EP_TYPE_INTR)
			return (0);
	}
	return (-1);
}

static int hid_kbd_attach(struct usb_device *dev)
{
	struct hid_kbd_softc *sc;
	struct usb_ep_desc *ep;
	int i, rc;

	/* Find interrupt IN endpoint */
	ep = NULL;
	for (i = 0; i < dev->ud_nep; i++)
	{
		if (USB_EP_DIR_IN(&dev->ud_ep[i]) && USB_EP_TYPE(&dev->ud_ep[i]) == USB_EP_TYPE_INTR)
		{
			ep = &dev->ud_ep[i];
			break;
		}
	}
	if (ep == NULL)
		return (-1);

	/* SET_PROTOCOL(Boot=0) — wIndex = interface number */
	rc = usb_control(dev, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, USB_REQ_SET_PROTOCOL, 0, dev->ud_iface_desc.bInterfaceNumber, NULL, 0);
	if (rc != 0)
	{
		kprintf("hid_kbd: SET_PROTOCOL failed\n");
		return (-1);
	}

	/* SET_IDLE(rate=0, report_id=0) — only report on state change */
	rc = usb_control(dev, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, USB_REQ_SET_IDLE, 0, dev->ud_iface_desc.bInterfaceNumber, NULL, 0);
	if (rc != 0)
		kprintf("hid_kbd: SET_IDLE failed (non-fatal)\n");

	sc = (struct hid_kbd_softc *)kmalloc(sizeof(*sc));
	if (sc == NULL)
		return (-1);
	memset(sc, 0, sizeof(*sc));
	sc->kd_dev = dev;

	sc->kd_intr_qh = uhci_schedule_intr(dev->ud_hc, dev->ud_addr, USB_EP_NUM(ep), ep->wMaxPacketSize, ep->bInterval, hid_kbd_callback, sc);
	if (sc->kd_intr_qh == NULL)
	{
		kfree(sc);
		return (-1);
	}

	dev->ud_drv_softc = sc;
	kprintf("hid_kbd: attached addr=%d ep=0x%02X maxpkt=%d\n", dev->ud_addr, ep->bEndpointAddress, ep->wMaxPacketSize);
	return (0);
}

static int hid_kbd_detach(struct usb_device *dev)
{
	(void)dev;
	kprintf("hid_kbd: detached\n");
	return (0);
}

struct usb_driver hid_kbd_driver = {
    .drv_class = 0x03,    /* HID */
    .drv_subclass = 0x01, /* Boot Interface */
    .drv_protocol = 0x01, /* Keyboard */
    .drv_probe = hid_kbd_probe,
    .drv_attach = hid_kbd_attach,
    .drv_detach = hid_kbd_detach,
};
