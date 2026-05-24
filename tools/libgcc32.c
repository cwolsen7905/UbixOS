/*
 * Minimal 64-bit integer helpers for i386 — stand-in for libgcc.
 * These are needed when building libc.so with x86_64-elf-gcc -m32
 * which does not ship a 32-bit libgcc.
 *
 * Only the functions that musl's .lo objects actually reference are here.
 */

typedef unsigned long long u64;
typedef long long           s64;
typedef unsigned int        u32;

/* Unsigned 64-bit divide with remainder. */
u64
__udivmoddi4(u64 n, u64 d, u64 *rem)
{
	if (d == 0) {
		/* undefined behaviour — match what GCC libgcc does: return ~0 */
		if (rem)
			*rem = n;
		return ~(u64)0;
	}

	if (d > n) {
		if (rem)
			*rem = n;
		return 0;
	}

	/* Use 32-bit fast path when both halves fit in 32 bits. */
	if (!(d >> 32) && !(n >> 32)) {
		u32 lo = (u32)n / (u32)d;
		if (rem)
			*rem = (u32)n % (u32)d;
		return lo;
	}

	/* Shift-and-subtract long division. */
	u64 q = 0, r = 0;
	for (int i = 63; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) {
			r -= d;
			q |= (u64)1 << i;
		}
	}
	if (rem)
		*rem = r;
	return q;
}

u64
__udivdi3(u64 n, u64 d)
{
	return __udivmoddi4(n, d, (u64 *)0);
}

u64
__umoddi3(u64 n, u64 d)
{
	u64 r;
	__udivmoddi4(n, d, &r);
	return r;
}

s64
__divmoddi4(s64 n, s64 d, s64 *rem)
{
	int neg_q = 0, neg_r = 0;
	u64 un = (u64)n, ud = (u64)d, ur;

	if (n < 0) { un = (u64)(-n); neg_q = 1; neg_r = 1; }
	if (d < 0) { ud = (u64)(-d); neg_q ^= 1; }

	u64 q = __udivmoddi4(un, ud, &ur);

	if (rem)
		*rem = neg_r ? -(s64)ur : (s64)ur;
	return neg_q ? -(s64)q : (s64)q;
}

s64
__divdi3(s64 n, s64 d)
{
	s64 r;
	return __divmoddi4(n, d, &r);
}

s64
__moddi3(s64 n, s64 d)
{
	s64 r;
	__divmoddi4(n, d, &r);
	return r;
}

/* Complex long double multiply — needed by cpow* but not the rtld path. */
_Complex long double
__mulxc3(long double ar, long double ai, long double br, long double bi)
{
	long double re = ar * br - ai * bi;
	long double im = ar * bi + ai * br;
	return __builtin_complex(re, im);
}
