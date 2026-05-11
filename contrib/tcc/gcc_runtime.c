/* 64-bit helpers for i386 — compiled with x86_64-elf-gcc -m32 */
typedef long long          DWtype;
typedef unsigned long long UDWtype;
typedef unsigned int       UWtype;
typedef unsigned int       USItype;

static UDWtype udivmod(UDWtype n, UDWtype d, int rem)
{
    if (d == 0) return 0;
    if (d > n) return rem ? n : 0;

    UDWtype q = 0;
    /* Shift d left until it's >= n/2 */
    int shift = 0;
    UDWtype dd = d;
    while ((dd << 1) > dd && (dd << 1) <= n) { dd <<= 1; shift++; }
    while (1) {
        if (n >= dd) { n -= dd; q += (UDWtype)1 << shift; }
        if (shift == 0) break;
        dd >>= 1; shift--;
    }
    return rem ? n : q;
}

unsigned long long __umoddi3(UDWtype a, UDWtype b) { return udivmod(a, b, 1); }
long long __moddi3(DWtype a, DWtype b) {
    int neg = 0;
    UDWtype ua = (UDWtype)a, ub = (UDWtype)b;
    if (a < 0) { ua = (UDWtype)-a; neg = 1; }
    if (b < 0) { ub = (UDWtype)-b; }
    UDWtype r = udivmod(ua, ub, 1);
    return neg ? -(DWtype)r : (DWtype)r;
}

unsigned long long __udivdi3(UDWtype a, UDWtype b) { return udivmod(a, b, 0); }
long long __divdi3(DWtype a, DWtype b) {
    int neg = (a < 0) != (b < 0);
    UDWtype ua = a < 0 ? (UDWtype)-a : (UDWtype)a;
    UDWtype ub = b < 0 ? (UDWtype)-b : (UDWtype)b;
    UDWtype q = udivmod(ua, ub, 0);
    return neg ? -(DWtype)q : (DWtype)q;
}
