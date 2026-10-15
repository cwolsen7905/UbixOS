/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libpkg — SHA-256, hex, and ECDSA P-256 signature verification (BearSSL).
 */

#include <errno.h>
#include <string.h>
#include <bearssl.h>
#include "pkg_internal.h"

_Static_assert(sizeof(br_sha256_context) <= sizeof(struct pkg_sha256), "pkg_sha256 too small");

#define SIG_LINE_KEY "ecdsa-p256-sha256"
#define KEY_LINE_KEY "ecdsa-p256"
#define SIG_DER_MAX 80 /* P-256 DER signature is at most 72 bytes */

/** Initialise a streaming SHA-256. */
void pkg_sha256_init(struct pkg_sha256 *s)
{
	br_sha256_init((br_sha256_context *)(void *)s);
}

/** Feed bytes into a streaming SHA-256. */
void pkg_sha256_update(struct pkg_sha256 *s, const void *data, size_t len)
{
	br_sha256_update((br_sha256_context *)(void *)s, data, len);
}

/** Finish a streaming SHA-256 and write the 32-byte digest. */
void pkg_sha256_final(struct pkg_sha256 *s, uint8_t out[PKG_SHA256_LEN])
{
	br_sha256_out((br_sha256_context *)(void *)s, out);
}

/** One-shot SHA-256 of a buffer. */
void pkg_sha256(const void *data, size_t len, uint8_t out[PKG_SHA256_LEN])
{
	struct pkg_sha256 s;

	pkg_sha256_init(&s);
	pkg_sha256_update(&s, data, len);
	pkg_sha256_final(&s, out);
}

/** Encode bytes as lowercase hex. */
void pkg_hex_encode(const uint8_t *in, size_t len, char *out)
{
	static const char digits[] = "0123456789abcdef";
	size_t i;

	for (i = 0; i < len; i++)
	{
		out[2 * i] = digits[in[i] >> 4];
		out[2 * i + 1] = digits[in[i] & 15];
	}
	out[2 * len] = '\0';
}

/** @return the value of one hex digit, or -1. */
static int hex_val(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/** Decode exactly 2*len hex digits. */
int pkg_hex_decode(const char *hex, size_t hexlen, uint8_t *out, size_t len)
{
	size_t i;

	if (len > SIZE_MAX / 2 || hexlen != 2 * len)
		return -EINVAL;
	for (i = 0; i < len; i++)
	{
		int hi = hex_val(hex[2 * i]);
		int lo = hex_val(hex[2 * i + 1]);

		if (hi < 0 || lo < 0)
			return -EINVAL;
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	return 0;
}

/** Parse a `ecdsa-p256: <hex>` key file. */
int pkg_key_parse(const char *text, size_t len, struct pkg_key *key)
{
	struct pkg_kv it;
	int rc;

	pkg_kv_init(&it, text, len);
	while ((rc = pkg_kv_next(&it)) > PKG_KV_EOF)
	{
		if (rc == PKG_KV_PAIR && strcmp(it.key, KEY_LINE_KEY) == 0)
		{
			if (pkg_hex_decode(it.val, it.vlen, key->point, PKG_ECDSA_PUB_LEN) != 0 ||
			    key->point[0] != 0x04)
				return -EINVAL;
			return 0;
		}
	}
	return rc < 0 ? rc : -EINVAL;
}

/** Verify one ECDSA P-256/SHA-256 signature. */
int pkg_ecdsa_verify(const struct pkg_key *key, const void *data, size_t len, const uint8_t *sig, size_t siglen)
{
	uint8_t digest[PKG_SHA256_LEN];
	br_ec_public_key pk;

	if (key == NULL || key->point[0] != 0x04)
		return -EINVAL;
	pkg_sha256(data, len, digest);
	pk.curve = BR_EC_secp256r1;
	pk.q = (unsigned char *)(void *)(uintptr_t)key->point;
	pk.qlen = PKG_ECDSA_PUB_LEN;
	if (br_ecdsa_i31_vrfy_asn1(&br_ec_prime_i31, digest, sizeof digest, &pk, sig, siglen) != 1)
		return -EBADMSG;
	return 0;
}

/** Verify a signature file against a trust list. */
int pkg_sigfile_verify(
    const char *sigtext, size_t siglen, const void *data, size_t len, const struct pkg_key *keys, size_t nkeys)
{
	struct pkg_kv it;
	uint8_t der[SIG_DER_MAX];
	int rc;
	int seen = 0;
	size_t k;

	if (nkeys == 0 || keys == NULL)
		return -EINVAL;
	pkg_kv_init(&it, sigtext, siglen);
	while ((rc = pkg_kv_next(&it)) > PKG_KV_EOF)
	{
		size_t n;

		if (rc != PKG_KV_PAIR || strcmp(it.key, SIG_LINE_KEY) != 0)
			continue;
		n = it.vlen / 2;
		if (n == 0 || n > sizeof der || pkg_hex_decode(it.val, it.vlen, der, n) != 0)
			return -EINVAL;
		seen = 1;
		for (k = 0; k < nkeys; k++)
		{
			if (pkg_ecdsa_verify(&keys[k], data, len, der, n) == 0)
				return 0;
		}
	}
	if (rc < 0)
		return rc;
	return seen ? -EBADMSG : -EINVAL;
}
