/*
 * Misc missing functions for TCC on UbixOS
 * Copyright (c) 2002-2026 The UbixOS Project.
 */
#include <stdlib.h>
#include <string.h>

/* strtoul — unsigned version of strtol */
unsigned long strtoul(const char *nptr, char **endptr, int base)
{
	unsigned long val = 0;
	const char *p = nptr;
	while (*p == ' ' || *p == '\t') p++;
	if (base == 0) {
		if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
		else if (*p == '0') { base = 8; p++; }
		else base = 10;
	} else if (base == 16 && *p == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
	for (;;) {
		int d;
		if (*p >= '0' && *p <= '9') d = *p - '0';
		else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
		else break;
		if (d >= base) break;
		val = val * (unsigned)base + (unsigned)d;
		p++;
	}
	if (endptr) *endptr = (char *)p;
	return val;
}

/* gettimeofday stub */
#include <sys/time.h>
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	if (tv) { tv->tv_sec = 0; tv->tv_usec = 0; }
	(void)tz;
	return 0;
}

/* fflush stub — our stdio has no buffering */
int fflush(void *fp) { (void)fp; return 0; }
