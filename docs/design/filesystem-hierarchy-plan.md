# Filesystem hierarchy plan — toward a POSIX `/usr` layout

**Status:** Phases 1–2 IMPLEMENTED (2026-06-26) — the source tree was split into
`bin/` + `sbin/` + `usr.bin/` + `usr.sbin/` + `tests/` (FreeBSD/macOS layout), and
all daemons/admin-tools/user-commands/GUI-apps/test-harnesses re-homed
accordingly. Phase 3 (symlink usrmerge once the pool is the universal root)
remains future work. **Owner:** [sshd] session (Phase 0); full split [self-hosting].
**Decision:** classic `/usr` split with **real directories** (no symlink
usrmerge), migrated **incrementally** (dropbear was the first mover).

**Mechanism (how it works now):** a program's *source tree* decides its install
path. Each tree (`sbin/`, `usr.bin/`, `usr.sbin/`, `tests/`) has a `Makefile.incl`
that sets `BINDIR`; the shared `share/mk/ubix.musl*.prog.mk` emits the binary to
`${OBJ_DIR}/${BINDIR}/` and `tools/mkimage.sh` stages it at the same path. The
top `Makefile`'s `world` target descends all the trees (Step 3 + 3b).

## Why

uBixOS historically homes *every* binary in a flat `/bin` — user commands and
system daemons alike (`/bin/authd`, `/bin/netcfg`, `/bin/views`, `/bin/ls`, …).
That was fine for bring-up but isn't how a POSIX-like system in 2026 is
organised, and it makes "what is a daemon vs. a user command" invisible. As we
add server software (sshd, …) it's time to start homing things appropriately.

## Storage direction (where the hierarchy physically lives)

**The long-term target: only the kernel stays in FAT; everything else lives in
the UbixFS pool.**

- **Now (transitional):** the kernel boots from a FAT `/boot` (GRUB/loader reads
  FAT). i386 already homes the *world* in the UbixFS pool (FAT carries only
  boot); aarch64/x86_64 still stage the world onto the FAT image via
  `mkimage.sh` while the pool-as-root path is finished.
- **Next:** once **GRUB can read the UbixFS pool**, the FAT boot partition goes
  away entirely — kernel + world all live in the pool.
- **After that:** FAT support is retained **only** for mounting *removable* media
  (external drives, USB sticks) — never as a system root.

This matters for the hierarchy: the FS that holds `/bin`, `/usr`, … is becoming
the **UbixFS pool, which *does* support symlinks** (`ubfs_fs_symlink` /
`ubfs_fs_readlink` are wired). So the "no symlinks" limitation below is a
property of the *transitional FAT staging*, not of the destination — a symlink
usrmerge (Phase 3) becomes viable as soon as the pool is the universal root.

## The constraints that decide the shape

1. **The transitional FAT staging cannot store symlinks.** The genuine 2026
   distro default is *usrmerge* (`/bin` → `/usr/bin` symlinks). While the
   aarch64/x86_64 world is still staged onto a FAT image, a symlink-based
   usrmerge can't be represented. → We use **real directories** now (they work on
   both FAT and the pool), and revisit usrmerge once the pool is the root
   everywhere (see *Storage direction* above).
2. **`/lib` is pinned by `PT_INTERP`.** Every musl PIE records
   `/lib/ld-musl-<arch>.so.1` as its interpreter at link time, so `/lib` must
   stay exactly where it is. `/usr/lib` is for *additional* (non-essential)
   shared objects only.
3. **Hardcoded early paths.** The kernel execs `/bin/init`; bmake and shebangs
   exec `/bin/sh`. So `/bin` keeps the essentials (`sh`, `init`, core commands)
   — it does not disappear.

## Target layout

| Path        | Contents |
|-------------|----------|
| `/lib`      | `ld-musl-<arch>.so.1` + essential shared libs (libc, libubix_api, libpw, libbearssl) — **fixed by PT_INTERP** |
| `/bin`      | Essential user commands needed early / by the kernel + bmake: `sh`, `init`, `ls`, core tools |
| `/sbin`     | Boot/system-critical admin tools |
| `/usr/bin`  | The bulk of user commands |
| `/usr/sbin` | System daemons + admin tools: **dropbear**, authd, netcfg, logd, ubistry, automountd, aural, views |
| `/usr/lib`  | Non-essential shared libs |
| `/etc`      | System config |
| `/usr/include`, `/usr/src` | Headers + source tree (self-host) — already present |

`PATH` is already `"/bin:/sbin:/usr/bin:/usr/sbin"` in init's `envp_login`
(`bin/init/main.c`), so the search order is in place — no change needed there.

## Build staging

The world build stages into an FS-shaped tree under `build/${ARCH}/`:
`build/${ARCH}/{bin,lib,libexec}` today, now joined by
`build/${ARCH}/{sbin,usr/bin,usr/sbin,usr/lib}` as things migrate. The image
builder (`tools/mkimage.sh`, used by **both** aarch64 and x86_64) mirrors the
staging tree into the FAT image verbatim. The dropbear port links straight into
`build/${ARCH}/usr/sbin/`.

## Migration plan (incremental)

- **Phase 0 (this pass):** establish the directories + staging convention;
  `mkimage.sh` creates `/sbin /usr /usr/bin /usr/sbin /usr/lib` and copies the
  new staging dirs. **dropbear/dropbearkey → `/usr/sbin`** as the first citizens.
- **Phase 1:** migrate the existing daemons (`authd`, `netcfg`, `logd`,
  `ubistry`, `automountd`, `aural`) to `/usr/sbin`, updating their `etc/init.d/*`
  units. One daemon (or a small batch) per change, kept reviewable.
- **Phase 2:** split the bulk of user commands `/bin` → `/usr/bin`, keeping only
  the early-essential set in `/bin`.
- **Phase 3 (later):** once every root is UbixFS, optionally flip to symlink
  usrmerge (`/bin`→`/usr/bin`, `/sbin`→`/usr/sbin`) to match modern distros.

## Notes

- **i386 (`tools/mkimage.sh`) is out of scope** — frozen on `releng/2`; it uses a
  separate STAGE→UbixFS-pool flow. Only `mkimage.sh` is updated here.
- Nothing in `/bin` is *removed* in Phase 0 — purely additive, so the change is
  safe to land before the daemons migrate.
