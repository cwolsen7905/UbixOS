/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 freestanding string/memory primitives.
 *
 * Portable C implementations of the handful of libc-style routines the kernel
 * core uses (the i386 port supplies these as hand-written assembly in
 * arch/i386).  Byte-at-a-time and simple — correctness over speed for bring-up;
 * an optimised SIMD-free version can replace them later.
 */

#include <sys/types.h>

/* Number-to-digit lookup table for the shared formatting engine (kvprintf in
 * sys/lib/kprintf.c references it via the hex2ascii() macro).  i386 supplies its
 * copy from sys/lib/string.c; aarch64 keeps its string primitives self-contained
 * in this arch file (whose basename would otherwise collide with sys/lib's). */
char const hex2ascii_data[] = "0123456789abcdefghijklmnopqrstuvwxyz";


/*
 * Word-at-a-time memset/memcpy.  These were byte loops, and the kernel clears
 * and copies a lot of memory (every new process page, every exec of a 100 MB
 * clang, every pipe buffer) — an 8x cut in loop iterations.  Plain C: the kernel
 * is built -ffreestanding -fno-builtin, so the compiler cannot turn these loops
 * back into calls to themselves.  Unaligned heads/tails go a byte at a time.
 */

/**
 * Fill @n bytes at @dst with byte @c.
 */
void *memset(void *dst, int c, size_t n)
{
	u_int8_t *p = (u_int8_t *)dst;
	u_int64_t w = (u_int8_t)c;

	while (n > 0 && ((uintptr_t)p & 7) != 0)
	{
		*p++ = (u_int8_t)c;
		n--;
	}
	w |= w << 8;
	w |= w << 16;
	w |= w << 32;
	while (n >= 8)
	{
		*(u_int64_t *)p = w;
		p += 8;
		n -= 8;
	}
	while (n-- > 0)
		*p++ = (u_int8_t)c;
	return dst;
}

/**
 * Copy @n bytes from @src to @dst (non-overlapping).
 */
void *memcpy(void *dst, const void *src, size_t n)
{
	u_int8_t *d = (u_int8_t *)dst;
	const u_int8_t *s = (const u_int8_t *)src;

	/* Word copies only when both pointers can be 8-aligned together. */
	if ((((uintptr_t)d ^ (uintptr_t)s) & 7) == 0)
	{
		while (n > 0 && ((uintptr_t)d & 7) != 0)
		{
			*d++ = *s++;
			n--;
		}
		while (n >= 8)
		{
			*(u_int64_t *)d = *(const u_int64_t *)s;
			d += 8;
			s += 8;
			n -= 8;
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
