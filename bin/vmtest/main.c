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
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

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

	for (i = 0; i < NBUFS; i++)
	{
		unsigned char *p = malloc(BUF_SIZE);
		printf("buf %2d: malloc = %p .. %p\n", i, (void *)p, (void *)(p ? p + BUF_SIZE : 0));
		if (p == NULL)
		{
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
	    "SANS.TTF",
	    "SANSB.TTF",
	    "SANSI.TTF",
	    "SANSBI.TTF",
	    "SERIF.TTF",
	    "SERIFB.TTF",
	    "SERIFI.TTF",
	    "SERIFBI.TTF",
	    "MONO.TTF",
	    "MONOB.TTF",
	    "MONOI.TTF",
	    "MONOBI.TTF",
	};
	printf("vmtest: reading %d font files from /usr/local/share/netsurf\n",
	       (int)(sizeof(fonts) / sizeof(fonts[0])));
	for (i = 0; i < (int)(sizeof(fonts) / sizeof(fonts[0])); i++)
	{
		char path[128];
		FILE *f;
		long len;
		unsigned char *p;
		size_t got;

		snprintf(path, sizeof(path), "/usr/local/share/netsurf/%s", fonts[i]);
		f = fopen(path, "rb");
		if (f == NULL)
		{
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
		       got,
		       p[0],
		       p[1],
		       p[2],
		       p[3],
		       (got == (size_t)len &&
		        ((p[0] == 0 && p[1] == 1 && p[2] == 0 && p[3] == 0) || p[0] == 'O' || p[0] == 't'))
		           ? "OK"
		           : "*** BAD ***");
		free(p);
	}

	printf("\nvmtest: stb_truetype InitFont + render on each font\n");
	for (i = 0; i < (int)(sizeof(fonts) / sizeof(fonts[0])); i++)
	{
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
		if (f == NULL)
		{
			printf("  %-10s fopen FAILED\n", fonts[i]);
			continue;
		}
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fseek(f, 0, SEEK_SET);
		buf = malloc((size_t)len);
		if (fread(buf, 1, (size_t)len, f) != (size_t)len)
		{
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

	/* Phase: msync / writeback (VMM 2.3).  Create a file with known content,
	 * mmap it MAP_SHARED read-write, verify the demand-read returns the file
	 * bytes, modify two separate pages, msync, munmap, then re-read from disk to
	 * confirm the writes persisted (and an untouched byte did not change). */
	printf("\nvmtest: msync writeback test\n");
	{
		const char *path = "/tmp/msync.dat";
		const int sz = 8192; /* two pages */
		char init[8192];
		char chk[8192];
		int fd;
		char *m;

		/* POSIX mmap-edit pattern: O_RDWR|O_CREAT|O_TRUNC to create+truncate,
		 * write known content, mmap that O_RDWR fd, then reopen O_RDWR to verify.
		 * Opening an existing file O_RDWR must NOT truncate it (honours O_TRUNC). */
		memset(init, 'A', sizeof(init));
		fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
		if (fd < 0)
		{
			printf("  open(create %s) FAILED\n", path);
		}
		else if (write(fd, init, sz) != sz)
		{
			printf("  write(init) FAILED\n");
			close(fd);
		}
		else
		{
			m = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (m == NULL || m == (char *)-1)
			{
				printf("  msync test: FAIL (mmap returned %p)\n", (void *)m);
				close(fd);
			}
			else
			{
				int dr_ok, wb_ok, n;
				char dr0, dr1;

				/* (1) demand-read must return the file's bytes (capture before
				 * unmap — m is invalid afterwards). */
				dr0 = m[0];
				dr1 = m[4096];
				dr_ok = (dr0 == 'A' && dr1 == 'A');

				/* (2) modify two pages, flush, unmap. */
				m[0] = 'Z';    /* page 0 */
				m[4096] = 'Y'; /* page 1 */
				msync(m, sz, MS_SYNC);
				munmap(m, sz);
				close(fd);

				/* (3) re-read from disk: writes persisted, neighbour untouched. */
				fd = open(path, O_RDWR, 0);
				n = (fd >= 0) ? (int)read(fd, chk, sz) : -1;
				if (fd >= 0)
					close(fd);
				wb_ok = (n == sz && chk[0] == 'Z' && chk[4096] == 'Y' && chk[1] == 'A');

				if (dr_ok && wb_ok)
				{
					printf("  msync test: PASS (demand-read + writeback verified)\n");
				}
				else
				{
					printf("  msync test: FAIL —%s%s\n",
					       dr_ok ? "" : " demand-read returned wrong bytes;",
					       wb_ok ? "" : " writeback did not persist to disk;");
					printf("    detail: demand [0]=%c [4096]=%c  reread n=%d [0]=%c [4096]=%c "
					       "[1]=%c\n",
					       dr0,
					       dr1,
					       n,
					       chk[0],
					       chk[4096],
					       chk[1]);
				}
			}
		}
	}

	printf("vmtest: done\n");
	return 0;
}
