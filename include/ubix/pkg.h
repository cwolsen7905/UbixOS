/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * libpkg — the package-format library shared by the ubpkg host tool, the
 * on-device pkg client and the installer.  See docs/design/packages-plan.md.
 *
 * Pieces: SHA-256 and ECDSA P-256 verification (over BearSSL), a streaming
 * ustar-over-gzip archive reader, the `key: value` manifest and repository
 * index parsers, and package version comparison.  Pure library: all input
 * arrives through memory buffers or a caller-supplied read callback.
 *
 * Failure convention: 0 (or a count) on success, -errno on failure.  The
 * specific codes are documented per function.
 */
#ifndef UBIXOS_PKG_H
#define UBIXOS_PKG_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define PKG_SHA256_LEN 32
#define PKG_SHA256_HEX 65    /**< 64 hex digits + NUL */
#define PKG_ECDSA_PUB_LEN 65 /**< uncompressed P-256 point: 0x04 || X || Y */
#define PKG_PATH_MAX 1024
#define PKG_NAME_MAX 64
#define PKG_VER_MAX 64
#define PKG_MANIFEST_NAME "+MANIFEST"

/* ---- SHA-256 and hex -------------------------------------------------- */

/** Streaming SHA-256 state (opaque). */
struct pkg_sha256
{
	uint64_t opaque[16];
};

void pkg_sha256_init(struct pkg_sha256 *s);
void pkg_sha256_update(struct pkg_sha256 *s, const void *data, size_t len);
void pkg_sha256_final(struct pkg_sha256 *s, uint8_t out[PKG_SHA256_LEN]);
void pkg_sha256(const void *data, size_t len, uint8_t out[PKG_SHA256_LEN]);

/** Encode len bytes as lowercase hex into out (2*len + 1 bytes, NUL-terminated). */
void pkg_hex_encode(const uint8_t *in, size_t len, char *out);
/** Decode exactly 2*len hex digits (either case) from hex into out.  @return 0 or -EINVAL. */
int pkg_hex_decode(const char *hex, size_t hexlen, uint8_t *out, size_t len);

/* ---- signatures ------------------------------------------------------- */

/** One trusted ECDSA P-256 public key. */
struct pkg_key
{
	uint8_t point[PKG_ECDSA_PUB_LEN];
};

/**
 * Parse a key file: `ecdsa-p256: <130 hex digits>` (uncompressed point), with
 * `#` comment lines and blank lines allowed.
 * @return 0, or -EINVAL if no well-formed key is present.
 */
int pkg_key_parse(const char *text, size_t len, struct pkg_key *key);

/**
 * Verify one ECDSA P-256 / SHA-256 signature (ASN.1 DER) over data.
 * @return 0 if valid, -EBADMSG if the signature does not verify, -EINVAL for a
 *         malformed key.
 */
int pkg_ecdsa_verify(const struct pkg_key *key, const void *data, size_t len, const uint8_t *sig, size_t siglen);

/**
 * Verify a signature file against a trust list.  The file holds one or more
 * `ecdsa-p256-sha256: <hex DER>` lines; success needs at least one signature
 * that verifies under at least one key (so a key rotation can sign with both).
 * @return 0 if some signature verifies, -EBADMSG if none does, -EINVAL if the
 *         file has no signature lines or the trust list is empty.
 */
int pkg_sigfile_verify(
    const char *sigtext, size_t siglen, const void *data, size_t len, const struct pkg_key *keys, size_t nkeys);

/* ---- paths and versions ----------------------------------------------- */

/**
 * Policy check for an archive/manifest path: relative, non-empty, no `..` or
 * empty components, no control characters, shorter than PKG_PATH_MAX.
 * @return 1 if acceptable, 0 if not.
 */
int pkg_path_is_safe(const char *path);

/**
 * Compare two version strings (`1.10` > `1.9`, `1.0a` < `1.0b`).  Numeric
 * runs compare as numbers, other runs bytewise; a longer version with equal
 * prefix is greater.
 * @return <0, 0 or >0.
 */
int pkg_version_cmp(const char *a, const char *b);

/** Comparison operator of a dependency or base constraint. */
enum pkg_op
{
	PKG_OP_ANY = 0, /**< no version constraint */
	PKG_OP_EQ,
	PKG_OP_GE,
	PKG_OP_GT,
	PKG_OP_LE,
	PKG_OP_LT,
};

/** `name [op version]`; for the manifest `base:` field name is empty. */
struct pkg_dep
{
	char name[PKG_NAME_MAX];
	enum pkg_op op;
	char version[PKG_VER_MAX];
};

/** @return 1 if `have` satisfies the constraint's op/version, else 0. */
int pkg_dep_satisfied(const struct pkg_dep *dep, const char *have);

/* ---- archive reader --------------------------------------------------- */

enum pkg_type
{
	PKG_FILE = 1,
	PKG_DIR,
	PKG_SYMLINK,
};

/** One archive member header (after validation). */
struct pkg_entry
{
	char path[PKG_PATH_MAX];
	char link[PKG_PATH_MAX]; /**< target, for PKG_SYMLINK */
	enum pkg_type type;
	uint32_t mode; /**< permission bits incl. setuid/setgid/sticky */
	uint64_t size; /**< payload bytes (0 for directories and links) */
	int64_t mtime;
};

/** Reader flags. */
#define PKG_READ_ALLOW_SETID 0x1 /**< accept setuid/setgid members (manifest must list them) */

/**
 * Pull-style source of compressed bytes.
 * @return bytes read (0 = end of input), or -errno.
 */
typedef long (*pkg_read_fn)(void *ctx, void *buf, size_t len);

/** Streaming reader over a gzip-compressed ustar stream (opaque; ~20 KiB). */
struct pkg_reader
{
	void *z; /**< z_stream, heap allocated */
	pkg_read_fn read;
	void *ctx;
	unsigned flags;
	const char *prefix; /**< if set, payload paths must start with it */
	uint8_t in[8192];
	int in_eof;
	int stream_end;
	int first;
	int done;
	uint64_t remaining; /**< unread payload bytes of current entry */
	uint64_t pad;       /**< padding bytes to skip after payload */
	int err;
};

/**
 * Start reading.  `prefix` (may be NULL) restricts payload paths, e.g.
 * "usr/local/"; `+MANIFEST` is always exempt and must be the first member.
 * @return 0, -ENOMEM.
 */
int pkg_reader_open(struct pkg_reader *r, pkg_read_fn read, void *ctx, unsigned flags, const char *prefix);

/**
 * Advance to the next member, skipping unread payload of the current one.
 * @return 1 and fills *e; 0 at the clean end of the archive; -EINVAL for a
 *         malformed archive (bad checksum, truncation, unsupported pax
 *         record), -EPERM for a member the policy rejects (absolute or `..`
 *         path, device/FIFO/hard link, setid without the flag, outside the
 *         prefix, first member not +MANIFEST), -EIO for a read/inflate error.
 */
int pkg_reader_next(struct pkg_reader *r, struct pkg_entry *e);

/**
 * Read payload of the current member.
 * @return bytes read, 0 at the end of the member, -errno.
 */
long pkg_reader_read(struct pkg_reader *r, void *buf, size_t len);

/** Release the reader. */
void pkg_reader_close(struct pkg_reader *r);

/* ---- manifest --------------------------------------------------------- */

enum pkg_mtype
{
	PKG_M_FILE = 1,
	PKG_M_DIR,
	PKG_M_LINK,
};

/** One `file:`, `dir:` or `link:` manifest line. */
struct pkg_mentry
{
	enum pkg_mtype type;
	uint32_t mode;
	uint64_t size;
	uint8_t sha256[PKG_SHA256_LEN]; /**< files only */
	int conf;                       /**< listed in a `conf:` line */
	char *path;
	char *target; /**< links only */
};

struct pkg_manifest
{
	char name[PKG_NAME_MAX];
	char version[PKG_VER_MAX];
	uint32_t revision;
	char arch[16];
	uint32_t abi;
	struct pkg_dep base; /**< constraint on the base system */
	char comment[256];
	char license[128];
	char compression[16];
	uint64_t installed_size;
	char recipe_hash[PKG_SHA256_HEX];
	struct pkg_dep *depends;
	size_t ndepends;
	struct pkg_dep *conflicts;
	size_t nconflicts;
	struct pkg_mentry *entries;
	size_t nentries;
};

/**
 * Parse a +MANIFEST.  Unknown keys are ignored (forward compatible); `name`,
 * `version`, `arch` and `abi` are required.  Entry paths are checked with
 * pkg_path_is_safe and must be unique.
 * @return 0, -EINVAL (malformed, with *line set to the 1-based line if non-NULL),
 *         -ENOMEM.
 */
int pkg_manifest_parse(const char *text, size_t len, struct pkg_manifest *m, unsigned *line);

/** @return the entry for `path`, or NULL. */
const struct pkg_mentry *pkg_manifest_find(const struct pkg_manifest *m, const char *path);

/** Release everything pkg_manifest_parse allocated. */
void pkg_manifest_free(struct pkg_manifest *m);

/* ---- repository index ------------------------------------------------- */

/** One package record of an index. */
struct pkg_irec
{
	char name[PKG_NAME_MAX];
	char version[PKG_VER_MAX];
	uint32_t revision;
	char file[PKG_PATH_MAX];
	uint64_t size;
	uint8_t sha256[PKG_SHA256_LEN];
	struct pkg_dep base;
	struct pkg_dep *depends;
	size_t ndepends;
	char comment[256];
};

struct pkg_index
{
	uint64_t serial;
	uint32_t abi;
	char arch[16];
	time_t created;
	time_t expires; /**< 0 = never */
	uint8_t previous[PKG_SHA256_LEN];
	int has_previous;
	struct pkg_irec *recs;
	size_t nrecs;
};

/**
 * Parse an index: a header block (serial, abi, arch, created, expires,
 * previous) then blank-line-separated package records.  `serial`, `abi` and
 * `arch` are required; records need name, version, file, size and sha256.
 * @return 0, -EINVAL (with *line as for pkg_manifest_parse), -ENOMEM.
 */
int pkg_index_parse(const char *text, size_t len, struct pkg_index *ix, unsigned *line);

/**
 * Freshness policy: refuse a serial below min_serial (rollback) and an index
 * whose `expires` has passed (freeze).
 * @return 0, -ERANGE for rollback, -ESTALE for expired.
 */
int pkg_index_check(const struct pkg_index *ix, time_t now, uint64_t min_serial);

/** @return the highest-versioned record named `name`, or NULL. */
const struct pkg_irec *pkg_index_find(const struct pkg_index *ix, const char *name);

/** Release everything pkg_index_parse allocated. */
void pkg_index_free(struct pkg_index *ix);

/** Parse `YYYY-MM-DDTHH:MM:SSZ`.  @return 0 or -EINVAL. */
int pkg_time_parse(const char *s, time_t *out);

#endif /* UBIXOS_PKG_H */
