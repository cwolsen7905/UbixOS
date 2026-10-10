/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 freestanding string/memory primitives.
 *
 * Portable C implementations of the handful of libc-style routines the kernel
 * core uses (the i386 port supplies these as hand-written assembly in
 * arch/i386).  memset/memcpy work a word at a time (they are on the page
 * zero/copy hot path); the rest are simple byte loops.
 */

#include <sys/types.h>

/* Number-to-digit lookup table for the shared formatting engine (kvprintf in
 * sys/lib/kprintf.c references it via the hex2ascii() macro).  i386 supplies its
 * copy from sys/lib/string.c; aarch64 keeps its string primitives self-contained
 * in this arch file (whose basename would otherwise collide with sys/lib's). */
char const hex2ascii_data[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/* The bodies below are plain loops; stop GCC from "recognising" them as
 * memset/memcpy and compiling them into calls to themselves. */
#define NO_LOOP_TO_CALL __attribute__((optimize("no-tree-loop-distribute-patterns")))

/**
 * Fill @n bytes at @dst with byte @c.
 *
 * Word-at-a-time (64 bytes per iteration once aligned): the kernel zeroes
 * every page it hands out, so this is on the hot path of exec, fork, mmap
 * and the page-fault handler.  The kernel is -mgeneral-regs-only, so no SIMD.
 */
NO_LOOP_TO_CALL void *memset(void *dst, int c, size_t n)
{
	u_int8_t *p = (u_int8_t *)dst;
	u_int64_t v = (u_int8_t)c;

	v |= v << 8;
	v |= v << 16;
	v |= v << 32;
	while (n > 0 && ((uintptr_t)p & 7) != 0)
	{
		*p++ = (u_int8_t)c;
		n--;
	}
	{
		u_int64_t *w = (u_int64_t *)p;
		while (n >= 64)
		{
			w[0] = v;
			w[1] = v;
			w[2] = v;
			w[3] = v;
			w[4] = v;
			w[5] = v;
			w[6] = v;
			w[7] = v;
			w += 8;
			n -= 64;
		}
		while (n >= 8)
		{
			*w++ = v;
			n -= 8;
		}
		p = (u_int8_t *)w;
	}
	while (n-- > 0)
		*p++ = (u_int8_t)c;
	return dst;
}

/**
 * Copy @n bytes from @src to @dst (non-overlapping).
 *
 * Word-at-a-time when both pointers share 8-byte alignment (the common case:
 * page frames, file blocks, kmalloc buffers); byte-at-a-time otherwise.
 */
NO_LOOP_TO_CALL void *memcpy(void *dst, const void *src, size_t n)
{
	u_int8_t *d = (u_int8_t *)dst;
	const u_int8_t *s = (const u_int8_t *)src;

	if ((((uintptr_t)d ^ (uintptr_t)s) & 7) == 0)
	{
		while (n > 0 && ((uintptr_t)d & 7) != 0)
		{
			*d++ = *s++;
			n--;
		}
		{
			u_int64_t *wd = (u_int64_t *)d;
			const u_int64_t *ws = (const u_int64_t *)s;
			while (n >= 64)
			{
				u_int64_t a = ws[0], b = ws[1], c2 = ws[2], e = ws[3];
				u_int64_t f = ws[4], g = ws[5], h = ws[6], k = ws[7];
				wd[0] = a;
				wd[1] = b;
				wd[2] = c2;
				wd[3] = e;
				wd[4] = f;
				wd[5] = g;
				wd[6] = h;
				wd[7] = k;
				wd += 8;
				ws += 8;
				n -= 64;
			}
			while (n >= 8)
			{
				*wd++ = *ws++;
				n -= 8;
			}
			d = (u_int8_t *)wd;
			s = (const u_int8_t *)ws;
		}
	}
	while (n-- > 0)
		*d++ = *s++;
	return dst;
}

/**
 * Copy at most @n bytes of the C string @src to @dst, NUL-padding the remainder.
 */
char *strncpy(char *dst, const char *src, size_t n)
{
	size_t i = 0;
	for (; i < n && src[i] != '\0'; i++)
		dst[i] = src[i];
	for (; i < n; i++)
		dst[i] = '\0';
	return dst;
}

/**
 * Copy the NUL-terminated C string @src (including its terminator) to @dst.
 *
 * @dst must be large enough to hold the string and its terminator.
 */
char *strcpy(char *dst, const char *src)
{
	size_t i = 0;
	while ((dst[i] = src[i]) != '\0')
		i++;
	return dst;
}

/**
 * Lexicographic compare of NUL-terminated strings @a and @b.
 *
 * @return <0, 0, or >0 as @a is less than, equal to, or greater than @b.
 */
int strcmp(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b)
	{
		a++;
		b++;
	}
	return (int)(u_int8_t)*a - (int)(u_int8_t)*b;
}

/**
 * Like strcmp but comparing at most @n bytes.
 */
int strncmp(const char *a, const char *b, size_t n)
{
	while (n > 0 && *a != '\0' && *a == *b)
	{
		a++;
		b++;
		n--;
	}
	if (n == 0)
		return 0;
	return (int)(u_int8_t)*a - (int)(u_int8_t)*b;
}

/**
 * Compare @n bytes of @a and @b.
 *
 * @return <0, 0, or >0 on the first differing byte (unsigned), or 0 if equal.
 */
int memcmp(const void *a, const void *b, size_t n)
{
	const u_int8_t *pa = (const u_int8_t *)a;
	const u_int8_t *pb = (const u_int8_t *)b;
	while (n-- > 0)
	{
		if (*pa != *pb)
			return (int)*pa - (int)*pb;
		pa++;
		pb++;
	}
	return 0;
}
