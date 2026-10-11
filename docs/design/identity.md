# uBixOS Identity — what makes it different

> **Status: PROPOSAL / direction document.** Nothing here is decided, scheduled
> or promised. It records ideas now so each can be discussed in detail later;
> each idea would get its own design plan before any code. Statements about
> "what exists today" were checked against the tree on 2026-10-10; statements
> about the future are proposals.

## Purpose and non-goals

uBixOS is a POSIX-like system, and there are many of those. This document asks
what should make it *worth choosing* over them, and records five candidate
answers tied to code that already exists.

Non-goals: this is not an implementation plan, not a schedule, and not a
commitment. It does **not** propose cloning Linux (APIs, ABI, cgroups,
systemd-style init, procfs/sysfs layout) — Linux is not a model here. Where
prior art is cited it is BeOS/Haiku, QNX, Plan 9, macOS and FreeBSD.

## Premise

- A **native uBix ABI/API** is the core (the native syscall table in
  `sys/kern/syscalls.c`; `lib/ubix_api/`, `include/ubistry/`).
- A **FreeBSD-numbered POSIX layer** (`sys/posix/syscalls_posix.c`, musl) exists
  so ports just work. It is a personality, not the identity — the same split
  `docs/design/mpi-modernization-plan.md` ("Identity" section) draws between
  Mach/XPC and the BSD layer in macOS.
- **Console-first, graphical-optional, one tree, profile-driven, MMU-class
  only** (`docs/design/console-and-arch-convergence-plan.md`, "Product identity").
- The **views compositor + objGFX** stack (`usr.sbin/views`, `lib/objgfx`) is
  already a differentiator; these ideas are about the *rest* of the system.

Two rules for every idea below: (1) the native API is the first-class surface;
(2) POSIX software must keep working unmodified, with the native feature
reachable through the POSIX layer only where a natural mapping exists.

---

## 1. A filesystem with indexed attributes and live queries

**Idea.** Files carry typed, named attributes (`Artist`, `From`, `Keywords`,
`Type`), selected attributes are indexed per dataset, and a *query* is a
first-class object: run once, or kept **live** so the result set updates as
files change. A mail client, media library or launcher becomes a query over the
filesystem rather than a private database.

**Exists today.**
- The pooled CoW filesystem: `include/fs/ubixfs/ondisk.h` (frozen format),
  `lib/ubixfs_core/` (`ubfs_dmu.c`, `ubfs_dsl.c`, `ubfs_fs.c`, `ubfs_dir.c`),
  kernel driver `sys/fs/ubixfs/ubfs_vfs.c`, admin tools `sbin/ubpool`,
  `sbin/ubfs` (native syscall 67 via `lib/ubix_api/ubfspool.c`). Status in
  `docs/design/ubixfs-pool-plan.md`.
- The inode reserves an xattr slot: `ubfs_inode_t.xattr` ("xattr-dir object id,
  0 = none (ACLs ride here later)", `ondisk.h:197`).
- `docs/design/ubixfs2-plan.md` is the earlier **BeFS-style** design (B+tree
  directories, inline small-data, attributes extent). It is **superseded** by
  the pool plan, but it records the original intent.

**Gap.** xattrs are reserved, not implemented. No attribute index structure, no
query language/engine, no change feed out of the VFS (needed for live queries),
no native API for any of it. Directories are linear/micro in v1.

**Coexistence with POSIX.** Attributes surface as POSIX-style xattrs
(`extattr_*` in the FreeBSD ABI) so ports can read and write them; indexing and
queries are native-only. Plain files with no attributes behave exactly as today.
FAT roots stay attribute-less.

**Prior art.** BeFS/Haiku (attributes + indices + live queries — the model to
study, including its mistakes: unbounded index growth, attribute loss crossing
foreign filesystems); macOS Spotlight/`mdfind` (index as a separate daemon,
the contrasting design); FreeBSD `extattr` and ZFS xattr dirs (the storage
layer we already resemble); Plan 9 (everything-is-a-file as the opposite bet).

**Risks / open questions.**
- Index inside the CoW pool (consistent, transactional, costs write
  amplification) vs. an index daemon fed by a change feed (Spotlight-style,
  eventually consistent)?
- Attributes are lost when files cross FAT, tar, or the network. Acceptable?
- Which attributes are indexed is per-dataset policy — who sets it?
- Query visibility must obey permissions (`multiuser-security-plan.md`); an
  index must never leak names a user cannot see.

**Rough first step.** Implement xattrs behind the reserved inode slot with a
host-side test in the existing `bmake test` harness; no index, no queries yet.
Then prototype a *non-live* indexed-attribute query in `sbin/ubfs` to see if
the format needs a hook before it is too late to change it.

---

## 2. A message-oriented system: typed service protocols over MPI

**Idea.** System services are reached by sending typed messages to named
mailboxes, with the protocol (message ids, payload structs, error contract)
published as a header — QNX/Plan 9 in spirit. A service *is* its protocol;
the native API is a thin, typed veneer over it, and POSIX is a personality
layered on top (a daemon may expose the same service through files or sockets
for ports).

**Exists today.**
- MPI: `sys/mpi/{system,message,mpi_syscalls}.c`; native syscalls 50–53
  (create/destroy/post/fetch) plus a blocking, timed wait at 69
  (`sys/kern/syscalls.c`); userland stubs `lib/ubix_api/mpi_*.S`.
- Request/reply helper with correlation in `lib/ubix_api/mpi_rpc.c`.
- Native services already speak it: `usr.sbin/ubistry`, `authd`, `views`
  (`include/views/display_proto.h`), `logd`, `init`.
- Direction and Mach/XPC analogy: `docs/design/mpi-modernization-plan.md`
  (mailbox-as-descriptor, blocking receive, variable payload, bounded queues,
  request/reply ids, reserved names). **Most phases are not started.**

**Gap.** Small fixed payloads with per-service truncation hacks
(`UB_NAMES_MAX`); mailboxes are not fds so they cannot join `poll()`; no
credentials on post (`multiuser-security-plan.md`); no machine-readable
protocol description — each service's contract is a hand-kept header. No notion
of a protocol *version* or a discovery step.

**Coexistence with POSIX.** MPI is **not** replaced by `AF_UNIX`; a future
`AF_UNIX` is a compat item beside it. Ported apps reach native services through
libraries that hide MPI (the `lib/ubix_api/ubistry.c` pattern). Mailbox-as-fd
(plan Phase 1) lets MPI sit in the same `poll()` loop as sockets without making
it a socket.

**Prior art.** QNX (message-passing as the OS: send/receive/reply, resource
managers); Plan 9 (9P: one protocol, per-process namespaces); Mach/XPC and
Haiku's BMessage/`BLooper` (typed messages as the app-level model);
FreeBSD (contrast: Capsicum capabilities; no native message bus).

**Risks / open questions.**
- Name-addressed vs capability-addressed endpoints: the plan keeps names; is
  that enough once untrusted apps exist?
- IDL or hand-written headers? Stub generators are heavy; a plain declarative
  table was the stated preference elsewhere.
- Synchronous-call priority inheritance (QNX) matters only if scheduling
  becomes realtime-ish; likely out of scope.

**Rough first step.** Finish MPI Phases 1 and 4 (variable payload + blocking
receive; request/reply with correlation ids), then write *one* service
protocol — ubistry's — as the reference "typed protocol" header others copy.

---

## 3. ubistry as the single structured configuration store

**Idea.** One daemon owns all structured settings: a typed, hierarchical,
layered (system / per-user) tree with schemas and **change notifications**.
GUI Settings, console tools and services all read and write the same keys and
react live. `/etc` shrinks to what POSIX ports require, generated or backed by
ubistry where possible.

**Exists today.**
- Daemon `usr.sbin/ubistry/{main,db,persist,message}.c`, client veneer
  `lib/ubix_api/ubistry.c`, contract `include/ubistry/ubistry.h`; text
  persistence at `/var/db/ubistry.db`; user-over-system layering by convention
  (`ubistry_get_for`/`set_user`).
- Consumers: `usr.sbin/netcfg`, `usr.sbin/views`, `usr.sbin/aural`,
  `usr.sbin/vlogin`, `usr.bin/settings`, `bin/init`.
- Remaining `/etc` content: `etc/{passwd,group,fstab,resolv.conf,motd,termcap,
  shells,...}` and `etc/init.d/*`.
- The plan (`docs/design/ubistry-plan.md`): acknowledged writes, WATCH/CHANGED,
  credential-derived user layer, schemas, list type, revisions.
  **None of Phases 0–4 are done.**

**Gap.** No notification, so settings apply at boot rather than live; writes are
fire-and-forget; user layer is trusted not enforced; no schema; no list type.
No story yet for which `/etc` files migrate and which stay as files.

**Coexistence with POSIX.** POSIX tools read files. Options: ubistry *renders*
`/etc` files from the tree (one-way, generated), or specific keys are mirrored
into legacy files by a small projector, or ports are patched to the client
library. Never require ports to speak MPI.

**Prior art.** GNOME dconf/GSettings (the model the plan already adopts); macOS
`defaults`/CFPreferences + `launchd` plists; Windows Registry (cautionary:
binary hive, no ownership); FreeBSD `rc.conf` + `sysctl` (what the "single
store" replaces, and a reminder that simple text wins for recovery).

**Risks / open questions.**
- A single daemon is a single point of failure for boot: what is the minimum
  config the system needs when ubistry is down (console, fstab, root login)?
- Hand-editable text vs. a tool-only store: recovery on a headless board needs
  the former. Keep persistence text-based (as the plan says).
- Do `passwd`/`group`/`fstab` move? Probably not; authentication data has
  different recovery requirements.

**Rough first step.** Deliver Phase 0+1 of the existing plan (acknowledged
writes, WATCH/CHANGED) and convert one live consumer (`netcfg`) from boot-time
applier to watcher, as the proof the model works.

---

## 4. One tree, many profiles, self-hosting, atomic and rollbackable updates

**Idea.** The same tree builds console, headless-IoT and desktop products
(`PROFILE`), the system can rebuild itself on the device, and a system update
is a **snapshot-backed atomic switch**: take a snapshot, install into a clone,
flip the boot dataset, keep the old one for rollback.

**Exists today.**
- Profiles in the image builder: `tools/mkimage.sh` (`PROFILE=desktop|base`,
  which skips graphical binaries and assets); design in
  `docs/design/console-and-arch-convergence-plan.md` Phase 4.
- Self-hosting: `docs/design/self-hosting-plan.md` (Clang/lld on-device) and
  `docs/design/on-device-build-plan.md` (native branch in `share/mk` still to
  do; single-program compile+link works on-device).
- Pool root: a pool can be `/` (`ubixfs-pool-plan.md`, K5/M3–M4) with a FAT
  `/boot`.
- CoW + checksums + uberblock/txg are done; **snapshots are deferred but their
  three format hooks are in place** (`birth_txg`, uberblock→MOS→dataset→objset
  indirection, single free chokepoint — `ubixfs-pool-plan.md`).

**Gap.** Snapshots/clones not implemented; no "boot dataset" selector in the
bootloader/kernel; `/boot` on FAT is outside the pool so kernel updates are not
atomic with the world; no update tool or manifest; profiles are a build-time
switch, not a runtime concept; self-hosting is incomplete.

**Coexistence with POSIX.** Entirely below POSIX: a rollback changes which
dataset is `/`, not how files behave. A POSIX app sees a normal tree.

**Prior art.** FreeBSD boot environments (`bectl`/`beadm`, ZFS clone + `bootfs`
— the closest match); macOS sealed system volume + APFS snapshots; Haiku's
package-management with boot-time activation states (`packagefs`, states
directory); NixOS generations (concept only).

**Risks / open questions.**
- Kernel and `/boot` on FAT break atomicity: move `/boot` into the pool (needs
  the deferred GRUB/ubfs reader) or keep two kernels on FAT with a flag?
- Which data is *state* (survives rollback: `/home`, ubistry user layer) vs
  *system* (rolled back)? Datasets make this natural; the layout needs design
  (`filesystem-hierarchy-plan.md`).
- Profiles: purely image-time, or runtime-selectable per boot?

**Rough first step.** Implement read-only snapshots at the dataset-pointer
level (the hooks exist), exposed through `ubfs snapshot`/`rollback`, and test
that a rollback of a populated dataset on the QEMU image boots.

---

## 5. Native apps get more; POSIX ports still just work

**Idea.** The uBix API is where the system's strengths surface as one coherent
toolkit: a native app can run live queries (idea 1), talk to services by typed
messages (2), observe settings changes (3), and integrate with views (window
state, notifications, drag/drop, sharing). A POSIX port gets a working program;
a native app gets a *better-integrated* one.

**Exists today.**
- Native app API: `lib/ubix_api/{views,display,ubistry,mpi_rpc,ulog,...}.c`;
  app/GUI docs `docs/apps/` (writing-a-views-app, objgfx-reference,
  views-protocol); `lib/objgfx`, `include/views/display_proto.h`.
- POSIX ports: musl + FreeBSD numbering (`docs/standards/abi-and-syscalls.md`),
  NetSurf, tcsh, vDoom.

**Gap.** No notification/toast service, no query API, no uniform event loop
(`views` apps poll MPI; mailbox-as-fd would unify), no documented stable
native-API layering and versioning policy, no "what a native app gets" list.

**Coexistence with POSIX.** Strict: nothing in a native feature may be required
by a port, and no POSIX behaviour may change to enable one. Thin optional
libraries bridge: e.g. a port can link `libubix_api` for a tray icon or a
watch on a setting without being rewritten.

**Prior art.** BeOS/Haiku (BApplication/BLooper/BMessage/BQuery — the closest
fit for *integrated* native apps); macOS (Cocoa/AppKit over BSD, Apple's
steering of apps off raw POSIX IPC); QNX Photon/Neutrino (native GUI over a
POSIX layer); FreeBSD (contrast: no native layer, ports only).

**Risks / open questions.**
- Two APIs doubles the documentation and test burden; the native API needs a
  stability promise to be worth targeting.
- Ports that prefer the native API fork their upstream; keep integration
  optional and out-of-tree patches small.

**Rough first step.** Write the "native app capabilities" list as a reference
page in `docs/apps/` once ideas 2 and 3 land, and pick one ported app as a
showcase for adding optional native integration.

---

## How the ideas reinforce each other

- **2 is the substrate for 3 and 5.** ubistry notification and the native
  event loop are MPI features; typed protocols give both a shared vocabulary.
- **1 + 2 give live queries a transport.** The filesystem emits a change feed;
  MPI delivers it; apps subscribe like they subscribe to ubistry keys (3).
- **3 + 4 make profiles real.** A profile becomes a set of datasets plus a
  ubistry default tree; a rollback restores both the system and its config
  schema together, while user layers live in a persistent dataset.
- **4 depends on the pool (1's storage).** Snapshots and attributes share the
  CoW pool; per-dataset attribute indexes are copied or rebuilt on clone.
- **5 is the visible payoff.** The more of 1–3 are native-API-reachable, the
  clearer the reason to write a native app rather than a port.
- All five stay **optional beneath POSIX**, protecting ports and keeping the
  console-first base small: no idea requires a display.

## Open questions for discussion

1. Which idea is the primary identity bet, and which are supporting cast?
   Doing all five fully is probably more than one maintainer can carry.
2. Is the BeFS-style attribute index worth its cost on the pool, or should
   queries be a separate indexing service from the start?
3. Name-addressed MPI endpoints versus capabilities: where is the line for
   untrusted native apps?
4. How much of `/etc` should ubistry own; what is the minimum boot config that
   must stay as plain files?
5. Boot environments: move `/boot` into the pool (and pay for a GRUB/loader
   reader), or accept a two-slot FAT `/boot`?
6. What is the stability/versioning promise for the native API, and from
   which release?
7. How is "one tree, many profiles" tested — what is the minimum matrix
   (arch × profile) that CI must keep green?
8. Is the MMU-class-only scope a hard boundary for these ideas, or can
   low-memory boards (<256 MB) drop idea 1's indexes and idea 4's snapshots?
9. Which ideas should be explicitly *rejected*, so the project does not drift
   toward another Unix clone or toward an everything-bus?
