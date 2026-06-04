/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * pwhash.c — password hash/verify in the modular-crypt format
 *   $pbkdf2-sha256$<iterations>$<base64 salt>$<base64 hash>
 * built on pw_pbkdf2_sha256().  Salts come from getentropy() (kernel CSPRNG);
 * verification is constant-time.  Passwords are HASHED, never encrypted.
 */

#include <pw/pw.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>     /* getentropy on musl (UbixOS target) */
#include <sys/random.h> /* getentropy on the build host (macOS/FreeBSD) */

#define PW_SALT_LEN 16
#define PW_HASH_LEN 32
#define PW_PREFIX "$pbkdf2-sha256$"

static const char g_b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Standard base64-encode @p inlen bytes into @p out (NUL-terminated).
 * @p out must hold at least 4*ceil(inlen/3)+1 bytes.
 */
static void b64_encode(const unsigned char *in, size_t inlen, char *out)
{
	size_t i = 0, o = 0;

	while (i + 3 <= inlen)
	{
		unsigned int v = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8) | in[i + 2];
		out[o++] = g_b64[(v >> 18) & 63];
		out[o++] = g_b64[(v >> 12) & 63];
		out[o++] = g_b64[(v >> 6) & 63];
		out[o++] = g_b64[v & 63];
		i += 3;
	}
	if (inlen - i == 1)
	{
		unsigned int v = (unsigned)in[i] << 16;
		out[o++] = g_b64[(v >> 18) & 63];
		out[o++] = g_b64[(v >> 12) & 63];
		out[o++] = '=';
		out[o++] = '=';
	}
	else if (inlen - i == 2)
	{
		unsigned int v = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8);
		out[o++] = g_b64[(v >> 18) & 63];
		out[o++] = g_b64[(v >> 12) & 63];
		out[o++] = g_b64[(v >> 6) & 63];
		out[o++] = '=';
	}
	out[o] = '\0';
}

/**
 * Decode one base64 character to its 6-bit value, or -1 if not in the alphabet.
 */
static int b64_val(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (c - 'A');
	if (c >= 'a' && c <= 'z')
		return (c - 'a' + 26);
	if (c >= '0' && c <= '9')
		return (c - '0' + 52);
	if (c == '+')
		return (62);
	if (c == '/')
		return (63);
	return (-1);
}

/**
 * Decode the base64 segment [in, in+inlen) into @p out (capacity @p outmax).
 * @return number of bytes decoded, or -1 on bad input / overflow.
 */
static int b64_decode(const char *in, size_t inlen, unsigned char *out, size_t outmax)
{
	size_t o = 0, i, qi = 0;
	int q[4];

	for (i = 0; i < inlen; i++)
	{
		int v;
		if (in[i] == '=')
			break;
		v = b64_val(in[i]);
		if (v < 0)
			return (-1);
		q[qi++] = v;
		if (qi == 4)
		{
			if (o + 3 > outmax)
				return (-1);
			out[o++] = (unsigned char)((q[0] << 2) | (q[1] >> 4));
			out[o++] = (unsigned char)(((q[1] & 15) << 4) | (q[2] >> 2));
			out[o++] = (unsigned char)(((q[2] & 3) << 6) | q[3]);
			qi = 0;
		}
	}
	if (qi == 2)
	{
		if (o + 1 > outmax)
			return (-1);
		out[o++] = (unsigned char)((q[0] << 2) | (q[1] >> 4));
	}
	else if (qi == 3)
	{
		if (o + 2 > outmax)
			return (-1);
		out[o++] = (unsigned char)((q[0] << 2) | (q[1] >> 4));
		out[o++] = (unsigned char)(((q[1] & 15) << 4) | (q[2] >> 2));
	}
	return ((int)o);
}

/* see pw/pw.h */
int pw_hash(const char *password, char *out, size_t outlen)
{
	unsigned char salt[PW_SALT_LEN], dk[PW_HASH_LEN];
	char b64salt[25], b64hash[45];
	int n;

	if (getentropy(salt, sizeof(salt)) != 0)
		return (-1);

	pw_pbkdf2_sha256(password, strlen(password), salt, sizeof(salt), PW_DEFAULT_ITERATIONS, dk, sizeof(dk));
	b64_encode(salt, sizeof(salt), b64salt);
	b64_encode(dk, sizeof(dk), b64hash);

	n = snprintf(out, outlen, "%s%u$%s$%s", PW_PREFIX, (unsigned)PW_DEFAULT_ITERATIONS, b64salt, b64hash);
	if (n < 0 || (size_t)n >= outlen)
		return (-1);
	return (0);
}

/* see pw/pw.h */
int pw_verify(const char *password, const char *stored)
{
	const char *p, *salt_b64, *hash_b64, *dollar;
	unsigned char salt[64], want[64], calc[64];
	unsigned char diff;
	unsigned long iter;
	size_t salt_b64_len;
	char *end;
	int sl, hl, i;

	if (strncmp(stored, PW_PREFIX, strlen(PW_PREFIX)) != 0)
		return (-1);
	p = stored + strlen(PW_PREFIX);

	iter = strtoul(p, &end, 10);
	if (end == p || *end != '$' || iter == 0)
		return (-1);

	salt_b64 = end + 1;
	dollar = strchr(salt_b64, '$');
	if (dollar == NULL)
		return (-1);
	salt_b64_len = (size_t)(dollar - salt_b64);
	hash_b64 = dollar + 1;

	sl = b64_decode(salt_b64, salt_b64_len, salt, sizeof(salt));
	hl = b64_decode(hash_b64, strlen(hash_b64), want, sizeof(want));
	if (sl <= 0 || hl <= 0 || (size_t)hl > sizeof(calc))
		return (-1);

	pw_pbkdf2_sha256(password, strlen(password), salt, (size_t)sl, (uint32_t)iter, calc, (size_t)hl);

	/* Constant-time compare. */
	diff = 0;
	for (i = 0; i < hl; i++)
		diff |= (unsigned char)(calc[i] ^ want[i]);
	return (diff == 0 ? 1 : 0);
}
