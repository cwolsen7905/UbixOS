/*
 * localoptions.h — uBixOS Dropbear feature selection (server profile).
 *
 * Overrides src/default_options.h.  Goal: a lean SSH-2 *server* matching
 * uBixOS's console-first identity — password + pubkey auth into a pty shell,
 * no port/agent/X11 forwarding (kept off until the kernel paths are proven).
 * Only included when build.sh defines LOCALOPTIONS_H_EXISTS.
 */
#ifndef DROPBEAR_LOCALOPTIONS_H_
#define DROPBEAR_LOCALOPTIONS_H_

/* Daemon only — uBixOS has no inetd.  No per-connection re-exec (no ASLR to
 * re-randomise, and re-exec relies on Linux /proc/self/exe). */
#define NON_INETD_MODE 1
#define INETD_MODE 0
#define DROPBEAR_REEXEC 0

/* Forwarding & agent: off for the first cut (verifiable follow-ups). */
#define DROPBEAR_X11FWD 0
#define DROPBEAR_SVR_AGENTFWD 0
#define DROPBEAR_SVR_LOCALTCPFWD 0
#define DROPBEAR_SVR_REMOTETCPFWD 0
#define DROPBEAR_SVR_LOCALSTREAMFWD 0

/* Authentication: password (routed to authd via patch) + pubkey. */
#define DROPBEAR_SVR_PASSWORD_AUTH 1
#define DROPBEAR_SVR_PUBKEY_AUTH 1
#define DROPBEAR_SVR_PAM_AUTH 0

/* Host key types: modern set.  DSS stays off (already default). */
#define DROPBEAR_ED25519 1
#define DROPBEAR_ECDSA 1
#define DROPBEAR_RSA 1
#define DROPBEAR_DSS 0

/* Key exchange: curve25519 + DH group14/sha256.  Post-quantum hybrids
 * (sntrup761, mlkem768) are heavy and unproven here — off for now. */
#define DROPBEAR_CURVE25519 1
#define DROPBEAR_DH_GROUP14_SHA256 1
#define DROPBEAR_SNTRUP761 0
#define DROPBEAR_MLKEM768 0

/* Ciphers: chacha20-poly1305 + AES-CTR (all present in libtomcrypt). */
#define DROPBEAR_CHACHA20POLY1305 1
#define DROPBEAR_AES128 1
#define DROPBEAR_AES256 1

#endif /* DROPBEAR_LOCALOPTIONS_H_ */
