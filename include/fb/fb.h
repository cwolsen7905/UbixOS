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

#ifndef _FB_H
#define _FB_H

#include <stdint.h>
#include <sys/mouse.h>

struct fb_info {
    void     *base;
    uint32_t  width;
    uint32_t  height;
    uint16_t  pitch;
    uint8_t   bpp;
};

/* Colour helpers: pack 8-bit R, G, B into a 32-bit pixel (32bpp BGRX) */
#define FB_RGB(r,g,b)  (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define FB_BLACK       0x00000000U
#define FB_WHITE       0x00FFFFFFU
#define FB_GREY        0x00808080U
#define FB_DARK_GREY   0x00404040U
#define FB_BLUE        0x000000FFU
#define FB_UBIX_BLUE   0x00003C8CU   /* UbixOS brand blue */

int  fb_open(struct fb_info *out);
void fb_clear(uint32_t color);
void fb_pixel(int x, int y, uint32_t color);
void fb_rect(int x, int y, int w, int h, uint32_t color);
void fb_rect_outline(int x, int y, int w, int h, uint32_t color);
void fb_blit(int dx, int dy, int w, int h, const uint32_t *src, int src_stride);
void fb_text(int x, int y, const char *s, uint32_t fg, uint32_t bg);
void fb_char(int x, int y, char c, uint32_t fg, uint32_t bg);
int  fb_poll_mouse(mouse_event_t *ev);

/*
 * fb_share_buffer — share a buffer allocated by this process with another
 * process identified by dst_pid.  The kernel maps the same physical pages
 * into dst_pid's virtual address space and writes the client's virtual
 * address into *client_vaddr.  Returns 0 on success, -1 on failure.
 */
int  fb_share_buffer(int dst_pid, void *buf, uint32_t size, uint32_t *client_vaddr);

/*
 * fb_set_target — redirect all fb_* drawing calls to an arbitrary buffer
 * instead of the framebuffer.  Used by display clients to render into their
 * shared window buffer without calling fb_open.
 */
void fb_set_target(void *base, uint32_t width, uint32_t height,
    uint32_t pitch, uint8_t bpp);

/* Font metrics */
#define FB_FONT_W  8
#define FB_FONT_H  8

#endif /* _FB_H */
