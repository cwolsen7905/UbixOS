/*
 * compiler_rt.c — minimal GCC runtime helpers for UbixOS i386 musl builds.
 *
 * GCC emits calls to these when compiling 64-bit integer operations on a
 * 32-bit target (-m32).  They would normally come from libgcc, but the
 * Homebrew x86_64-elf-gcc toolchain ships only a 64-bit libgcc.  We supply
 * just the functions musl actually references so the link is self-contained.
 *
 * Algorithm: Knuth Vol.2 §4.3.1 Algorithm D (long division), adapted for
 * unsigned 64-bit operands on a 32-bit host.
 */

typedef unsigned long long uint64;
typedef unsigned int       uint32;

/*
 * __udivmoddi4 — unsigned 64-bit divide-with-remainder.
 *
 * Computes quotient = a / b, optionally stores remainder in *rem_p.
 * Division by zero returns 0 (undefined behaviour per C standard; musl
 * never calls it with b == 0 during normal printf operation).
 */
uint64 __udivmoddi4(uint64 a, uint64 b, uint64 *rem_p)
{
	uint64 q = 0;
	uint64 r = 0;
	int    i;

	if (b == 0) {
		if (rem_p)
			*rem_p = a;
		return 0;
	}

	for (i = 63; i >= 0; i--) {
		r = (r << 1) | ((a >> i) & 1);
		if (r >= b) {
			r -= b;
			q |= (uint64)1 << i;
		}
	}

	if (rem_p)
		*rem_p = r;
	return q;
}
