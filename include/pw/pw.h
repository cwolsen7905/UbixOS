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
void pw_pbkdf2_sha256(const void *pw,
                      size_t pwlen,
                      const void *salt,
                      size_t saltlen,
                      uint32_t iterations,
                      unsigned char *out,
                      size_t outlen);

/* PBKDF2 iteration count used by pw_hash().  Tunable: higher is slower to
 * brute-force but adds login latency (emulated i386 is the limiting case). */
#define PW_DEFAULT_ITERATIONS 50000

/* A formatted hash never exceeds this many bytes including the NUL.
 * "$pbkdf2-sha256$" + iter + "$" + 24 (16B salt) + "$" + 44 (32B hash) ~= 90. */
#define PW_HASH_STRLEN 128

/**
 * Hash @p password into the modular-crypt string
 *   $pbkdf2-sha256$<iterations>$<base64 salt>$<base64 hash>
 * using a fresh random salt from getentropy().  Store the result and later
 * check it with pw_verify().
 *
 * @param out     buffer of at least PW_HASH_STRLEN bytes.
 * @return 0 on success, -1 on failure (no entropy or buffer too small).
 */
int pw_hash(const char *password, char *out, size_t outlen);

/**
 * Constant-time check of @p password against a @p stored pw_hash() string.
 * @return 1 if it matches, 0 if not, -1 if @p stored is malformed.
 */
int pw_verify(const char *password, const char *stored);

#endif /* _PW_PW_H */
