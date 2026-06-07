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

/**
 * Fill @n bytes at @dst with byte @c.
 */
void *memset(void *dst, int c, size_t n)
{
	u_int8_t *p = (u_int8_t *)dst;
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
