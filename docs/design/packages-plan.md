# Binary packages — design

**Status:** proposal (2026-10-15); P1 (`lib/libpkg`, W-069) is implemented (`include/ubix/pkg.h`, tests in `tests/pkgtest`), the rest is plan only. Every section ends in a
**Recommendation** and the **Questions for the person**. Nothing here is
decided until the person answers; rows W-069..W-078 in [`../WORK.md`](../WORK.md)
are `todo`.

**Goal.** An installed uBixOS can `pkg add llvm` and get clang/lld, from
`packages.ubixos.org`, for its own arch (aarch64 or x86_64). A separate git
project, **ubixos-packages**, builds every package for every supported arch and
publishes only what changed. The **package manager** and the **installer** are
equally important and share one library, so neither is an afterthought of the
other.

Companion documents: [third-party-ports-plan.md](third-party-ports-plan.md)
(the recipe shape this reuses), [installer-plan.md](installer-plan.md)
(W-054..W-064), [identity.md](identity.md) idea 4 (W-023, atomic updates),
[self-hosting-plan.md](self-hosting-plan.md) (why the compiler matters).

---

## 0. Boundaries: base versus packages

The rule that everything else hangs on:

| | Base | Package |
|---|------|---------|
| Built from | this tree (`sys/`, `bin/`, `lib/`, `usr.*`, vendored `contrib/`) | a recipe in **ubixos-packages** (pinned upstream tarball + patches) |
| Versioned | in lock-step with the kernel (`sys/include/ubixos/version.h`) | independently (`<upstream version>-r<revision>`) |
| Installed under | `/` (`/bin`, `/lib`, `/usr/bin`, ...) | `/usr/local` (`PREFIX`) |
| Updated by | image / base update (W-023) | `pkg` |
| Needed to build the base | yes (bmake, oksh, musl, BearSSL, zlib, the userland, `views`) | no |

Placing packages under `/usr/local` (the FreeBSD convention) makes "which files
are base and which are package" a path test, keeps `pkg` from ever touching
base files, and lets a rollback or reinstall of one not disturb the other.

Consequences:

- **clang/lld/LLVM binutils are the first package**, not part of the default
  image. Today `tools/mkimage.sh` stages them from `build/${ARCH}/usr` plus the
  clang resource headers under `/usr/lib/clang/18/include` (and `lib/crtbegin`
  stays base: the C runtime objects belong to libc, not to the compiler).
- Candidates after it: `git`, `dropbear` (both already have
  `tools/ports/*` recipes), `nodejs` ([nodejs-port-plan.md](nodejs-port-plan.md)),
  later NetSurf if it is ever decoupled from `world`.
- Anything ABI-coupled to the kernel (`views`, `objgfx`, `term`, the native
  `ubix_api` clients) stays **base**. A package may depend on the base ABI
  (`base: >= 3.1.0`), never the reverse.

**Recommendation.** Adopt the rule above: base = built from this tree and
versioned with the kernel; packages = third-party recipes under `/usr/local`.
Move LLVM out of the default image into the first package.

**Questions for the person.**
1. `/usr/local` as the package prefix, or a uBixOS-specific one (e.g. `/opt`,
   `/usr/pkg`)? [recommend `/usr/local`]
2. Is a default image without a compiler acceptable (self-hosting then needs
   `pkg add llvm` first), or should the `desktop` profile pre-install it via the
   installer's package step (section 5)? [recommend: not in `base`, offered in
   the installer's "development" set]
3. `/usr/local` must be on `PATH`, and clang's resource dir must resolve
   relative to the binary (`/usr/local/lib/clang/18`). OK to drop the base copy
   of `/usr/lib/clang/18/include` once the package exists?

---

## 1. Package format

### Constraints

Buildable and installable with what is already in base: **musl**, **BearSSL**
(`lib/libbearssl`), **zlib** (`contrib/zlib`), **oksh**, **bmake**. No GPL code
anywhere in the packaging path, host or device. Base has no `tar`
(`contrib/busybox` is GPLv2 and is W-068's concern), so the client carries its
own small archive reader.

### Archive

`<name>-<version>-r<rev>.upkg`: a **ustar** stream, **deflate-compressed**
(zlib; gzip framing). Members:

1. `+MANIFEST` first, always (so the client can reject before unpacking).
2. Payload members with paths relative to `/` (`usr/local/bin/clang`), regular
   files, directories and symlinks only. Rejected on read: absolute paths, `..`,
   device/FIFO/hard-link members, setuid/setgid bits unless the manifest lists
   them explicitly, paths outside `PREFIX` for third-party packages.

ustar is chosen because it is trivially implemented (512-byte headers, ~300
lines), streamable, and needs no new dependency; its 100-character name limit is
avoided with the pax `path` extended header for long names (only that one
record type is accepted).

### Manifest

Line-oriented `key: value` records (no JSON/YAML parser exists in base; it is
parseable with `fgets` and `strtok`). One `file:` line per payload entry:

```
name: llvm
version: 18.1.8
revision: 1
arch: aarch64
abi: 3
base: >= 3.1.0
comment: Clang C/C++ compiler, lld linker, LLVM binutils
license: Apache-2.0 WITH LLVM-exception
depends: zlib >= 1.3
conflicts:
installed-size: 612000000
recipe-hash: <sha256 of the recipe directory>
file: 0755 118334312 <sha256> usr/local/bin/clang-18
link: usr/local/bin/clang -> clang-18
dir: 0755 usr/local/lib/clang/18/include
file: 0644 4096 <sha256> usr/local/lib/clang/18/include/stddef.h
```

- `abi` is the OS series (`3` for 3.x); a package is only offered to a matching
  base. `base:` is the minimum kernel/libc version it needs.
- Checksums are **SHA-256** (BearSSL). The manifest is part of the archive and
  is also hashed in the index, so a tampered manifest fails the index check.
- `recipe-hash` makes a republish with unchanged version/revision detectable
  (section 3).
- Config files are `conf: <path>` entries: on upgrade a locally modified one is
  kept and the new one is written beside it (`.upkg-new`).

### Signing

- **Algorithm: ECDSA P-256 over SHA-256.** BearSSL implements verify and sign
  (`br_ecdsa_*_vrfy_asn1`, `*_sign_asn1`) and key generation; P-256/SHA-256 is
  already linked for TLS. No new crypto code. Ed25519 would be nicer but
  BearSSL ships X25519 only, so it would mean vendoring a signature
  implementation.
- **What is signed: the index** (section 2), which carries the SHA-256 and size
  of every package file. Packages are not signed one by one; the chain is
  key → index → package hash → manifest → file hashes. Installing a package that
  is not in a verified index requires `pkg add --local` plus an explicit
  `--no-verify`, which prints a warning (local development, offline media
  testing).
- **Where the key lives.** Public key(s): `/etc/pkg/keys/*.pub` in the base
  image (written by `mkimage`, so the installer media and every installed
  system trust the project key from first boot). Private key: the CI protected
  variable on the ubixos-packages runner, scoped to the protected publish job
  (section 3); a printout of the fingerprint is committed in the repo README.
- **Device verification.** `pkg` downloads `index.<serial>` and
  `index.<serial>.sig`, verifies against every key in `/etc/pkg/keys`, refuses a
  serial lower than the cached one (rollback), refuses an index past its
  `expires` (freeze), then checks each downloaded package's SHA-256 against the
  index before unpacking, and each unpacked file against the manifest.
- **Rotation.** The trust list holds several keys and an index may carry several
  signatures, so rotation = ship the new public key in a base update, sign with
  both for a transition, drop the old one.

**Recommendation.** ustar + zlib `.upkg`, `key: value` manifest, SHA-256, and
ECDSA P-256 signature over the index only, with the public key in the base image.
Everything verifiable with BearSSL + zlib already in the tree.

**Questions for the person.**
1. ECDSA P-256 (no new code) versus vendoring an Ed25519 implementation?
   [recommend ECDSA P-256]
2. One online repo key held in CI, or an offline root key that only endorses
   short-lived CI keys? The second is safer and costs a manual step each
   rotation. [recommend one online key now, multi-key trust list so the second
   model can be adopted later without a client change]
3. gzip/deflate now, accepting that LLVM is ~100+ MB per arch; or add `xz`/`zstd`
   (permissive liblzma/zstd-BSD) to base first? [recommend deflate now; the
   manifest carries a `compression:` field so a new codec is additive]
4. Per-package detached signatures as well, for mirrors that cannot be trusted
   to carry the index? [recommend no; mirrors serve the signed index too]

---

## 2. Repository layout and index

### Origin

`packages.ubixos.org` is served exactly like `cdn.ubixsys.com`
(`~/git/kubernetes/cdn/README.md`): static files in
`kube-003:/mnt/usb/pv/assets/`, nginx read-only, rsync upload with
`--ignore-existing`, `Cache-Control: public, max-age=2592000, immutable`,
directory listings off. The constraints this puts on the design:

1. **Files are never overwritten.** A file name is therefore a content
   identity: new version or revision, new name.
2. `immutable` plus 30 days means *nothing* may be a mutable URL that clients
   rely on seeing fresh. In particular a plain `index.json` or `latest` file
   cannot work.
3. No directory listing: the client must never need to enumerate.
4. The disk (114 G USB flash) is fast to read, slow to write, and finite;
   "never overwrite" also means "never delete" unless a human does it.

### Layout

```
/packages/<abi>/<arch>/
    index/index.<serial>            signed index, serial is a monotonic integer
    index/index.<serial>.sig
    pool/<name>/<name>-<version>-r<rev>.upkg
    keys/<key-fingerprint>.pub      public keys, for humans and bootstrap
```

- `<abi>` is the OS series (`3`). A musl/kernel ABI break starts a new series
  rather than mutating one.
- `<arch>` is `aarch64` or `x86_64`; each has its own index, because a package
  may exist for only one arch.
- The path under `/packages/` keeps the package tree out of the CDN's existing
  asset namespace if the same disk and pod serve both.

### Index

One record per package, `key: value`, blank-line separated, after a header:

```
serial: 42
abi: 3
arch: aarch64
created: 2026-10-15T12:00:00Z
expires: 2026-12-15T12:00:00Z
previous: <sha256 of index.41>

name: llvm
version: 18.1.8
revision: 1
file: pool/llvm/llvm-18.1.8-r1.upkg
size: 98304000
sha256: <sha256 of the .upkg>
depends: zlib >= 1.3
base: >= 3.1.0
comment: ...
```

`previous` chains the indexes so a client that has seen serial N detects a
repository that rewrote history.

### The "latest index" problem

The client has to find the newest index without a mutable file. Two
mechanisms, in order of preference:

- **A. Probe forward on immutable names (works with the CDN as-is).** Serials
  are dense (`index.1`, `index.2`, ...) and never removed, so "exists" is
  monotone. A client holding serial *S* requests `index.<S+1>`; a 404 means it
  is current, a hit means keep going. A fresh client (no *S*) gallops
  (1, 2, 4, 8, ...) then bisects, so first contact costs O(log N) requests. A
  404 may be cached for a short time by Cloudflare; the worst case is a client
  seeing an update a few minutes late. The installer media ships its baseline
  index, so even a fresh install starts from a nearby serial.
- **B. One deliberately mutable pointer.** A single `latest` file served with
  `Cache-Control: no-cache`, written by a rsync *without* `--ignore-existing`.
  Cheaper for the client (one request) but needs a `location = /packages/.../latest`
  override in `~/git/kubernetes/cdn/nginx.yml` — an ops change, and the one
  exception to "immutable". Not needed for correctness.

### Publishing order and retention

1. rsync new `pool/` files, 2. rsync `index.<serial>`, 3. rsync the `.sig`
last. A reader that finds the `.sig` therefore finds everything it names.
`--ignore-existing` skips silently on a collision, so CI re-fetches what it
uploaded and compares hashes (a collision means two pipelines raced; publishing
is a serialised job, section 3).

Old versions stay on disk so a pinned install still resolves. The index lists
only current versions; space is reclaimed by a human, rarely, via
`files.ubixsys.com`/ssh, never by the pipeline (it has no delete path by
design).

### Domain and origin wiring

`packages.ubixos.org` is **not** under `ubixsys.com`: the origin needs its own
DNS record, an ingress host rule, and a certificate (`*.ubixsys.com` does not
cover it). The cdn README also notes the existing records point at a private
address, so public reachability needs a Cloudflare-proxied record on the public
IP. Cloudflare in front is strongly advised: LLVM-sized downloads would
otherwise all cross one home uplink to one Raspberry Pi.

**Recommendation.** Layout `/packages/<abi>/<arch>/{index,pool,keys}`; indexes
named by serial and discovered by forward probing (A); skip the mutable pointer
(B) until measurement says probing is too slow; publish pool → index → sig;
never delete from the pipeline. Serve the path from the same nginx pod as an
additional ingress host.

**Questions for the person.**
1. Is `packages.ubixos.org` the final name, and who owns its DNS (Cloudflare
   account for ubixos.org)? Will it be public, or LAN-only at first?
2. Serve from the existing `cdn` nginx (new host + `/packages` root) or a
   separate deployment/PV? The first is less to run; the second isolates it.
3. Allow the one mutable `latest` pointer (needs an nginx location in the
   kubernetes repo), or stay purely immutable? [recommend purely immutable]
4. Retention: keep every version forever, or keep the last N per package and
   have the person prune? What disk budget (114 G shared with other assets)?
5. Index lifetime (`expires`): 60 days is proposed; CI re-signs and bumps the
   serial on a schedule (an index with no package change) so it never expires
   on a quiet repo. Acceptable churn?

---

## 3. The build project: ubixos-packages

### Shape (reuses the ports framework)

```
ubixos-packages/
    share/mk/ports.mk          # fetch -> verify -> extract -> patch (copied from UbixOS, resynced by a script)
    share/mk/pkg.mk            # adds: do-stage (DESTDIR), do-package, recipe-hash
    recipes/<name>/
        Makefile               # PORT_NAME PORT_VERSION PORT_URL PORT_SHA256, PKG_REV, PKG_DEPENDS, do-build, do-install
        patches/*.patch
        pkg.conf               # comment, license, conf files, split rules
        README.md
    tools/ubpkg/               # host tool: build a package, make+sign an index (uses libpkg)
    ci/                        # pipeline definition + plan script
```

The recipe is `tools/ports/<name>/Makefile` with two additions: `PKG_REV` and a
`do-install` into a `DESTDIR` instead of into `build/${ARCH}`. The existing
`tools/ports/llvm/{Makefile,build.sh,ubixos*.cmake}` moves over nearly
unchanged and keeps its two-phase cross build (host tblgen, then cross). (Note:
the LLVM recipe is `tools/ports/llvm/` in this tree; there is no
`tools/build-llvm.sh`.)

### What the builder needs

Packages are built **against a sysroot**: musl, libc++/libcxxabi, BearSSL and
the system headers of the matching base release. That sysroot is published by
the UbixOS repo as a build artifact (`ubixos-sysroot-<version>-<arch>`), pinned
by version in ubixos-packages. It is itself a package-shaped artifact, so the
same repository can serve it. It also decides `abi:`.

### Change detection: build only on a version or revision change

Per recipe and arch, CI's `plan` step computes the file name
`<name>-<version>-r<rev>.upkg` and asks the current index (probed as in section
2) whether it exists.

- Present: skip. Absent: build.
- **Guard against silent drift.** The manifest records `recipe-hash`. If a
  recipe directory's hash differs from the published package of the same
  name/version/revision, `plan` fails the pipeline with "bump `PKG_REV`" rather
  than building (it could not be published anyway: never overwrite).
- Dependents are **not** rebuilt automatically when a dependency's revision
  changes; recipes that link ABI-sensitive libraries list them in
  `PKG_ABI_DEPS`, and `plan` fails with the list of dependents whose revision
  needs bumping. Explicit beats a hidden rebuild cascade that would mint new
  LLVM files on every zlib bump.
- Upstream-version watching (bump PRs) is a later nicety.

### Per-arch builds: cross, not native

A native in-OS build is too slow to be the pipeline (`bmake -j4 musl-libc`
takes ~2 h 11 min in-OS on aarch64 under HVF, W-043). Instead:

- **Cross-build both arches on one Linux or macOS builder** using the toolchain
  already used for `bmake world` (`aarch64-elf-gcc`, `x86_64-elf-gcc`, cmake,
  ninja, bmake), pinned in a builder container image. The LLVM recipe already
  cross-builds both targets from one host.
- **Gate by running the result.** Before an index is published, a smoke job
  boots the matching QEMU image (HVF/KVM for aarch64, TCG for x86_64), runs
  `pkg add` of the new packages from a local repo, and runs a per-recipe check
  (`clang --version`, compile and run a hello, `ld.lld --version`). A package
  that does not run on its target is never indexed.
- A native runner per arch is only worth adding if cross builds of a recipe
  prove impossible (autoconf-heavy ports that run test binaries).

### Publishing

A `publish` job on the default branch only, one at a time (a CI
resource lock), does: allocate `serial = latest + 1`; build/refresh the index;
sign it; rsync pool then index then sig to `kube-003`
with `--ignore-existing`; re-fetch and verify. Credentials: an SSH key limited to
`/mnt/usb/pv/assets/packages/` (rsync-only, e.g. `command=` restricted or an
rrsync wrapper), kept as a protected CI variable.

### Where it lives: GitHub or the brainchurts GitLab

| | GitHub (where UbixOS lives) | brainchurts GitLab |
|---|------|---------|
| Reach kube-003 (10.50.18.43, LAN) | **No**: hosted runners are on the internet; needs a self-hosted runner on the LAN | **Yes**: runners are on the home network |
| Build capacity (LLVM: many GB disk, GB-class link RAM, hours) | Hosted standard runners are small (disk, 6 h job cap); big runners cost money | Own runners: sized as needed, no time cap |
| Secrets (signing key, SSH key) | Fine, but a self-hosted runner on a *public* repo runs fork-PR code unless carefully restricted | Protected variables/branches, private project |
| Visibility / contribution | Public, same place as the OS | Private-ish by default; can mirror read-only to GitHub |
| Resource locks for serial allocation | `concurrency` groups | `resource_group` |

The deciding fact is that the publish target is on a private LAN and the
builds are heavy: GitHub would need a self-hosted runner anyway, with the
public-repo fork risk, while GitLab already has them.

**Recommendation.** ubixos-packages lives on the brainchurts GitLab: private
repo, build on its runners (cross-build both arches), publish from protected
default-branch pipelines only, read-only mirror to GitHub for visibility. The
UbixOS repo stays on GitHub and publishes its sysroot artifact for the packages
project to consume.

**Questions for the person.**
1. GitLab as recommended? What are the runners' arch, RAM and disk (LLVM needs
   several GB RAM at link, tens of GB of disk), and is there a runner that can
   run QEMU with KVM/HVF for the smoke gate?
2. Is the repo private or public? (Recipes and patches are not secret; public
   enables contributions.)
3. Who holds the repo signing key and the rsync SSH key, and where (GitLab
   protected variables on the runner, or a hardware token with a manual sign
   step)?
4. The builder needs the UbixOS sysroot. Publish it as a UbixOS release
   artifact on GitHub, or build it inside ubixos-packages from a pinned UbixOS
   commit?

---

## 4. The on-device client

### Commands

`pkg` (`usr.sbin/pkg`, thin front end over `lib/libpkg`):

| Command | Meaning |
|---------|---------|
| `pkg update` | fetch and verify the newest index for this arch/abi |
| `pkg search <pat>` / `pkg info [name]` | query the index / the installed database |
| `pkg add <name>...` | resolve dependencies, download, verify, install |
| `pkg add --local <file.upkg>` | install a file (offline media, development) |
| `pkg del <name>` | remove; refuse if something installed depends on it unless `--force` |
| `pkg upgrade [name...]` | install newer versions listed in the index |
| `pkg verify [name...]` | re-hash installed files against their manifests; report drift |
| `pkg --root <dir> ...` | operate on another root (used by `mkimage` and the installer) |

Fetching uses `lib/libhttp` (`http_get`, BearSSL TLS 1.2). The package payload
integrity does not depend on TLS (index signature plus hashes), so a mirror may
be plain HTTP.

### Files and database

```
/etc/pkg/repo.conf              repository URL(s), abi, arch   (admin-owned)
/etc/pkg/keys/*.pub             trusted public keys            (base)
/var/db/pkg/<name>/MANIFEST     installed manifest
/var/db/pkg/<name>/FILES        path, mode, sha256 (as installed)
/var/db/pkg/base/MANIFEST       the base set's file list (see below)
/var/db/pkg/index.<serial>      last verified index (+ .sig)
/var/cache/pkg/                 downloaded .upkg, kept until verified+installed
/var/lock/pkg.lock              single transaction at a time
```

Plain files, no database engine: enumerable with `ls`/`grep`, recoverable by
hand, and consistent with how ubistry started. Note that
[filesystem-hierarchy-plan.md](filesystem-hierarchy-plan.md) does not define
`/var` yet; this plan needs `/var/db`, `/var/cache`, `/var/lock` there.

### Transactions

Per install: take the lock; download and verify everything first; unpack each
file to `<path>.upkg-tmp` and `rename(2)` into place (atomic per file since
W-051); write `/var/db/pkg/<name>` last. A journal file in `/var/db/pkg/` lists
the in-progress transaction so an interrupted `pkg` can roll forward or back on
the next run. Removal deletes files first, the database entry last, and only
files whose hash still matches unless `--force`.

### As built (P3, W-071)

`usr.sbin/pkg` implements `add --local`, `del`, `info`, `verify` and `--root`;
host tests are `bmake -C tests/pkgdev host`.

- **Local signature.** A package outside a signed index is checked against a
  detached `<file>.upkg.sig`: `ecdsa-p256-sha256: <hex DER>` lines (the
  `pkg_sigfile_verify` format) over the message `<lowercase hex SHA-256 of the
  whole .upkg>\n`, so a very large package is streamed, not held in memory.
  The signature must verify under at least one key in `/etc/pkg/keys/*.pub`.
  No signature or no trusted key refuses the add; `--no-verify` installs anyway
  with a warning. (`ubpkg` signs indexes, not packages, so signing a loose file
  is `tests/pkgdev/mkpkg sign` for now.)
- **`FILES` format**, one line per manifest entry:
  `F <octal mode> <sha256> <path>`, `D <octal mode> - <path>`,
  `L <octal mode> - <path> -> <target>`. `/var/db/pkg/base/MANIFEST`, when
  present, uses the same lines and its files are treated as owned by `base`
  (W-076 writes it).
- **Add checks, in order, before touching the disk:** signature; arch, abi,
  `base:` against the running OS; not already installed; `depends:` installed
  and satisfied; no installed package matches `conflicts:`; every path is under
  `usr/local/`, passes through no symlink (on disk or made by the package), and
  is neither owned by another package or base nor already on disk.
- **Transaction.** `/var/lock/pkg.lock` (pid; a dead owner's lock is stolen),
  then `/var/db/pkg/.journal` (`add <name>`, then `D <dir>` / `F <file>` lines
  written before each action). Files are written to `<path>.upkg-tmp`, hashed as
  written, compared with the manifest and renamed. The record is built in
  `/var/db/pkg/.new-<name>/` and renamed to `<name>` last: that rename is the
  commit. Any failure, or the next `pkg` run after a crash, replays the journal
  backwards and removes exactly what the add created. An interrupted `del` is
  rolled forward.
- **`del`** removes files whose hash still matches (others are kept and named),
  then empty directories, then the record; refused if another installed package
  `depends:` on it unless `--force`, which also removes modified files.
- Setuid/setgid members and packages touching paths outside `usr/local/` are
  refused for now. Upgrade-in-place is P4: re-adding an installed name is refused.
- `mkimage` creates `/var/db/pkg`, `/var/cache/pkg`, `/var/lock` and copies
  `etc/pkg/keys/*.pub` into `/etc/pkg/keys`. The project key itself is not in
  the tree yet.

### Relationship to base and to W-023

- **Base is recorded, not managed by `pkg`.** `mkimage` (and the installer)
  write `/var/db/pkg/base/MANIFEST`: every base file with its hash. `pkg verify`
  and `pkg add` use it to refuse a package that would overwrite a base file and
  to tell the person what is base. `pkg` never modifies base files.
- **Base updates are W-023's mechanism, not `pkg`'s.** Idea 4 is a
  snapshot-backed atomic switch of the boot dataset. Snapshots are not
  implemented (three format hooks only). Until they are, a base update is "write
  a new image". Packages live in `/usr/local` and `/var/db/pkg`, which idea 4
  classifies as *state vs system*: they must travel with the system dataset (a
  rolled-back base must not keep packages built against a newer ABI). Open in
  W-023.
- **Hook, not coupling.** When snapshots exist, `pkg` calls
  `ubfs snapshot` before a transaction and offers `pkg rollback`. A package's
  `base:` constraint is checked on install and again by `pkg upgrade`, so a base
  update that outruns installed packages is reported rather than silent.
- A base update could later itself be a repository item (`base-<version>`
  pseudo-package carrying an image payload for the W-023 switcher). Not in scope
  here; the format does not preclude it.

**Recommendation.** `pkg` with the command set above over `lib/libpkg`, flat-file
database under `/var/db/pkg`, file-granular atomic installs with a journal now,
snapshot integration as a later row once W-023 has snapshots. Base is recorded
as a manifest and never managed by `pkg`.

**Questions for the person.**
1. Command name `pkg`, or something uBix-flavoured (`ubpkg`, matching
   `ubdisk`/`ubpool`/`ubinstall`)? [the host tool is `ubpkg`; the device
   command `pkg` is proposed for familiarity]
2. May this plan define `/var/db`, `/var/cache`, `/var/lock` in the filesystem
   hierarchy plan?
3. Should `pkg upgrade` ever run unattended (a timer), or manual only?
   [recommend manual]
4. Dependency solver: a simple depth-first install order with no version
   backtracking (recommended: packages are few and the index lists one version
   per name) versus a real solver?

---

## 5. Installer tie-in

The installer ([installer-plan.md](installer-plan.md)) lays the same layout
`mkimage.sh` does today and, by its open choice 4, writes a **prebuilt pool
image**. That image is the **base set**. Packages are an optional second step.

### Sets

| Set | Contents | Delivered as |
|-----|----------|--------------|
| `base` | kernel, loader, CLI world (today's `PROFILE=base`) | prebuilt pool image |
| `desktop` | `views`, `objgfx`, GUI apps, assets (today's `PROFILE=desktop`) | in the pool image, or a second image layer; ABI-coupled, so base, not packages |
| `development` | `llvm` (clang/lld/binutils) and its dependencies | packages |
| extras | `git`, `dropbear`, ... | packages |

### Flow

1. B7/B8 write the base pool image and the boot partition, as planned.
2. `mkimage` (and the installer) create `/var/db/pkg/base` and install the
   public key into `/etc/pkg/keys`.
3. The package step shows the sets; selected packages are installed with
   `libpkg` directly (the same code path as `pkg add --root /mnt/target`), not
   by exec'ing a shell command, so the text (`ubinstall`) and graphical front
   ends share it exactly as they already share `libinstall`.
4. Source of packages: the **network** (DHCP is available in the live
   environment) or the **media's own repository**.

### Offline media

`tools/mkmedia.sh` (W-057) can copy a subset of the repository onto the media,
in exactly the origin's layout (`/packages/<abi>/<arch>/{index,pool}`), plus its
signed index. `pkg` treats a path or `file://` URL as a repository like any
other, and verification is identical, so an offline install needs no special
code and an installed system keeps the media's repo URL as a fallback.
`mkmedia` chooses the set to bundle by a size budget (LLVM is the large item).

### Image builds consume packages

Today `mkimage` stages LLVM from a local `build/` cache. Once packages exist,
`mkimage PROFILE=...` can install the `development` set from a repository
snapshot with `pkg --root`, so the image and an installed system use one code
path and the in-tree LLVM staging block can be deleted.

### Ordering

`libpkg` (archive + manifest + signature verification) must exist before B7 so
`libinstall` has a package step to call; B7 itself does not depend on the
network repo or on LLVM being published.

**Recommendation.** Base and desktop are image content; third-party software
(first: LLVM) is packages, installed in an optional installer step over `libpkg`,
from the network or from a bundled repository on the live media. Build
`libpkg` before B7.

**Questions for the person.**
1. Is `desktop` (views and GUI) part of the base image or an installable set? As
   built from this tree it is ABI-coupled, so this plan keeps it in the image.
2. Should the live media include the `development` set by default (a large
   image), or only on request?
3. Is the installer's package step mandatory-network, or must it always work
   offline from the media?
4. Does the installed system keep the media's repository as a configured
   fallback?

---

## 6. Phased work list

One PR-sized row per step. Rows are in [`../WORK.md`](../WORK.md) as `todo`.
Row ids are allocated here; renumber at merge if another lane took them.

| Phase | Row | Item | Depends on |
|-------|-----|------|-----------|
| P1 | W-069 | `lib/libpkg`: ustar+deflate archive reader, manifest/index parser, SHA-256 and ECDSA verify; host-side unit tests | - |
| P2 | W-070 | `ubpkg` host tool: build a package from a staging dir, build and sign an index, `--root` install; key generation | W-069 |
| P3 | W-071 | `pkg` on-device: `add --local`, `del`, `info`, `verify`, database, base manifest, key trust list in the image | W-069 |
| P4 | W-072 | `pkg update/add/upgrade` from a repository: `libhttp` fetch, index probing, dependency order | W-071 |
| P5 | W-073 | ubixos-packages bootstrap: ports.mk + pkg.mk, the `plan` change-detection step, `hello` package, builder image, GitLab pipeline | W-070 |
| P6 | W-074 | Origin and publish: packages.ubixos.org DNS/ingress/cert, rsync job, serial allocation, post-upload verify | W-073 and the person's answers |
| P7 | W-075 | **clang/lld as the first package**: llvm recipe moved over, both arches, QEMU smoke gate; drop LLVM staging from `mkimage` | W-072, W-074 |
| P8 | W-076 | Base relationship: `mkimage` writes `/var/db/pkg/base`; `base:` constraint checks; `/var` in the hierarchy plan | W-071 |
| P9 | W-077 | Installer tie-in: package step in `libinstall`, set selection, bundled repository in `mkmedia` | W-060, W-057, W-072 |
| P10 | W-078 | Snapshot hook: `pkg` snapshots before a transaction and offers rollback | W-023 has snapshots |

P1..P4 need nothing outside the UbixOS repo and can start now. P5..P7 wait on
the answers in sections 2 and 3. The first user-visible result is P7.

**Recommendation.** Start P1 and P2 (format and tooling, host-testable, no
hardware or infrastructure) in parallel with the person's answers on the origin
and the CI host; land P7 as the milestone.

**Questions for the person.**
1. P1 first, as proposed, or the infrastructure rows (P6) first because they
   are the long pole?
2. Split LLVM into `clang`, `lld` and `llvm-tools` packages in P7, or ship one
   `llvm` package and split later? [recommend one package first]
3. Which arch ships first if P7 has to be staged: aarch64 (primary) with
   x86_64 following? [recommend both from the first publish, since the
   pipeline is cross-built]
