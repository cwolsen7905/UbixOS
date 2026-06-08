/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * Fletcher-4 checksum implementation.  See fletcher.h.
 */
#include "fletcher.h"

void ubfs_fletcher4(const void *buf, size_t len, uint64_t out[4])
{
	const uint32_t *p = (const uint32_t *)buf;
	size_t          n = len / 4;
	uint64_t        a = 0, b = 0, c = 0, d = 0;
	size_t          i;

	for (i = 0; i < n; i++)
	{
		a += p[i];
		b += a;
		c += b;
		d += c;
	}
	out[0] = a;
	out[1] = b;
	out[2] = c;
	out[3] = d;
}
