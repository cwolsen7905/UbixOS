/*
 * 64-bit arithmetic helpers for i386 — compiler support functions.
 * Needed by TCC itself (gen_opic constant folding) and by musl's time code.
 */
typedef long long           DWtype;
typedef unsigned long long  UDWtype;
typedef unsigned int        UWtype;

/* unsigned 64-bit divide: a / b */
UDWtype __udivdi3(UDWtype a, UDWtype b)
{
	UDWtype q = 0, r = 0;
	int i;
	if (b == 0)
		return 0;
	for (i = 63; i >= 0; i--) {
		r = (r << 1) | ((a >> i) & 1);
		if (r >= b) {
			r -= b;
			q |= (UDWtype)1 << i;
		}
	}
	return q;
}

/* unsigned 64-bit modulo: a % b */
UDWtype __umoddi3(UDWtype a, UDWtype b)
{
	UDWtype r = 0;
	int i;
	if (b == 0)
		return 0;
	for (i = 63; i >= 0; i--) {
		r = (r << 1) | ((a >> i) & 1);
		if (r >= b)
			r -= b;
	}
	return r;
}

/* signed 64-bit divide: a / b */
DWtype __divdi3(DWtype a, DWtype b)
{
	int neg = 0;
	UDWtype ua, ub, q;
	if (a < 0) { ua = (UDWtype)-a; neg ^= 1; } else { ua = (UDWtype)a; }
	if (b < 0) { ub = (UDWtype)-b; neg ^= 1; } else { ub = (UDWtype)b; }
	q = __udivdi3(ua, ub);
	return neg ? -(DWtype)q : (DWtype)q;
}

/* signed 64-bit modulo: a % b */
DWtype __moddi3(DWtype a, DWtype b)
{
	DWtype q = __divdi3(a, b);
	return a - q * b;
}

/* combined signed divide+modulo — used by musl's __secs_to_tm */
DWtype __divmoddi4(DWtype a, DWtype b, DWtype *rem)
{
	DWtype q = __divdi3(a, b);
	if (rem)
		*rem = a - q * b;
	return q;
}

/* combined unsigned divide+modulo — used by musl's vfprintf/floatscan */
UDWtype __udivmoddi4(UDWtype a, UDWtype b, UDWtype *rem)
{
	UDWtype q = __udivdi3(a, b);
	if (rem)
		*rem = a - q * b;
	return q;
}
