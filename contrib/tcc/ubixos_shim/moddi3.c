/*
 * 64-bit modulo helpers for i386 — uses __divdi3/__udivdi3 from quad/
 */
typedef long long          DWtype;
typedef unsigned long long UDWtype;

extern DWtype  __divdi3(DWtype a, DWtype b);
extern UDWtype __udivdi3(UDWtype a, UDWtype b);

DWtype __moddi3(DWtype a, DWtype b)
{
	return a - __divdi3(a, b) * b;
}

UDWtype __umoddi3(UDWtype a, UDWtype b)
{
	return a - __udivdi3(a, b) * b;
}
