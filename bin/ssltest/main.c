/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * BearSSL smoke test: compute SHA-256("abc") and compare against the known
 * FIPS-180 test vector.  Proves libbearssl is callable in-OS and that the
 * -mno-sse scalar code paths produce correct results.
 */

#include <bearssl.h>
#include <unistd.h>

static const unsigned char g_expected[32] = {
    0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
    0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};

/**
 * Entry point: hash "abc" with BearSSL SHA-256 and report PASS/FAIL.
 *
 * @return 0 if the digest matches the known vector, 1 otherwise.
 */
int
main(void)
{
	br_sha256_context ctx;
	unsigned char out[32];
	int i;

	br_sha256_init(&ctx);
	br_sha256_update(&ctx, "abc", 3);
	br_sha256_out(&ctx, out);

	for (i = 0; i < 32; i++) {
		if (out[i] != g_expected[i]) {
			write(1, "bearssl sha256: FAIL\n", 21);
			return (1);
		}
	}
	write(1, "bearssl sha256: PASS\n", 21);
	return (0);
}
