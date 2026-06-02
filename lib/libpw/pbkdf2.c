/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * PBKDF2-HMAC-SHA256 (RFC 8018) implemented over BearSSL's HMAC.
 */

#include <bearssl.h>
#include <pw/pw.h>
#include <string.h>

#define PW_SHA256_LEN 32

/* see pw/pw.h */
void
pw_pbkdf2_sha256(const void *pw, size_t pwlen, const void *salt, size_t saltlen,
    uint32_t iterations, unsigned char *out, size_t outlen)
{
	br_hmac_key_context kc;
	size_t copied;
	uint32_t i, blocks;

	if (iterations == 0)
		iterations = 1;

	/* Key the HMAC once with the password; reused for every block/iteration. */
	br_hmac_key_init(&kc, &br_sha256_vtable, pw, pwlen);

	blocks = (uint32_t)((outlen + PW_SHA256_LEN - 1) / PW_SHA256_LEN);
	copied = 0;
	for (i = 1; i <= blocks; i++) {
		unsigned char u[PW_SHA256_LEN], t[PW_SHA256_LEN], idx[4];
		br_hmac_context hc;
		uint32_t j;
		size_t k, n;

		/* INT_32_BE(i) appended to the salt for U_1. */
		idx[0] = (unsigned char)(i >> 24);
		idx[1] = (unsigned char)(i >> 16);
		idx[2] = (unsigned char)(i >> 8);
		idx[3] = (unsigned char)(i);

		br_hmac_init(&hc, &kc, 0);
		br_hmac_update(&hc, salt, saltlen);
		br_hmac_update(&hc, idx, sizeof(idx));
		br_hmac_out(&hc, u);
		memcpy(t, u, PW_SHA256_LEN);

		/* U_j = HMAC(P, U_{j-1}); T = U_1 ^ U_2 ^ ... ^ U_c. */
		for (j = 1; j < iterations; j++) {
			br_hmac_init(&hc, &kc, 0);
			br_hmac_update(&hc, u, PW_SHA256_LEN);
			br_hmac_out(&hc, u);
			for (k = 0; k < PW_SHA256_LEN; k++)
				t[k] ^= u[k];
		}

		n = outlen - copied;
		if (n > PW_SHA256_LEN)
			n = PW_SHA256_LEN;
		memcpy(out + copied, t, n);
		copied += n;
	}
}
