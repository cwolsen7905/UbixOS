/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * Fletcher-4 checksum — the default block/uberblock integrity check for UbixFS.
 * Cheap, position-sensitive 256-bit checksum (4 x 64-bit accumulators).  One of
 * the pluggable checksum strategies (enum ubfs_checksum); stronger algos (SHA-256)
 * slot in later without touching callers.
 */
#ifndef _UBIXFS_FLETCHER_H
#define _UBIXFS_FLETCHER_H

#include <stdint.h>
#include <stddef.h>

/**
 * Compute the Fletcher-4 checksum of `len` bytes (must be a multiple of 4) into
 * out[0..3].
 */
void ubfs_fletcher4(const void *buf, size_t len, uint64_t out[4]);

#endif /* _UBIXFS_FLETCHER_H */
