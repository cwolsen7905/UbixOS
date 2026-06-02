/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * libpw — password key-derivation primitives for UbixOS, built on BearSSL.
 * Passwords are HASHED with a salted, iterated KDF, never encrypted.
 */

#ifndef _PW_PW_H
#define _PW_PW_H

#include <stddef.h>
#include <stdint.h>

/**
 * Derive a key from a password using PBKDF2-HMAC-SHA256 (RFC 8018 / RFC 2898).
 *
 * Deterministic and one-way: the same inputs always yield the same output, and
 * the password cannot be recovered from it.  Use a per-user random @p salt and
 * a high @p iterations count to slow brute-force attacks on a stolen database.
 *
 * @param pw          password bytes (not necessarily NUL-terminated).
 * @param salt        per-user random salt; >= 16 bytes recommended.
 * @param iterations  PRF iteration count; must be >= 1, higher is slower/safer.
 * @param out         caller buffer receiving @p outlen derived bytes.
 * @param outlen      desired key length in bytes (e.g. 32).
 */
void pw_pbkdf2_sha256(const void *pw, size_t pwlen, const void *salt,
    size_t saltlen, uint32_t iterations, unsigned char *out, size_t outlen);

#endif /* _PW_PW_H */
