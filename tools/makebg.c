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
 * makebg — host tool that generates the stock desktop backgrounds as 24bpp
 * BMPs (vertical gradients), installed to /var/background by tools/mkimage.sh.
 * The compositor stretches them to fill the screen, so they are kept small.
 * Run from tools/:  cc -o /tmp/makebg makebg.c && (cd tools && /tmp/makebg)
 */

#include <stdio.h>
#include <stdint.h>

#define BG_W 256
#define BG_H 192

static void put_le32(FILE *f, uint32_t v)
{
	fputc(v & 0xFF, f);
	fputc((v >> 8) & 0xFF, f);
	fputc((v >> 16) & 0xFF, f);
	fputc((v >> 24) & 0xFF, f);
}

static void put_le16(FILE *f, uint16_t v)
{
	fputc(v & 0xFF, f);
	fputc((v >> 8) & 0xFF, f);
}

/**
 * Write a BG_W x BG_H 24bpp BMP that is a vertical gradient from the top colour
 * (r0,g0,b0) to the bottom colour (r1,g1,b1).
 */
static void write_gradient(const char *name, int r0, int g0, int b0, int r1, int g1, int b1)
{
	FILE *f = fopen(name, "wb");
	int rowbytes = BG_W * 3;
	int pad = (4 - (rowbytes & 3)) & 3;
	int imgsize = (rowbytes + pad) * BG_H;
	int y, x, p;

	if (f == NULL)
	{
		perror(name);
		return;
	}

	/* BITMAPFILEHEADER */
	fputc('B', f);
	fputc('M', f);
	put_le32(f, 54 + (uint32_t)imgsize);
	put_le32(f, 0);
	put_le32(f, 54);
	/* BITMAPINFOHEADER */
	put_le32(f, 40);
	put_le32(f, BG_W);
	put_le32(f, BG_H);
	put_le16(f, 1);
	put_le16(f, 24);
	put_le32(f, 0);
	put_le32(f, (uint32_t)imgsize);
	put_le32(f, 2835);
	put_le32(f, 2835);
	put_le32(f, 0);
	put_le32(f, 0);

	/* Pixel rows, bottom-up (BMP convention); y is the image row. */
	for (y = BG_H - 1; y >= 0; y--)
	{
		int t = (BG_H > 1) ? y * 255 / (BG_H - 1) : 0;
		int r = r0 + (r1 - r0) * t / 255;
		int g = g0 + (g1 - g0) * t / 255;
		int b = b0 + (b1 - b0) * t / 255;
		for (x = 0; x < BG_W; x++)
		{
			fputc((uint8_t)b, f); /* BMP stores B,G,R */
			fputc((uint8_t)g, f);
			fputc((uint8_t)r, f);
		}
		for (p = 0; p < pad; p++)
			fputc(0, f);
	}

	fclose(f);
	printf("wrote %s (%dx%d)\n", name, BG_W, BG_H);
}

int main(void)
{
	write_gradient("./backgrounds/aqua.bmp", 0x10, 0x22, 0x44, 0x2C, 0x60, 0xA8);     /* navy -> blue   */
	write_gradient("./backgrounds/graphite.bmp", 0x20, 0x24, 0x2A, 0x44, 0x4A, 0x54); /* dark -> grey   */
	write_gradient("./backgrounds/dusk.bmp", 0x2A, 0x10, 0x3A, 0x80, 0x30, 0x70);     /* purple -> rose */
	return (0);
}
