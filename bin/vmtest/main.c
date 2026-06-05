/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * vmtest — minimal reproducer for the "Non Mapped" heap fault seen in nsfb.
 *
 * Mimics what the NetSurf font loader does: malloc() a large (~700 KB) buffer
 * and write the whole thing (as fread/memcpy would), repeatedly, without
 * freeing — so the heap grows the same way loading several fonts does.
 *
 * Expected-bad behaviour: one large alloc+write succeeds, a later one faults
 * "Non Mapped" inside the write because malloc handed out heap/anon memory the
 * page-fault handler does not demand-fill.  Each buffer's malloc address is
 * printed BEFORE it is written, so the kernel's fault address can be matched to
 * the exact allocation (fault_addr - malloc_addr = byte offset that was not
 * backed).  Output is unbuffered so the last line printed is the one that
 * faulted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBI_NO_SIMD
#include "stb_truetype.h"

#define BUF_SIZE (700 * 1024) /* ~ a DejaVu face; > musl's mmap threshold */
#define NBUFS 12              /* ~ the 12 font faces nsfb loads */

int main(void)
{
	int i;

	setvbuf(stdout, NULL, _IONBF, 0);

	printf("vmtest: malloc(%d)+write x%d, no free (mimics nsfb font load)\n", BUF_SIZE, NBUFS);

	for (i = 0; i < NBUFS; i++) {
		unsigned char *p = malloc(BUF_SIZE);
		printf("buf %2d: malloc = %p .. %p\n", i, (void *)p, (void *)(p ? p + BUF_SIZE : 0));
		if (p == NULL) {
			printf("        malloc returned NULL — clean OOM, not the bug\n");
			break;
		}

		/* One big write of the whole buffer — exactly what fread/memcpy does
		 * when loading a font.  If a page in this range is not backed, the
		 * fault happens HERE; the kernel's fault address minus the malloc
		 * address printed above is the byte offset that was not mapped. */
		memset(p, i, BUF_SIZE);
		printf("        wrote %d bytes OK\n", BUF_SIZE);
	}

	printf("vmtest: completed all %d allocations OK (no fault)\n\n", NBUFS);

	/* Phase 2: read + validate each font file exactly as the font loader does
	 * (fopen/ftell/fread), with NO stb_truetype involved.  If this faults or
	 * reports a bad/short read or wrong magic, the bug is in the FAT file read
	 * — not stbtt.  If every file reads cleanly with a valid sfnt magic, then
	 * the nsfb crash is inside stb_truetype's parse/raster. */
	static const char *const fonts[] = {
		"SANS.TTF", "SANSB.TTF", "SANSI.TTF", "SANSBI.TTF",
		"SERIF.TTF", "SERIFB.TTF", "SERIFI.TTF", "SERIFBI.TTF",
		"MONO.TTF", "MONOB.TTF", "MONOI.TTF", "MONOBI.TTF",
	};
	printf("vmtest: reading %d font files from /usr/local/share/netsurf\n",
	       (int)(sizeof(fonts) / sizeof(fonts[0])));
	for (i = 0; i < (int)(sizeof(fonts) / sizeof(fonts[0])); i++) {
		char path[128];
		FILE *f;
		long len;
		unsigned char *p;
		size_t got;

		snprintf(path, sizeof(path), "/usr/local/share/netsurf/%s", fonts[i]);
		f = fopen(path, "rb");
		if (f == NULL) {
			printf("  %-10s fopen FAILED\n", fonts[i]);
			continue;
		}
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fseek(f, 0, SEEK_SET);
		p = malloc(len > 0 ? (size_t)len : 1);
		printf("  %-10s len=%-7ld buf=%p ", fonts[i], len, (void *)p);
		got = fread(p, 1, (size_t)len, f);
		fclose(f);
		/* sfnt magic: 00 01 00 00 (TrueType) or 'OTTO'/'true'/'ttcf'. */
		printf("read=%-7zu magic=%02X%02X%02X%02X %s\n",
		       got, p[0], p[1], p[2], p[3],
		       (got == (size_t)len &&
			((p[0] == 0 && p[1] == 1 && p[2] == 0 && p[3] == 0) ||
			 p[0] == 'O' || p[0] == 't')) ? "OK" : "*** BAD ***");
		free(p);
	}

	printf("\nvmtest: stb_truetype InitFont + render on each font\n");
	for (i = 0; i < (int)(sizeof(fonts) / sizeof(fonts[0])); i++) {
		char path[128];
		FILE *f;
		long len;
		unsigned char *buf;
		stbtt_fontinfo info;
		int off, ok, w = 0, h = 0, xo = 0, yo = 0;
		float scale;
		unsigned char *bm;

		snprintf(path, sizeof(path), "/usr/local/share/netsurf/%s", fonts[i]);
		f = fopen(path, "rb");
		if (f == NULL) {
			printf("  %-10s fopen FAILED\n", fonts[i]);
			continue;
		}
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fseek(f, 0, SEEK_SET);
		buf = malloc((size_t)len);
		if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
			printf("  %-10s short read\n", fonts[i]);
			fclose(f);
			free(buf);
			continue;
		}
		fclose(f);

		/* Print each step BEFORE doing it; with unbuffered stdout the last
		 * thing on screen is whatever faulted. */
		printf("  %-10s offset...", fonts[i]);
		off = stbtt_GetFontOffsetForIndex(buf, 0);
		printf(" %d  init...", off);
		ok = stbtt_InitFont(&info, buf, off < 0 ? 0 : off);
		printf(" %d  scale...", ok);
		scale = stbtt_ScaleForPixelHeight(&info, 16.0f);
		printf(" %.4f  render 'A'...", (double)scale);
		bm = stbtt_GetCodepointBitmap(&info, scale, scale, 'A', &w, &h, &xo, &yo);
		printf(" w=%d h=%d  OK\n", w, h);
		free(bm);
		free(buf);
	}

	printf("vmtest: done\n");
	return 0;
}
