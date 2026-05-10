/*
 * Minimal time stubs for TCC on UbixOS
 * Copyright (c) 2002-2026 The UbixOS Project.
 */
#include <time.h>

/* Stub: return 0 (epoch); TCC uses this for __DATE__ / __TIME__ macros */
time_t time(time_t *tloc)
{
	if (tloc)
		*tloc = 0;
	return 0;
}

static struct tm _tm;

struct tm *localtime(const time_t *tp)
{
	(void)tp;
	/* Return a zeroed struct — TCC formats it for __DATE__ / __TIME__ */
	return &_tm;
}
