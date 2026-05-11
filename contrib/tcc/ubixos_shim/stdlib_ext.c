/*
 * Missing stdlib functions for TCC on UbixOS
 * Copyright (c) 2002-2026 The UbixOS Project.
 */
#include <stdlib.h>
#include <string.h>

void *realloc(void *ptr, size_t size)
{
	void *np;
	if (!ptr)
		return malloc(size);
	if (size == 0) {
		free(ptr);
		return NULL;
	}
	np = malloc(size);
	if (!np)
		return NULL;
	memcpy(np, ptr, size); /* may copy too much — acceptable for now */
	free(ptr);
	return np;
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	void *p = malloc(total);
	if (p)
		memset(p, 0, total);
	return p;
}

int putenv(char *str)
{
	(void)str;
	return 0;
}

/* Simple strtod — handles decimal integers and basic floats */
double strtod(const char *nptr, char **endptr)
{
	double d = 0.0, frac = 0.0, scale = 1.0;
	int neg = 0, have_frac = 0;
	const char *p = nptr;

	while (*p == ' ' || *p == '\t') p++;
	if (*p == '-') { neg = 1; p++; }
	else if (*p == '+') { p++; }

	while (*p >= '0' && *p <= '9')
		d = d * 10.0 + (*p++ - '0');

	if (*p == '.') {
		p++;
		have_frac = 1;
		while (*p >= '0' && *p <= '9') {
			frac = frac * 10.0 + (*p++ - '0');
			scale *= 10.0;
		}
	}
	(void)have_frac;
	d += frac / scale;
	if (neg) d = -d;

	/* skip exponent */
	if (*p == 'e' || *p == 'E') {
		int eneg = 0; double ep = 1.0; p++;
		if (*p == '-') { eneg = 1; p++; } else if (*p == '+') p++;
		int ev = 0;
		while (*p >= '0' && *p <= '9') ev = ev * 10 + (*p++ - '0');
		while (ev--) ep *= 10.0;
		d = eneg ? d / ep : d * ep;
	}

	if (endptr) *endptr = (char *)p;
	return d;
}

float strtof(const char *nptr, char **endptr)
{
	return (float)strtod(nptr, endptr);
}

long double strtold(const char *nptr, char **endptr)
{
	return (long double)strtod(nptr, endptr);
}

unsigned long long strtoull(const char *nptr, char **endptr, int base)
{
	unsigned long long val = 0;
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

long long strtoll(const char *nptr, char **endptr, int base)
{
	int neg = 0;
	const char *p = nptr;
	while (*p == ' ' || *p == '\t') p++;
	if (*p == '-') { neg = 1; p++; } else if (*p == '+') p++;
	unsigned long long u = strtoull(p, endptr, base);
	return neg ? -(long long)u : (long long)u;
}

/* Simple insertion-sort qsort — adequate for small arrays TCC uses */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
	char *b = base;
	char *tmp = malloc(size);
	if (!tmp) return;
	for (size_t i = 1; i < nmemb; i++) {
		memcpy(tmp, b + i * size, size);
		size_t j = i;
		while (j > 0 && compar(b + (j-1) * size, tmp) > 0) {
			memcpy(b + j * size, b + (j-1) * size, size);
			j--;
		}
		memcpy(b + j * size, tmp, size);
	}
	free(tmp);
}
