/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Freestanding memcpy/memset the compiler may emit calls to.
 */

#include "stand.h"

/** Copy @n bytes. */
void *memcpy(void *d, const void *s, unsigned long long n)
{
	uint8_t *dp = d;
	const uint8_t *sp = s;

	while (n--)
		*dp++ = *sp++;
	return d;
}

/** Fill @n bytes with @c. */
void *memset(void *d, int c, unsigned long long n)
{
	uint8_t *dp = d;

	while (n--)
		*dp++ = (uint8_t)c;
	return d;
}
