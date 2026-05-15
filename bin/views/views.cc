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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>

/* Colour helpers (were in fb/fb.h) */
#define FB_RGB(r,g,b) \
    (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define FB_WHITE       0x00FFFFFFu
#define FB_FONT_W      8
#define FB_FONT_H      8

#define WIN_BPP       4   /* shared buffers always 32bpp BGRX */

/* Decoration colours */
#define DECOR_BG        FB_RGB(0x28, 0x48, 0x70)
#define DECOR_HI        FB_RGB(0x40, 0x70, 0xA8)
#define DECOR_BG_INACT  FB_RGB(0x28, 0x28, 0x38)
#define DECOR_HI_INACT  FB_RGB(0x38, 0x38, 0x50)
#define DECOR_SEP       FB_RGB(0x10, 0x20, 0x30)
#define DECOR_CLOSE_BG  FB_RGB(0x90, 0x22, 0x22)

#define CUR_W  12
#define CUR_H  12

#define PAGE_SIZE 4096

/* ------------------------------------------------------------------ */
/* Syscall stubs                                                        */
/* ------------------------------------------------------------------ */

/* Syscall 43 — map the VESA framebuffer into the calling process */
asm(
    ".text                            \n"
    ".globl _sys_mapfb                \n"
    ".type  _sys_mapfb, @function     \n"
    "_sys_mapfb:                      \n"
    "  movl $43, %eax                 \n"
    "  int  $0x81                     \n"
    "  ret                            \n"
);

/* Syscall 45 — share a virtual region into another process's space */
asm(
    ".text                                \n"
    ".globl _sys_shareregion              \n"
    ".type  _sys_shareregion, @function   \n"
    "_sys_shareregion:                    \n"
    "  movl $45, %eax                     \n"
    "  int  $0x81                         \n"
    "  ret                                \n"
);

/* Syscall 44 — poll mouse event queue */
asm(
    ".text                              \n"
    ".globl _sys_getmouse               \n"
    ".type  _sys_getmouse, @function    \n"
    "_sys_getmouse:                     \n"
    "  movl $44, %eax                   \n"
    "  int  $0x81                       \n"
    "  ret                              \n"
);

/* Syscall 46 — poll keyboard event queue */
asm(
    ".text                              \n"
    ".globl _sys_getkbd                 \n"
    ".type  _sys_getkbd, @function      \n"
    "_sys_getkbd:                       \n"
    "  movl $46, %eax                   \n"
    "  int  $0x81                       \n"
    "  ret                              \n"
);

extern "C" {
    struct _fb_map {
        void     *base;
        uint32_t  width;
        uint32_t  height;
        uint16_t  pitch;
        uint8_t   bpp;
    };
    int _sys_mapfb(struct _fb_map *info);
    int _sys_shareregion(int dst_pid, void *vaddr,
        uint32_t size, uint32_t *out_vaddr);
    int _sys_getmouse(mouse_event_t *ev);
    int _sys_getkbd(kbd_event_t *ev);
}

static int
share_buffer(int dst_pid, void *buf, uint32_t size, uint32_t *client_vaddr)
{
    return _sys_shareregion(dst_pid, buf, size, client_vaddr);
}

static int
poll_mouse(mouse_event_t *ev)
{
    return _sys_getmouse(ev);
}

static int
poll_kbd(kbd_event_t *ev)
{
    return _sys_getkbd(ev);
}

/* ------------------------------------------------------------------ */
/* 8x8 bitmap font (printable ASCII 0x20-0x7F)                         */
/* ------------------------------------------------------------------ */

static const uint8_t font8x8[96][8] = {
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0x20 space */
    { 0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x00 }, /* 0x21 ! */
    { 0x28,0x28,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0x22 " */
    { 0x28,0x28,0xFE,0x28,0xFE,0x28,0x28,0x00 }, /* 0x23 # */
    { 0x10,0x7C,0x90,0x7C,0x12,0x7C,0x10,0x00 }, /* 0x24 $ */
    { 0xC2,0xC4,0x08,0x10,0x26,0x46,0x00,0x00 }, /* 0x25 % */
    { 0x30,0x48,0x30,0x72,0xCC,0x76,0x00,0x00 }, /* 0x26 & */
    { 0x10,0x10,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0x27 ' */
    { 0x08,0x10,0x20,0x20,0x20,0x10,0x08,0x00 }, /* 0x28 ( */
    { 0x20,0x10,0x08,0x08,0x08,0x10,0x20,0x00 }, /* 0x29 ) */
    { 0x00,0x28,0x10,0x7C,0x10,0x28,0x00,0x00 }, /* 0x2A * */
    { 0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00 }, /* 0x2B + */
    { 0x00,0x00,0x00,0x00,0x00,0x18,0x10,0x20 }, /* 0x2C , */
    { 0x00,0x00,0x00,0x7C,0x00,0x00,0x00,0x00 }, /* 0x2D - */
    { 0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00 }, /* 0x2E . */
    { 0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x00 }, /* 0x2F / */
    { 0x38,0x44,0x4C,0x54,0x64,0x44,0x38,0x00 }, /* 0x30 0 */
    { 0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00 }, /* 0x31 1 */
    { 0x38,0x44,0x04,0x18,0x20,0x40,0x7C,0x00 }, /* 0x32 2 */
    { 0x38,0x44,0x04,0x18,0x04,0x44,0x38,0x00 }, /* 0x33 3 */
    { 0x08,0x18,0x28,0x48,0x7C,0x08,0x08,0x00 }, /* 0x34 4 */
    { 0x7C,0x40,0x78,0x04,0x04,0x44,0x38,0x00 }, /* 0x35 5 */
    { 0x18,0x20,0x40,0x78,0x44,0x44,0x38,0x00 }, /* 0x36 6 */
    { 0x7C,0x04,0x08,0x10,0x20,0x20,0x20,0x00 }, /* 0x37 7 */
    { 0x38,0x44,0x44,0x38,0x44,0x44,0x38,0x00 }, /* 0x38 8 */
    { 0x38,0x44,0x44,0x3C,0x04,0x08,0x30,0x00 }, /* 0x39 9 */
    { 0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00 }, /* 0x3A : */
    { 0x00,0x18,0x18,0x00,0x18,0x10,0x20,0x00 }, /* 0x3B ; */
    { 0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x00 }, /* 0x3C < */
    { 0x00,0x00,0x7C,0x00,0x7C,0x00,0x00,0x00 }, /* 0x3D = */
    { 0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x00 }, /* 0x3E > */
    { 0x38,0x44,0x04,0x08,0x10,0x00,0x10,0x00 }, /* 0x3F ? */
    { 0x3C,0x42,0x9A,0xAA,0x9E,0x40,0x3C,0x00 }, /* 0x40 @ */
    { 0x38,0x44,0x44,0x7C,0x44,0x44,0x44,0x00 }, /* 0x41 A */
    { 0x78,0x44,0x44,0x78,0x44,0x44,0x78,0x00 }, /* 0x42 B */
    { 0x38,0x44,0x40,0x40,0x40,0x44,0x38,0x00 }, /* 0x43 C */
    { 0x70,0x48,0x44,0x44,0x44,0x48,0x70,0x00 }, /* 0x44 D */
    { 0x7C,0x40,0x40,0x78,0x40,0x40,0x7C,0x00 }, /* 0x45 E */
    { 0x7C,0x40,0x40,0x78,0x40,0x40,0x40,0x00 }, /* 0x46 F */
    { 0x38,0x44,0x40,0x4C,0x44,0x44,0x3C,0x00 }, /* 0x47 G */
    { 0x44,0x44,0x44,0x7C,0x44,0x44,0x44,0x00 }, /* 0x48 H */
    { 0x38,0x10,0x10,0x10,0x10,0x10,0x38,0x00 }, /* 0x49 I */
    { 0x04,0x04,0x04,0x04,0x04,0x44,0x38,0x00 }, /* 0x4A J */
    { 0x44,0x48,0x50,0x60,0x50,0x48,0x44,0x00 }, /* 0x4B K */
    { 0x40,0x40,0x40,0x40,0x40,0x40,0x7C,0x00 }, /* 0x4C L */
    { 0x44,0x6C,0x54,0x54,0x44,0x44,0x44,0x00 }, /* 0x4D M */
    { 0x44,0x64,0x54,0x4C,0x44,0x44,0x44,0x00 }, /* 0x4E N */
    { 0x38,0x44,0x44,0x44,0x44,0x44,0x38,0x00 }, /* 0x4F O */
    { 0x78,0x44,0x44,0x78,0x40,0x40,0x40,0x00 }, /* 0x50 P */
    { 0x38,0x44,0x44,0x44,0x54,0x48,0x34,0x00 }, /* 0x51 Q */
    { 0x78,0x44,0x44,0x78,0x50,0x48,0x44,0x00 }, /* 0x52 R */
    { 0x38,0x44,0x40,0x38,0x04,0x44,0x38,0x00 }, /* 0x53 S */
    { 0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0x00 }, /* 0x54 T */
    { 0x44,0x44,0x44,0x44,0x44,0x44,0x38,0x00 }, /* 0x55 U */
    { 0x44,0x44,0x44,0x44,0x44,0x28,0x10,0x00 }, /* 0x56 V */
    { 0x44,0x44,0x44,0x54,0x54,0x6C,0x44,0x00 }, /* 0x57 W */
    { 0x44,0x44,0x28,0x10,0x28,0x44,0x44,0x00 }, /* 0x58 X */
    { 0x44,0x44,0x28,0x10,0x10,0x10,0x10,0x00 }, /* 0x59 Y */
    { 0x7C,0x04,0x08,0x10,0x20,0x40,0x7C,0x00 }, /* 0x5A Z */
    { 0x38,0x20,0x20,0x20,0x20,0x20,0x38,0x00 }, /* 0x5B [ */
    { 0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x00 }, /* 0x5C \ */
    { 0x38,0x08,0x08,0x08,0x08,0x08,0x38,0x00 }, /* 0x5D ] */
    { 0x10,0x28,0x44,0x00,0x00,0x00,0x00,0x00 }, /* 0x5E ^ */
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x7C,0x00 }, /* 0x5F _ */
    { 0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0x60 ` */
    { 0x00,0x00,0x38,0x04,0x3C,0x44,0x3C,0x00 }, /* 0x61 a */
    { 0x40,0x40,0x58,0x64,0x44,0x64,0x58,0x00 }, /* 0x62 b */
    { 0x00,0x00,0x38,0x44,0x40,0x44,0x38,0x00 }, /* 0x63 c */
    { 0x04,0x04,0x34,0x4C,0x44,0x4C,0x34,0x00 }, /* 0x64 d */
    { 0x00,0x00,0x38,0x44,0x7C,0x40,0x38,0x00 }, /* 0x65 e */
    { 0x18,0x20,0x70,0x20,0x20,0x20,0x20,0x00 }, /* 0x66 f */
    { 0x00,0x00,0x3C,0x44,0x44,0x3C,0x04,0x38 }, /* 0x67 g */
    { 0x40,0x40,0x58,0x64,0x44,0x44,0x44,0x00 }, /* 0x68 h */
    { 0x10,0x00,0x30,0x10,0x10,0x10,0x38,0x00 }, /* 0x69 i */
    { 0x08,0x00,0x18,0x08,0x08,0x08,0x48,0x30 }, /* 0x6A j */
    { 0x40,0x40,0x48,0x50,0x60,0x50,0x48,0x00 }, /* 0x6B k */
    { 0x30,0x10,0x10,0x10,0x10,0x10,0x38,0x00 }, /* 0x6C l */
    { 0x00,0x00,0x68,0x54,0x54,0x44,0x44,0x00 }, /* 0x6D m */
    { 0x00,0x00,0x58,0x64,0x44,0x44,0x44,0x00 }, /* 0x6E n */
    { 0x00,0x00,0x38,0x44,0x44,0x44,0x38,0x00 }, /* 0x6F o */
    { 0x00,0x00,0x58,0x64,0x64,0x58,0x40,0x40 }, /* 0x70 p */
    { 0x00,0x00,0x34,0x4C,0x4C,0x34,0x04,0x04 }, /* 0x71 q */
    { 0x00,0x00,0x58,0x60,0x40,0x40,0x40,0x00 }, /* 0x72 r */
    { 0x00,0x00,0x38,0x40,0x38,0x04,0x78,0x00 }, /* 0x73 s */
    { 0x20,0x20,0x70,0x20,0x20,0x24,0x18,0x00 }, /* 0x74 t */
    { 0x00,0x00,0x44,0x44,0x44,0x4C,0x34,0x00 }, /* 0x75 u */
    { 0x00,0x00,0x44,0x44,0x44,0x28,0x10,0x00 }, /* 0x76 v */
    { 0x00,0x00,0x44,0x44,0x54,0x54,0x28,0x00 }, /* 0x77 w */
    { 0x00,0x00,0x44,0x28,0x10,0x28,0x44,0x00 }, /* 0x78 x */
    { 0x00,0x00,0x44,0x44,0x3C,0x04,0x38,0x00 }, /* 0x79 y */
    { 0x00,0x00,0x7C,0x08,0x10,0x20,0x7C,0x00 }, /* 0x7A z */
    { 0x18,0x20,0x20,0x60,0x20,0x20,0x18,0x00 }, /* 0x7B { */
    { 0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00 }, /* 0x7C | */
    { 0x30,0x08,0x08,0x06,0x08,0x08,0x30,0x00 }, /* 0x7D } */
    { 0x00,0x00,0x32,0x4C,0x00,0x00,0x00,0x00 }, /* 0x7E ~ */
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0x7F DEL */
};

/* ------------------------------------------------------------------ */
/* Framebuffer — native pixel operations, no libfb dependency           */
/* ------------------------------------------------------------------ */

class Framebuffer {
	void put(uint8_t *row, int x, uint32_t color) const {
		if (bpp == 32) {
			((uint32_t *)row)[x] = color;
		} else {
			uint8_t *p = row + (uint32_t)x * 3;
			p[0] =  color        & 0xFF;
			p[1] = (color >>  8) & 0xFF;
			p[2] = (color >> 16) & 0xFF;
		}
	}

	uint8_t *row_ptr(int y) const {
		return (uint8_t *)base + (uint32_t)y * pitch;
	}

public:
	uint32_t  width, height, pitch, bpp;
	void     *base;

	int open() {
		struct _fb_map info;
		if (_sys_mapfb(&info) != 0)
			return -1;
		width  = info.width;
		height = info.height;
		pitch  = info.pitch;
		bpp    = info.bpp;
		base   = info.base;
		return 0;
	}

	void pixel(int x, int y, uint32_t color) {
		if (x < 0 || y < 0 ||
		    (uint32_t)x >= width || (uint32_t)y >= height)
			return;
		put(row_ptr(y), x, color);
	}

	void rect(int x, int y, int w, int h, uint32_t color) {
		for (int r = y; r < y + h; r++) {
			if (r < 0 || (uint32_t)r >= height) continue;
			uint8_t *p = row_ptr(r);
			for (int c = x; c < x + w; c++) {
				if (c < 0 || (uint32_t)c >= width) continue;
				put(p, c, color);
			}
		}
	}

	void blit(int dx, int dy, int w, int h,
	    const uint32_t *src, int src_stride) {
		for (int r = 0; r < h; r++) {
			int dst_y = dy + r;
			if (dst_y < 0 || (uint32_t)dst_y >= height) continue;
			uint8_t *dst_row = row_ptr(dst_y);
			const uint32_t *src_row = src + r * src_stride;
			for (int c = 0; c < w; c++) {
				int dst_x = dx + c;
				if (dst_x < 0 || (uint32_t)dst_x >= width)
					continue;
				put(dst_row, dst_x, src_row[c]);
			}
		}
	}

	void ch(int x, int y, char c, uint32_t fg, uint32_t bg) {
		uint8_t idx = (uint8_t)c;
		if (idx < 0x20 || idx > 0x7F) idx = 0x20;
		const uint8_t *glyph = font8x8[idx - 0x20];
		for (int row = 0; row < FB_FONT_H; row++) {
			uint8_t bits = glyph[row];
			for (int col = 0; col < FB_FONT_W; col++) {
				uint32_t color =
				    (bits & (0x80 >> col)) ? fg : bg;
				pixel(x + col, y + row, color);
			}
		}
	}

	void text(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
		int cx = x;
		while (*s) {
			if (*s == '\n') { cx = x; y += FB_FONT_H; }
			else { ch(cx, y, *s, fg, bg); cx += FB_FONT_W; }
			s++;
		}
	}

	uint32_t read(int x, int y) const {
		if (x < 0 || y < 0 ||
		    (uint32_t)x >= width || (uint32_t)y >= height)
			return 0;
		const uint8_t *p = row_ptr(y);
		if (bpp == 32)
			return ((const uint32_t *)p)[x];
		p += (uint32_t)x * 3;
		return (uint32_t)p[0] |
		       ((uint32_t)p[1] << 8) |
		       ((uint32_t)p[2] << 16);
	}
};

/* ------------------------------------------------------------------ */
/* Window                                                               */
/* ------------------------------------------------------------------ */

class Window {
public:
	uint32_t     id;
	int32_t      x, y, w, h;
	uint32_t     pitch;
	void        *buf;   /* page-aligned region shared with client */
	int          decor_h;
	std::string  title;
	std::string  mbox;

	bool hit_test(int cx, int cy) const {
		return cx >= x && cx < x + w &&
		       cy >= y && cy < y + decor_h + h;
	}

	bool in_decor(int cx, int cy) const {
		return decor_h > 0 &&
		       cx >= x && cx < x + w &&
		       cy >= y && cy < y + decor_h;
	}

	bool in_close_btn(int cx, int cy) const {
		return in_decor(cx, cy) && cx >= x + w - decor_h;
	}

	void draw_decor(Framebuffer &fb, bool is_focused) const {
		uint32_t bg = is_focused ? DECOR_BG    : DECOR_BG_INACT;
		uint32_t hi = is_focused ? DECOR_HI    : DECOR_HI_INACT;
		fb.rect(x, y,               w, decor_h, bg);
		fb.rect(x, y,               w, 1,       hi);
		fb.rect(x, y + decor_h - 1, w, 1,       DECOR_SEP);
		fb.text(x + 6, y + (decor_h - FB_FONT_H) / 2,
		    title.c_str(), FB_WHITE, bg);
		int cbx = x + w - decor_h;
		fb.rect(cbx, y + 1, decor_h - 1, decor_h - 2, DECOR_CLOSE_BG);
		fb.ch(cbx + (decor_h - FB_FONT_W) / 2,
		    y + (decor_h - FB_FONT_H) / 2,
		    'X', FB_WHITE, DECOR_CLOSE_BG);
	}

	void blit_to(Framebuffer &fb) const {
		fb.blit(x, y + decor_h, w, h,
		    (const uint32_t *)buf,
		    (int)(pitch / WIN_BPP));
	}
};

/* ------------------------------------------------------------------ */
/* WindowManager                                                        */
/* ------------------------------------------------------------------ */

static const uint8_t cursor_mask[CUR_H][CUR_W] = {
	{ 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2 },
	{ 1, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
};

static const uint32_t jailbar_colors[4] = {
	FB_RGB(0x1A, 0x1A, 0x2E),
	FB_RGB(0x1A, 0x2E, 0x00),
	FB_RGB(0x2E, 0x00, 0x1A),
	FB_RGB(0x00, 0x1A, 0x1A),
};

#define CASCADE_STEP  24
#define CASCADE_START 20

class WindowManager {
	Framebuffer  fb;

	/* Window storage — heap-allocated, stable pointers. */
	std::vector<std::unique_ptr<Window>> windows;

	/* Z-order: back=bottom, front=top. Pointers into windows storage. */
	std::vector<Window *> z_stack;

	uint32_t  next_win_id;
	int       cascade_x, cascade_y;

	Window   *focused;

	bool      dragging;
	Window   *drag_win;
	int       drag_off_x, drag_off_y;

	int       cur_x, cur_y;
	uint32_t  cur_saved[CUR_H * CUR_W];
	bool      cur_drawn;
	uint8_t   prev_buttons;

	/* -- Z-order ---------------------------------------------------- */

	void z_push(Window *w) { z_stack.push_back(w); }

	void z_remove(Window *w) {
		auto it = std::find(z_stack.begin(), z_stack.end(), w);
		if (it != z_stack.end())
			z_stack.erase(it);
	}

	void z_raise(Window *w) { z_remove(w); z_push(w); }

	/* -- Window table ---------------------------------------------- */

	Window *win_alloc() {
		auto w   = std::make_unique<Window>();
		Window *p = w.get();
		windows.push_back(std::move(w));
		return p;
	}

	void win_destroy(Window *w) {
		auto it = std::find_if(windows.begin(), windows.end(),
		    [w](const std::unique_ptr<Window> &p) {
			    return p.get() == w;
		    });
		if (it != windows.end())
			windows.erase(it);
	}

	Window *win_find(uint32_t id) {
		for (auto &w : windows)
			if (w->id == id)
				return w.get();
		return nullptr;
	}

	/* -- Desktop ---------------------------------------------------- */

	void desktop_fill_rect(int x, int y, int w, int h) {
		int x2 = x + w, y2 = y + h;
		if (x  < 0) x  = 0;
		if (y  < 0) y  = 0;
		if (x2 > (int)fb.width)  x2 = (int)fb.width;
		if (y2 > (int)fb.height) y2 = (int)fb.height;
		for (int py = y; py < y2; py++)
			for (int px = x; px < x2; px++)
				fb.pixel(px, py, jailbar_colors[px & 3]);
	}

	void draw_desktop() {
		desktop_fill_rect(0, 0, (int)fb.width, (int)fb.height);
	}

	/* -- Compositing ------------------------------------------------ */

	void reblit_rect(int rx, int ry, int rw, int rh) {
		for (auto *w : z_stack) {
			int cy = w->y + w->decor_h;

			int x1 = (rx    > w->x)       ? rx    : w->x;
			int y1 = (ry    > cy)          ? ry    : cy;
			int x2 = (rx+rw < w->x + w->w) ? rx+rw : w->x + w->w;
			int y2 = (ry+rh < cy  + w->h)  ? ry+rh : cy   + w->h;
			if (x2 > x1 && y2 > y1) {
				int bx = x1 - w->x;
				int by = y1 - cy;
				fb.blit(x1, y1, x2 - x1, y2 - y1,
				    (const uint32_t *)((const uint8_t *)w->buf +
				        (uint32_t)by * w->pitch +
				        (uint32_t)bx * WIN_BPP),
				    (int)(w->pitch / WIN_BPP));
			}

			if (w->decor_h > 0 &&
			    rx < w->x + w->w && rx + rw > w->x &&
			    ry < w->y + w->decor_h && ry + rh > w->y)
				w->draw_decor(fb, w == focused);
		}
	}

	/* -- Cursor ----------------------------------------------------- */

	void cursor_save(int x, int y) {
		for (int row = 0; row < CUR_H; row++) {
			if (y + row < 0 || y + row >= (int)fb.height)
				continue;
			for (int col = 0; col < CUR_W; col++)
				cur_saved[row * CUR_W + col] =
				    fb.read(x + col, y + row);
		}
	}

	void cursor_erase(int x, int y) {
		for (int row = 0; row < CUR_H; row++)
			for (int col = 0; col < CUR_W; col++)
				fb.pixel(x + col, y + row,
				    cur_saved[row * CUR_W + col]);
	}

	void cursor_draw(int x, int y) {
		for (int row = 0; row < CUR_H; row++) {
			for (int col = 0; col < CUR_W; col++) {
				uint8_t m = cursor_mask[row][col];
				if (m == 2)
					continue;
				uint32_t color = (m == 1)
				    ? FB_WHITE
				    : FB_RGB(0x40, 0x40, 0x40);
				fb.pixel(x + col, y + row, color);
			}
		}
	}

	/* -- Mouse helper ---------------------------------------------- */

	void send_mouse(Window *w, int cx, int cy, uint8_t buttons) {
		if (w->mbox.empty())
			return;
		mpi_message_t mev;
		struct display_mouse_ev *me =
		    (struct display_mouse_ev *)mev.data;
		mev.header    = DISPLAY_MOUSE;
		me->window_id = w->id;
		me->x         = (int16_t)(cx - w->x);
		me->y         = (int16_t)(cy - (w->y + w->decor_h));
		me->dx        = 0;
		me->dy        = 0;
		me->buttons   = buttons;
		ubix::post_message(w->mbox, DISPLAY_MOUSE, mev);
	}

	void notify_taskbar(Window *w, uint8_t added) {
		static const char tb[] = "taskbar";
		mpi_message_t nm;
		struct display_notify *dn = (struct display_notify *)nm.data;
		nm.header     = DISPLAY_NOTIFY;
		dn->window_id = w->id;
		dn->added     = added;
		std::strncpy(dn->title, w->title.c_str(), sizeof(dn->title) - 1);
		dn->title[sizeof(dn->title) - 1] = '\0';
		ubix::post_message(tb, DISPLAY_NOTIFY, nm);
	}

	void close_window(Window *w) {
		if (!w->mbox.empty()) {
			mpi_message_t cm;
			struct display_close *dc =
			    (struct display_close *)cm.data;
			cm.header     = DISPLAY_CLOSE;
			dc->window_id = w->id;
			ubix::post_message(w->mbox, DISPLAY_CLOSE, cm);
		}
		if (w->decor_h > 0)
			notify_taskbar(w, 0);
		std::free(w->buf);
		z_remove(w);
		focused = z_stack.empty() ? nullptr : z_stack.back();
		win_destroy(w);   /* w is dangling after this point */
		composite_all();
	}

public:
	WindowManager()
	    : next_win_id(1), cascade_x(CASCADE_START), cascade_y(CASCADE_START),
	      focused(nullptr),
	      dragging(false), drag_win(nullptr),
	      drag_off_x(0), drag_off_y(0),
	      cur_x(0), cur_y(0), cur_drawn(false), prev_buttons(0)
	{}

	int init() { return fb.open(); }

	void startup() {
		draw_desktop();
		cur_x = (int)fb.width  / 2;
		cur_y = (int)fb.height / 2;
		cursor_save(cur_x, cur_y);
		cursor_draw(cur_x, cur_y);
		cur_drawn = true;
	}

	void composite_all() {
		draw_desktop();
		for (auto *w : z_stack) {
			w->blit_to(fb);
			if (w->decor_h > 0)
				w->draw_decor(fb, w == focused);
		}
		cursor_save(cur_x, cur_y);
		cursor_draw(cur_x, cur_y);
		cur_drawn = true;
	}

	void cursor_move(int dx, int dy) {
		int old_x = cur_x, old_y = cur_y;

		if (cur_drawn)
			cursor_erase(old_x, old_y);

		cur_x += dx;
		cur_y += dy;
		if (cur_x < 0) cur_x = 0;
		if (cur_y < 0) cur_y = 0;
		if (cur_x >= (int)fb.width  - CUR_W)
			cur_x = (int)fb.width  - CUR_W;
		if (cur_y >= (int)fb.height - CUR_H)
			cur_y = (int)fb.height - CUR_H;

		desktop_fill_rect(old_x, old_y, CUR_W, CUR_H);
		reblit_rect(old_x, old_y, CUR_W, CUR_H);

		cursor_save(cur_x, cur_y);
		cursor_draw(cur_x, cur_y);
		cur_drawn = true;
	}

	void handle_query(struct display_query *dq) {
		if (!dq->reply[0])
			return;
		mpi_message_t info;
		struct display_info *di = (struct display_info *)info.data;
		info.header  = DISPLAY_INFO;
		di->screen_w = fb.width;
		di->screen_h = fb.height;
		di->bpp      = (uint8_t)fb.bpp;
		ubix::post_message(dq->reply, DISPLAY_INFO, info);
	}

	void handle_claim(struct display_claim_req *creq) {
		mpi_message_t ack;
		struct display_ack *da = (struct display_ack *)ack.data;

		auto deny = [&]() {
			ack.header = DISPLAY_DENIED;
			if (creq->reply[0])
				ubix::post_message(creq->reply,
				    DISPLAY_DENIED, ack);
		};

		if (creq->sender_pid <= 0 || creq->w <= 0 || creq->h <= 0) {
			deny(); return;
		}

		int dh = creq->no_decor ? 0 : DECOR_H;

		int32_t wx = creq->x, wy = creq->y;
		int32_t ww = creq->w, wh = creq->h;

		if (dh > 0) {
			wx = (int32_t)cascade_x;
			wy = (int32_t)cascade_y;
			cascade_x += CASCADE_STEP;
			cascade_y += CASCADE_STEP;
			if (cascade_x + ww > (int32_t)fb.width ||
			    cascade_y + dh + wh > (int32_t)fb.height) {
				cascade_x = CASCADE_START;
				cascade_y = CASCADE_START;
			}
		}

		if (wx < 0) wx = 0;
		if (wy < 0) wy = 0;
		if (wx + ww > (int32_t)fb.width)
			ww = (int32_t)fb.width - wx;
		if (wy + dh + wh > (int32_t)fb.height)
			wh = (int32_t)fb.height - wy - dh;

		uint32_t pitch    = (uint32_t)ww * WIN_BPP;
		uint32_t buf_size = pitch * (uint32_t)wh;
		/* Over-allocate so we can align to a page boundary.
		 * vmm_share_region requires a page-aligned src address. */
		uint32_t alloc_size = (buf_size + PAGE_SIZE - 1)
		    & ~(uint32_t)(PAGE_SIZE - 1);
		void *buf = std::aligned_alloc(PAGE_SIZE, alloc_size);
		if (!buf) { deny(); return; }
		std::memset(buf, 0, buf_size);

		uint32_t client_vaddr = 0;
		if (share_buffer(creq->sender_pid, buf,
		    buf_size, &client_vaddr) != 0) {
			std::free(buf); deny(); return;
		}

		Window *w  = win_alloc();
		w->id      = next_win_id++;
		w->x       = wx;   w->y = wy;
		w->w       = ww;   w->h = wh;
		w->pitch   = pitch;
		w->buf     = buf;
		w->decor_h = dh;
		w->title   = creq->title;
		w->mbox    = creq->reply;

		z_push(w);
		focused = w;

		ack.header    = DISPLAY_ACK;
		da->window_id = w->id;
		da->shm_base  = (void *)client_vaddr;
		da->pitch     = (uint16_t)pitch;
		da->x = wx;  da->y = wy + dh;
		da->w = ww;  da->h = wh;
		if (creq->reply[0])
			ubix::post_message(creq->reply, DISPLAY_ACK, ack);

		std::printf("views: window %u pid %d %dx%d+%d+%d shm=0x%X\n",
		    w->id, creq->sender_pid, ww, wh, wx, wy, client_vaddr);

		if (dh > 0)
			notify_taskbar(w, 1);
		composite_all();
	}

	void partial_composite(int sx, int sy, int sw, int sh) {
		desktop_fill_rect(sx, sy, sw, sh);
		reblit_rect(sx, sy, sw, sh);
		if (cur_x < sx + sw && cur_x + CUR_W > sx &&
		    cur_y < sy + sh && cur_y + CUR_H > sy) {
			cursor_save(cur_x, cur_y);
			cursor_draw(cur_x, cur_y);
			cur_drawn = true;
		}
	}

	void handle_flip(struct display_flip *fl) {
		Window *w = win_find(fl->window_id);
		if (!w)
			return;
		if (fl->dirty_w > 0 && fl->dirty_h > 0) {
			int sx = w->x + (int)fl->dirty_x;
			int sy = w->y + w->decor_h + (int)fl->dirty_y;
			partial_composite(sx, sy,
			    (int)fl->dirty_w, (int)fl->dirty_h);
		} else {
			composite_all();
		}
	}

	void handle_release(struct display_release *rel) {
		Window *w = win_find(rel->window_id);
		if (!w) return;
		if (w->decor_h > 0)
			notify_taskbar(w, 0);
		std::free(w->buf);
		z_remove(w);
		if (focused == w)
			focused = z_stack.empty() ? nullptr : z_stack.back();
		win_destroy(w);   /* w is dangling after this point */
		composite_all();
	}

	void handle_mouse(mouse_event_t &ev) {
		cursor_move(ev.dx, ev.dy);

		if (dragging && drag_win && (ev.dx || ev.dy)) {
			drag_win->x = cur_x - drag_off_x;
			drag_win->y = cur_y - drag_off_y;
			if (drag_win->x < 0) drag_win->x = 0;
			if (drag_win->y < 0) drag_win->y = 0;
			composite_all();
		}

		if (ev.buttons == prev_buttons)
			return;

		bool pressed = (ev.buttons & 1) != 0;

		if (!pressed && dragging) {
			dragging = false;
			drag_win = nullptr;
		}

		if (pressed) {
			Window *hit = nullptr;
			for (auto it = z_stack.rbegin();
			    it != z_stack.rend(); ++it) {
				if ((*it)->hit_test(cur_x, cur_y)) {
					hit = *it;
					break;
				}
			}
			if (hit) {
				z_raise(hit);
				focused = hit;
				composite_all();

				if (hit->in_close_btn(cur_x, cur_y)) {
					close_window(hit);
				} else if (hit->in_decor(cur_x, cur_y)) {
					dragging   = true;
					drag_win   = hit;
					drag_off_x = cur_x - hit->x;
					drag_off_y = cur_y - hit->y;
				} else {
					send_mouse(hit, cur_x, cur_y,
					    ev.buttons);
				}
			}
		} else {
			if (focused)
				send_mouse(focused, cur_x, cur_y, ev.buttons);
		}

		prev_buttons = ev.buttons;
	}

	void handle_raise(struct display_raise *dr) {
		Window *w = win_find(dr->window_id);
		if (!w) return;
		z_raise(w);
		focused = w;
		composite_all();
	}

	void handle_kbd(kbd_event_t &kev) {
		if (!focused || focused->mbox.empty())
			return;
		mpi_message_t kmsg;
		struct display_key *dk = (struct display_key *)kmsg.data;
		kmsg.header   = DISPLAY_KEY;
		dk->window_id = focused->id;
		dk->keycode   = kev.keycode;
		dk->pressed   = kev.pressed;
		ubix::post_message(focused->mbox, DISPLAY_KEY, kmsg);
	}
};

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	ubix::Mailbox g_mbox;
	g_mbox.assign("views");
	if (!g_mbox.create()) {
		std::printf("views: mpi_createMbox failed\n");
		return 1;
	}

	mpi_message_t req;
	req.header  = 0x82;
	req.data[0] = 'v'; req.data[1] = 'i'; req.data[2] = 'e';
	req.data[3] = 'w'; req.data[4] = 's'; req.data[5] = '\0';
	ubix::post_message("system", 0x82, req);

	mpi_message_t reply;
	while (!g_mbox.try_fetch(reply))
		ubix::yield();

	if (reply.data[0] == 0) {
		std::printf("views: VESA init failed\n");
		return 1;
	}

	WindowManager wm;
	if (wm.init() != 0) {
		std::printf("views: fb_open failed\n");
		return 1;
	}
	wm.startup();

	/* Launch taskbar */
	{
		int pid = ::fork();
		if (pid == 0) {
			char *argv_tb[] = { (char *)"taskbar", nullptr };
			char *envp_tb[] = { nullptr };
			::execve("sys:/bin/taskbar", argv_tb, envp_tb);
			std::printf("views: failed to exec taskbar\n");
			std::exit(1);
		}
	}

	mpi_message_t msg;

	for (;;) {
		kbd_event_t kev;
		while (poll_kbd(&kev) == 0)
			wm.handle_kbd(kev);

		mouse_event_t ev;
		while (poll_mouse(&ev) == 0)
			wm.handle_mouse(ev);

		while (g_mbox.try_fetch(msg)) {
			switch (msg.header) {
			case DISPLAY_QUERY:
				wm.handle_query(
				    (struct display_query *)msg.data);
				break;
			case DISPLAY_CLAIM:
				wm.handle_claim(
				    (struct display_claim_req *)msg.data);
				break;
			case DISPLAY_FLIP:
				wm.handle_flip(
				    (struct display_flip *)msg.data);
				break;
			case DISPLAY_RELEASE:
				wm.handle_release(
				    (struct display_release *)msg.data);
				break;
			case DISPLAY_RAISE:
				wm.handle_raise(
				    (struct display_raise *)msg.data);
				break;
			default:
				break;
			}
		}

		ubix::yield();
	}

	return 0;
}
