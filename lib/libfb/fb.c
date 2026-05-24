/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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

#include <fb/fb.h>
#include <stdint.h>
#include <sys/mouse.h>
#include <sys/kbd.h>

extern const uint8_t fb_font8x8[96][8];

/*
 * _sys_mapfb — UbixOS native syscall 43 (int $0x81).
 * Arg: pointer to struct fb_info on the caller's stack.
 * The kernel reads args at [esp+4] from the call frame, which is exactly
 * where C puts the first argument when calling this function.
 */
asm(
  ".text                            \n"
  ".globl _sys_mapfb                \n"
  ".type  _sys_mapfb, @function     \n"
  "_sys_mapfb:                      \n"
  "  movl $43, %eax                 \n"
  "  int  $0x81                     \n"
  "  ret                            \n"
);

static int _sys_mapfb(struct fb_info *info);

static struct fb_info _fb;
static int            _fb_ready;

int
fb_open(struct fb_info *out)
{
	int ret = _sys_mapfb(&_fb);
	if (ret != 0)
		return ret;
	_fb_ready = 1;
	if (out)
		*out = _fb;
	return 0;
}

static inline uint8_t *
_fb_row(int y)
{
	return (uint8_t *)_fb.base + (uint32_t)y * _fb.pitch;
}

static inline void
_fb_put(uint8_t *row, int x, uint32_t color)
{
	if (_fb.bpp == 32) {
		((uint32_t *)row)[x] = color;
	} else {
		uint8_t *p = row + (uint32_t)x * 3;
		p[0] = color & 0xFF;
		p[1] = (color >> 8) & 0xFF;
		p[2] = (color >> 16) & 0xFF;
	}
}

void
fb_pixel(int x, int y, uint32_t color)
{
	if (!_fb_ready || x < 0 || y < 0 ||
	    (uint32_t)x >= _fb.width || (uint32_t)y >= _fb.height)
		return;
	_fb_put(_fb_row(y), x, color);
}

void
fb_clear(uint32_t color)
{
	if (!_fb_ready)
		return;
	for (uint32_t y = 0; y < _fb.height; y++) {
		uint8_t *row = _fb_row(y);
		for (uint32_t x = 0; x < _fb.width; x++)
			_fb_put(row, x, color);
	}
}

void
fb_rect(int x, int y, int w, int h, uint32_t color)
{
	if (!_fb_ready)
		return;
	for (int row = y; row < y + h; row++) {
		if (row < 0 || (uint32_t)row >= _fb.height)
			continue;
		uint8_t *p = _fb_row(row);
		for (int col = x; col < x + w; col++) {
			if (col < 0 || (uint32_t)col >= _fb.width)
				continue;
			_fb_put(p, col, color);
		}
	}
}

void
fb_rect_outline(int x, int y, int w, int h, uint32_t color)
{
	fb_rect(x,         y,         w, 1, color);
	fb_rect(x,         y + h - 1, w, 1, color);
	fb_rect(x,         y,         1, h, color);
	fb_rect(x + w - 1, y,         1, h, color);
}

void
fb_blit(int dx, int dy, int w, int h, const uint32_t *src, int src_stride)
{
	if (!_fb_ready)
		return;
	for (int row = 0; row < h; row++) {
		int dst_y = dy + row;
		if (dst_y < 0 || (uint32_t)dst_y >= _fb.height)
			continue;
		uint8_t *dst_row = _fb_row(dst_y);
		const uint32_t *src_row = src + row * src_stride;
		for (int col = 0; col < w; col++) {
			int dst_x = dx + col;
			if (dst_x < 0 || (uint32_t)dst_x >= _fb.width)
				continue;
			_fb_put(dst_row, dst_x, src_row[col]);
		}
	}
}

void
fb_char(int x, int y, char c, uint32_t fg, uint32_t bg)
{
	if (!_fb_ready)
		return;
	uint8_t idx = (uint8_t)c;
	if (idx < 0x20 || idx > 0x7F)
		idx = 0x20;
	const uint8_t *glyph = fb_font8x8[idx - 0x20];
	for (int row = 0; row < FB_FONT_H; row++) {
		uint8_t bits = glyph[row];
		for (int col = 0; col < FB_FONT_W; col++) {
			uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
			fb_pixel(x + col, y + row, color);
		}
	}
}

void
fb_text(int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
	if (!_fb_ready || !s)
		return;
	int cx = x;
	while (*s) {
		if (*s == '\n') {
			cx = x;
			y += FB_FONT_H;
		} else {
			fb_char(cx, y, *s, fg, bg);
			cx += FB_FONT_W;
		}
		s++;
	}
}

/*
 * fb_set_target — redirect all fb_* drawing calls to an arbitrary buffer.
 * Used by display clients (e.g. taskbar) to render into their shared window
 * buffer without calling fb_open (which would map the raw framebuffer).
 */
void
fb_set_target(void *base, uint32_t width, uint32_t height,
    uint32_t pitch, uint8_t bpp)
{
	_fb.base   = base;
	_fb.width  = width;
	_fb.height = height;
	_fb.pitch  = pitch;
	_fb.bpp    = bpp;
	_fb_ready  = 1;
}

/*
 * _sys_shareregion — UbixOS native syscall 45 (int $0x81).
 * Shares [vaddr, vaddr+size) from the calling process into dst_pid's space.
 * Writes the client's virtual address into *out_vaddr.
 * Returns 0 on success, -1 on failure.
 */
asm(
  ".text                                \n"
  ".globl _sys_shareregion              \n"
  ".type  _sys_shareregion, @function   \n"
  "_sys_shareregion:                    \n"
  "  movl $45, %eax                     \n"
  "  int  $0x81                         \n"
  "  ret                                \n"
);

static int _sys_shareregion(int dst_pid, void *vaddr,
    uint32_t size, uint32_t *out_vaddr);

int
fb_share_buffer(int dst_pid, void *buf, uint32_t size, uint32_t *client_vaddr)
{
	return _sys_shareregion(dst_pid, buf, size, client_vaddr);
}

/*
 * _sys_getkbd — UbixOS native syscall 46 (int $0x81).
 * Returns 0 and fills *ev if an event is available, -1 if queue empty.
 */
asm(
  ".text                              \n"
  ".globl _sys_getkbd                 \n"
  ".type  _sys_getkbd, @function      \n"
  "_sys_getkbd:                       \n"
  "  movl $46, %eax                   \n"
  "  int  $0x81                       \n"
  "  ret                              \n"
);

static int _sys_getkbd(kbd_event_t *ev);

int
fb_poll_kbd(kbd_event_t *ev)
{
	return _sys_getkbd(ev);
}

/*
 * _sys_getmouse — UbixOS native syscall 44 (int $0x81).
 * Returns 0 and fills *ev if an event is available, -1 if queue empty.
 */
asm(
  ".text                              \n"
  ".globl _sys_getmouse               \n"
  ".type  _sys_getmouse, @function    \n"
  "_sys_getmouse:                     \n"
  "  movl $44, %eax                   \n"
  "  int  $0x81                       \n"
  "  ret                              \n"
);

static int _sys_getmouse(mouse_event_t *ev);

int
fb_poll_mouse(mouse_event_t *ev)
{
	return _sys_getmouse(ev);
}
