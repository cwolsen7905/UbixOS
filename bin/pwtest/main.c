/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * libpw smoke test: PBKDF2-HMAC-SHA256 known-answer vectors (dkLen=32).
 * Proves libpw.so + its transitive libbearssl.so load and compute correctly
 * in-OS under the -mno-sse scalar code paths.
 */

#include <pw/pw.h>
#include <stdio.h>
#include <string.h>

struct vec
{
	const char *pw;
	const char *salt;
	unsigned int iter;
	const char *hex;
};

/* Published PBKDF2-HMAC-SHA256 test vectors. */
static const struct vec g_vecs[] = {
    {"password", "salt", 1, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"},
    {"password", "salt", 2, "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"},
    {"password", "salt", 4096, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"},
};

/**
 * Run each PBKDF2 vector and report PASS/FAIL.
 *
 * @return 0 if all vectors match, 1 otherwise.
 */
int main(void)
{
	unsigned int v;
	int fail = 0;

	for (v = 0; v < sizeof(g_vecs) / sizeof(g_vecs[0]); v++)
	{
		unsigned char out[32];
		char got[65];
		int i;

		pw_pbkdf2_sha256(g_vecs[v].pw,
		                 strlen(g_vecs[v].pw),
		                 g_vecs[v].salt,
		                 strlen(g_vecs[v].salt),
		                 g_vecs[v].iter,
		                 out,
		                 sizeof(out));
		for (i = 0; i < 32; i++)
			sprintf(got + i * 2, "%02x", out[i]);
		if (strcmp(got, g_vecs[v].hex) == 0)
		{
			printf("pbkdf2-sha256 c=%-5u PASS\n", g_vecs[v].iter);
		}
		else
		{
			printf("pbkdf2-sha256 c=%-5u FAIL got=%s\n", g_vecs[v].iter, got);
			fail = 1;
		}
	}

	/* hash/verify round-trip: salt comes from getentropy() -> kernel CSPRNG. */
	{
		char h[PW_HASH_STRLEN];

		if (pw_hash("hunter2", h, sizeof(h)) != 0)
		{
			printf("pw_hash: FAIL\n");
			fail = 1;
		}
		else if (pw_verify("hunter2", h) != 1)
		{
			printf("pw_verify correct: FAIL\n");
			fail = 1;
		}
		else if (pw_verify("Hunter2", h) != 0)
		{
			printf("pw_verify wrong: FAIL (accepted bad password)\n");
			fail = 1;
		}
		else
		{
			printf("pw_hash/verify round-trip PASS\n  %s\n", h);
		}
	}

	printf(fail ? "pwtest: FAIL\n" : "pwtest: PASS\n");
	return (fail);
}
