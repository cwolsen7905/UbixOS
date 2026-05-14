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

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mpi.h>
#include <sys/sched.h>
#include <views/display_proto.h>
}
#include <objgfx/objgfx.h>

#define WIN_W   800
#define WIN_H   600

/* Pack 8-bit R,G,B into 0x00RRGGBB used by ogSurface */
#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

static uint32_t  win_id;
static void     *win_buf;
static int32_t   win_w, win_h;
static ogSurface g_surf;

/* BMP pixel cache (24bpp → 32bpp converted) */
static uint32_t *bmp_pixels;
static int       bmp_w, bmp_h;

static char mbox_muffin[] = "muffin";
static char mbox_views[]  = "views";

static int
connect_to_views(void)
{
	mpi_message_t msg;
	mpi_message_t reply;
	struct display_claim_req *req;
	struct display_ack       *ack;

	if (mpi_createMbox(mbox_muffin) != 0) {
		printf("muffin: mpi_createMbox failed\n");
		return -1;
	}

	memset(&msg, 0, sizeof(msg));
	msg.header = DISPLAY_CLAIM;
	req = (struct display_claim_req *)msg.data;

	req->x          = 0;
	req->y          = 0;
	req->w          = WIN_W;
	req->h          = WIN_H;
	req->sender_pid = getpid();
	strncpy(req->title, "muffin", sizeof(req->title) - 1);
	strncpy(req->reply, "muffin", sizeof(req->reply) - 1);

	while (mpi_postMessage(mbox_views, DISPLAY_CLAIM, &msg) != 0)
		sched_yield();

	while (mpi_fetchMessage(mbox_muffin, &reply) != 0)
		sched_yield();

	if (reply.header != DISPLAY_ACK) {
		printf("muffin: DISPLAY_DENIED\n");
		return -1;
	}

	ack     = (struct display_ack *)reply.data;
	win_id  = ack->window_id;
	win_buf = ack->shm_base;
	win_w   = ack->w;
	win_h   = ack->h;

	printf("muffin: window %u %dx%d buf=0x%X\n",
	    win_id, win_w, win_h, (uint32_t)(uintptr_t)win_buf);

	g_surf.ogAttach(win_buf, (uint32_t)win_w, (uint32_t)win_h, OG_PIXFMT_32BPP);
	return 0;
}

static void
do_flip(int dx, int dy, int dw, int dh)
{
	mpi_message_t msg;
	struct display_flip *fl;

	msg.header    = DISPLAY_FLIP;
	fl            = (struct display_flip *)msg.data;
	fl->window_id = win_id;
	fl->dirty_x   = dx;
	fl->dirty_y   = dy;
	fl->dirty_w   = dw;
	fl->dirty_h   = dh;
	mpi_postMessage(mbox_views, DISPLAY_FLIP, &msg);
}

static void
load_bmp(const char *path)
{
	uint8_t hdr[54];
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("muffin: cannot open %s\n", path);
		return;
	}

	if (read(fd, hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
		printf("muffin: bad BMP header\n");
		close(fd);
		return;
	}

	int32_t  w   = (int32_t)(hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24));
	int32_t  h   = (int32_t)(hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24));
	uint16_t bpp = (uint16_t)(hdr[28] | (hdr[29] << 8));

	if (bpp != 24) {
		printf("muffin: only 24bpp BMP supported (got %d)\n", bpp);
		close(fd);
		return;
	}

	uint32_t data_offset = (uint32_t)(hdr[10] | (hdr[11] << 8) |
	    (hdr[12] << 16) | (hdr[13] << 24));
	lseek(fd, (int)data_offset, SEEK_SET);

	uint32_t  row_bytes = (uint32_t)(((w * 3) + 3) & ~3);
	uint8_t  *row_buf   = (uint8_t *)malloc(row_bytes);
	if (!row_buf) { close(fd); return; }

	bmp_pixels = (uint32_t *)malloc((uint32_t)(w * h) * sizeof(uint32_t));
	if (!bmp_pixels) { free(row_buf); close(fd); return; }

	bmp_w = w;
	bmp_h = h;

	for (int row = h - 1; row >= 0; row--) {
		if (read(fd, row_buf, row_bytes) != (int)row_bytes)
			break;
		uint32_t *dst = bmp_pixels + row * w;
		for (int col = 0; col < w; col++) {
			uint8_t b = row_buf[col * 3 + 0];
			uint8_t g = row_buf[col * 3 + 1];
			uint8_t r = row_buf[col * 3 + 2];
			dst[col] = RGB(r, g, b);
		}
	}

	free(row_buf);
	close(fd);
	printf("muffin: loaded %s (%dx%d)\n", path, w, h);
}

static void
draw_frame(int r, int g, int b)
{
	g_surf.ogFillRect(0, 0, win_w - 1, win_h - 1, (uint32_t)RGB(r, g, b));

	if (bmp_pixels) {
		int bw = bmp_w < win_w ? bmp_w : win_w;
		int bh = bmp_h < win_h ? bmp_h : win_h;
		for (int y = 0; y < bh; y++) {
			for (int x = 0; x < bw; x++)
				g_surf.ogSetPixel(x, y, bmp_pixels[y * bmp_w + x]);
		}
	}

	g_surf.ogFillRect(50,  50, 99,  99,  RGB(255,   0,   0));
	g_surf.ogFillRect(100, 50, 149, 99,  RGB(  0, 255,   0));
	g_surf.ogFillRect(150, 50, 199, 99,  RGB(  0,   0, 255));
	g_surf.ogFillRect(200, 50, 249, 99,  RGB(  0,   0,   0));
	g_surf.ogFillRect(250, 50, 299, 99,  RGB(255, 255, 255));

	do_flip(0, 0, win_w, win_h);
}

int
main(int argc, char **argv)
{
	(void)argc; (void)argv;

	if (connect_to_views() != 0)
		return 1;

	load_bmp("/var/background/ubix.bmp");

	int rv = 2, gv = 0, bv = 0;
	for (;;) {
		for (rv = 2; rv < 255; rv += 16)
			for (gv = 0; gv < 255; gv += 16)
				for (bv = 0; bv < 255; bv += 16)
					draw_frame(rv, gv, bv);
	}

	return 0;
}
