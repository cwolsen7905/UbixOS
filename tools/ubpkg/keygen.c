/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg keygen: generate an ECDSA P-256 signing key pair with BearSSL.
 *
 *   <base>.key   `ecdsa-p256-private: <64 hex>` (mode 0600)
 *   <base>.pub   `ecdsa-p256: <130 hex>`        (what /etc/pkg/keys holds)
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bearssl.h>
#include "ubpkg.h"

/** Fill buf from the OS entropy source.  @return 0 or -errno. */
static int os_random(void *buf, size_t len)
{
	int fd = open("/dev/urandom", O_RDONLY);
	uint8_t *p = buf;

	if (fd < 0)
		return -errno;
	while (len > 0)
	{
		ssize_t k = read(fd, p, len);

		if (k <= 0)
		{
			close(fd);
			return -EIO;
		}
		p += k;
		len -= (size_t)k;
	}
	close(fd);
	return 0;
}

/** `ubpkg keygen <basename>`.  @return exit status. */
int cmd_keygen(int argc, char **argv)
{
	br_hmac_drbg_context rng;
	br_ec_private_key sk;
	br_ec_public_key pk;
	uint8_t seed[48];
	uint8_t kbuf[BR_EC_KBUF_PRIV_MAX_SIZE];
	uint8_t pbuf[BR_EC_KBUF_PUB_MAX_SIZE];
	uint8_t fp[PKG_SHA256_LEN];
	char hexp[PKG_ECDSA_PUB_LEN * 2 + 1];
	char hexk[BR_EC_KBUF_PRIV_MAX_SIZE * 2 + 1];
	char hexfp[PKG_SHA256_HEX];
	char path[PKG_PATH_MAX];
	char text[512];
	int rc;

	if (argc != 2)
	{
		ub_err("usage: ubpkg keygen <basename>");
		return 2;
	}
	if ((rc = os_random(seed, sizeof seed)) != 0)
	{
		ub_err("no entropy: %s", strerror(-rc));
		return 1;
	}
	br_hmac_drbg_init(&rng, &br_sha256_vtable, seed, sizeof seed);
	if (br_ec_keygen(&rng.vtable, &br_ec_prime_i31, &sk, kbuf, BR_EC_secp256r1) == 0 ||
	    br_ec_compute_pub(&br_ec_prime_i31, &pk, pbuf, &sk) == 0 || pk.qlen != PKG_ECDSA_PUB_LEN || sk.xlen != 32)
	{
		ub_err("key generation failed");
		return 1;
	}
	pkg_hex_encode(pk.q, pk.qlen, hexp);
	pkg_hex_encode(sk.x, sk.xlen, hexk);
	pkg_sha256(pk.q, pk.qlen, fp);
	pkg_hex_encode(fp, sizeof fp, hexfp);

	snprintf(path, sizeof path, "%s.key", argv[1]);
	snprintf(text, sizeof text, "%s: %s\n", UBPKG_PRIV_KEY, hexk);
	if ((rc = ub_write_new(path, text, strlen(text), 0600)) != 0)
	{
		ub_err("%s: %s", path, strerror(-rc));
		return 1;
	}
	snprintf(path, sizeof path, "%s.pub", argv[1]);
	snprintf(text, sizeof text, "# uBixOS package signing key, fingerprint %s\necdsa-p256: %s\n", hexfp, hexp);
	if ((rc = ub_write_new(path, text, strlen(text), 0644)) != 0)
	{
		ub_err("%s: %s", path, strerror(-rc));
		return 1;
	}
	memset(kbuf, 0, sizeof kbuf);
	memset(hexk, 0, sizeof hexk);
	printf("fingerprint: %s\n", hexfp);
	return 0;
}
