/*
 * Copyright (c) 2002-2026 The UbixOS Project.
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * 64-bit integer arithmetic helpers for i386 (GCC compiler-rt ABI).
 * The x86_64-elf cross-compiler has no 32-bit multilib, so these symbols
 * are absent from its libgcc.a.  musl references them for vfprintf et al.
 */

typedef unsigned long long u64;
typedef long long          s64;

/* Bit-by-bit long division — correct and free of undefined behaviour. */
static u64
uldivmod(u64 num, u64 den, u64 *rem_p)
{
	u64 quot = 0;
	u64 rem = 0;
	int i;

	for (i = 63; i >= 0; i--) {
		rem = (rem << 1) | ((num >> i) & 1);
		if (rem >= den) {
			rem -= den;
			quot |= (u64)1 << i;
		}
	}
	if (rem_p)
		*rem_p = rem;
	return quot;
}

u64 __udivmoddi4(u64 num, u64 den, u64 *rem_p)
{
	return uldivmod(num, den, rem_p);
}

u64 __udivdi3(u64 a, u64 b)
{
	return uldivmod(a, b, (u64 *)0);
}

u64 __umoddi3(u64 a, u64 b)
{
	u64 rem;
	uldivmod(a, b, &rem);
	return rem;
}

s64 __divmoddi4(s64 num, s64 den, s64 *rem_p)
{
	int neg = 0;
	u64 q, rem;

	if (num < 0) { num = -num; neg ^= 1; }
	if (den < 0) { den = -den; neg ^= 1; }

	q = uldivmod((u64)num, (u64)den, &rem);

	if (neg) { q = -(s64)q; rem = -(s64)rem; }
	if (rem_p)
		*rem_p = (s64)rem;
	return (s64)q;
}

s64 __divdi3(s64 a, s64 b)
{
	return __divmoddi4(a, b, (s64 *)0);
}

s64 __moddi3(s64 a, s64 b)
{
	s64 rem;
	__divmoddi4(a, b, &rem);
	return rem;
}
