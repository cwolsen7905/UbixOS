# dropbear — uBixOS SSH server (sshd)

[Dropbear](https://matt.ucc.asn.au/dropbear/dropbear.html) is a small SSH-2
server/client built for low-resource embedded systems. uBixOS uses it as its
`sshd`, started by initd. Pulled in as a **port** per
`docs/design/third-party-ports-plan.md` (a sibling to the `bmake`/`sh` ports),
**not** vendored into `contrib/`.

## Why Dropbear

- **License:** MIT-style (`LICENSE`) — compatible with uBixOS's BSD-3-Clause.
  Bundled libtomcrypt/libtommath are public-domain/Unlicense.
- **Self-contained crypto:** ships its own libtomcrypt + libtommath — no OpenSSL
  to port (uBixOS's BearSSL is TLS-only and doesn't speak the SSH wire protocol).
- **Embedded-targeted:** small, single-binary, minimal POSIX surface — matches
  uBixOS's console-first, IoT-class identity. (OpenSSH would need OpenSSL +
  privsep/setresuid glue; wolfSSH is GPL.)

## Build

```sh
bmake -C tools/ports/dropbear                 # aarch64 -> build/aarch64/usr/sbin/{dropbear,dropbearkey}
bmake -C tools/ports/dropbear TARGET=x86_64   # x86_64  -> build/x86_64/usr/sbin/...
```

Binaries stage into `build/${ARCH}/usr/sbin/` to mirror the target FS layout —
sshd is a system daemon, so it homes in **`/usr/sbin`** per the POSIX `/usr`-split
migration (`docs/design/filesystem-hierarchy-plan.md`). `tools/mkimage.sh`
copies the staging tree into the image verbatim.

Cross-built against the musl-freestanding world toolchain (like the `bmake`/`sh`
ports). Dropbear's own `configure` runs target compile+**link** tests that fail
under the freestanding toolchain (no sysroot), so we author `config.h` from a
host `./configure` + musl adaptation and a server-profile `localoptions.h`, then
drive the compile/link directly in `build.sh` (bypassing configure and the
generated Makefiles). The bundled libtomcrypt (413 `.c`) + libtommath (155 `.c`)
are glob-compiled into static archives; `gmp_desc.c`/`tfm_desc.c` self-disable
without `-DGMP_DESC`/`-DTFM_DESC`.

### Feature profile (`localoptions.h`)

Lean **server**: password + pubkey auth into a pty shell. Forwarding (TCP/agent/
X11) and the post-quantum KEX hybrids are **off** for the first cut. KEX =
curve25519 + dh-group14-sha256; ciphers = chacha20-poly1305 + AES-CTR; host keys
= ed25519/ecdsa/rsa.

## uBixOS integration (patches/)

- **config.h** disables utmp/wtmp/lastlog/syslog/PAM/libutil (no uBixOS backing);
  `loginrec.c` compiles to no-ops.
- **Zero-arg launch:** initd execs a service binary with *no arguments*
  (`bin/init/main.c`), so a patch bakes the defaults `/bin/dropbear` needs —
  listen on port 22 and auto-generate **ephemeral** host keys (`-R` semantics) so
  no host-key files are required. *(Persistent keys via `dropbearkey` into
  `/etc/dropbear` are a follow-up.)*
- **Password auth → authd:** a patch replaces Dropbear's `getpwnam`/`crypt`
  shadow check with an `authd` `AUTH_REQUEST`/`AUTH_RESPONSE` round-trip (PBKDF2
  via `pw_verify`) — `authd`'s header already anticipated ssh. *(In progress.)*

## Status

- [x] Port recipe + cross-build scaffold (config.h, localoptions.h, build.sh)
- [ ] Clean compile+link on both arches
- [ ] authd-backed password auth + pty session bridge
- [ ] `etc/init.d/30-sshd` + image install; QEMU-verified login over ssh
