/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libpkg — path policy, version comparison and ISO-8601 timestamps.
 */

#include <errno.h>
#include <string.h>
#include <ubix/pkg.h>

/** @return nonzero for an ASCII digit. */
static int is_digit(char c)
{
	return c >= '0' && c <= '9';
}

/** @return nonzero for a version separator. */
static int is_sep(char c)
{
	return c == '.' || c == '-' || c == '_' || c == '+';
}

/** Check a relative, traversal-free path. */
int pkg_path_is_safe(const char *path)
{
	const char *p = path;
	size_t len;

	if (path == NULL || path[0] == '\0' || path[0] == '/')
		return 0;
	len = strlen(path);
	if (len >= PKG_PATH_MAX)
		return 0;
	while (*p != '\0')
	{
		const char *slash = strchr(p, '/');
		size_t clen = slash != NULL ? (size_t)(slash - p) : strlen(p);
		size_t i;

		if (clen == 0 || (clen == 1 && p[0] == '.') || (clen == 2 && p[0] == '.' && p[1] == '.'))
			return 0;
		for (i = 0; i < clen; i++)
		{
			if ((unsigned char)p[i] < 0x20 || p[i] == 0x7f)
				return 0;
		}
		if (slash == NULL)
			break;
		p = slash + 1;
		if (*p == '\0') /* trailing slash */
			return 0;
	}
	return 1;
}

/** @return the length of the run of letters/other bytes at s. */
static size_t alpha_len(const char *s)
{
	size_t n = 0;

	while (s[n] != '\0' && !is_digit(s[n]) && !is_sep(s[n]))
		n++;
	return n;
}

/** Compare two version strings. */
int pkg_version_cmp(const char *a, const char *b)
{
	for (;;)
	{
		while (is_sep(*a))
			a++;
		while (is_sep(*b))
			b++;
		if (*a == '\0' || *b == '\0')
			return (*a != '\0') - (*b != '\0');
		if (is_digit(*a) && is_digit(*b))
		{
			size_t la;
			size_t lb;

			while (*a == '0' && is_digit(a[1]))
				a++;
			while (*b == '0' && is_digit(b[1]))
				b++;
			for (la = 0; is_digit(a[la]); la++)
				;
			for (lb = 0; is_digit(b[lb]); lb++)
				;
			if (la != lb)
				return la < lb ? -1 : 1;
			if (memcmp(a, b, la) != 0)
				return memcmp(a, b, la) < 0 ? -1 : 1;
			a += la;
			b += lb;
		}
		else if (is_digit(*a) || is_digit(*b))
		{
			return is_digit(*a) ? 1 : -1; /* a number outranks letters */
		}
		else
		{
			size_t la = alpha_len(a);
			size_t lb = alpha_len(b);
			size_t m = la < lb ? la : lb;
			int c = memcmp(a, b, m);

			if (c != 0)
				return c < 0 ? -1 : 1;
			if (la != lb)
				return la < lb ? -1 : 1;
			a += la;
			b += lb;
		}
	}
}

/** Test a version against a constraint. */
int pkg_dep_satisfied(const struct pkg_dep *dep, const char *have)
{
	int c;

	if (dep->op == PKG_OP_ANY)
		return 1;
	c = pkg_version_cmp(have, dep->version);
	switch (dep->op)
	{
		case PKG_OP_EQ:
			return c == 0;
		case PKG_OP_GE:
			return c >= 0;
		case PKG_OP_GT:
			return c > 0;
		case PKG_OP_LE:
			return c <= 0;
		case PKG_OP_LT:
			return c < 0;
		default:
			return 0;
	}
}

/** Read exactly n decimal digits.  @return the value or -1. */
static int digits(const char *s, int n)
{
	int v = 0;
	int i;

	for (i = 0; i < n; i++)
	{
		if (!is_digit(s[i]))
			return -1;
		v = v * 10 + (s[i] - '0');
	}
	return v;
}

/** Parse `YYYY-MM-DDTHH:MM:SSZ` (UTC). */
int pkg_time_parse(const char *s, time_t *out)
{
	int y;
	int mo;
	int d;
	int h;
	int mi;
	int se;
	int64_t days;
	int64_t yy;
	int64_t era;
	int64_t yoe;
	int64_t doy;
	int64_t doe;

	if (strlen(s) != 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':' ||
	    s[19] != 'Z')
		return -EINVAL;
	y = digits(s, 4);
	mo = digits(s + 5, 2);
	d = digits(s + 8, 2);
	h = digits(s + 11, 2);
	mi = digits(s + 14, 2);
	se = digits(s + 17, 2);
	if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 ||
	    se > 60)
		return -EINVAL;
	/* days from civil (H. Hinnant) */
	yy = y - (mo <= 2);
	era = yy / 400;
	yoe = yy - era * 400;
	doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	days = era * 146097 + doe - 719468;
	*out = (time_t)(days * 86400 + h * 3600 + mi * 60 + se);
	return 0;
}
