# Multi-User Security Model Plan

> The uncovered gap surfaced by the 2026-06-10 "what's missing to be a complete
> OS?" review. uBixOS already *stores* identity (a `userdb`, per-process uid/gid,
> per-inode `mode/uid/gid`) but never *enforces* it: no VFS path checks
> ownership or mode bits, effective credentials are vestigial, and there is no
> `chmod`/`chown`/`umask`/`setuid`-on-exec. This plan turns the existing
> at-rest identity into a runtime-enforced multi-user model.
>
> Companion to `ubixfs-pool-plan.md` (the inode that carries `mode/uid/gid` is
> the on-disk source of truth) and `clang-selfhost-plan.md` (a self-hosting OS
> wants real `/usr` permissions). Cross-arch by construction — all of it lives in
> the generic credential + VFS layers; neither `sys/arch/` tree is touched.

## North Star

A process runs as a **credential**, every privileged object (file, directory,
device node) carries an **owner + mode**, and every access decision routes
through **one** machine-independent chokepoint that compares the two. `login`
authenticates against `/etc/userdb` and then *drops privilege* so the shell and
its children actually run as that user — a non-root user cannot read another
user's files, cannot write `/etc`, and cannot regain root except through a
`setuid` binary. Root (`uid 0`) bypasses the checks. Same kernel, both arches.

## Status Matrix

Legend: ✅ done & verified · 🟡 partial · ⬜ not started

| Phase | Item | Status | Notes |
|-------|------|--------|-------|
| — | uid/gid stored per-process | 🟡 | loose fields on `kTask_t` (`sched.h:77-79`); effective creds vestigial |
| — | owner/mode stored per-inode | ✅ | ubfs inode `mode/uid/gid/nlink` (`ondisk.h:181-185`) |
| — | `/etc/userdb` authentication | 🟡 | `login` checks it but never binds the uid to the process |
| 1 | `struct ucred` DTO replaces loose fields | ⬜ | |
| 1 | `getuid`/`geteuid`/`getgid`/`getegid` return correct field | ⬜ | `getEUID` returns `uid` today |
| 1 | cred copied on `fork`, preserved across `exec` | ⬜ | fold into `proc_fork_inherit_context()` |
| 2 | `chmod`/`fchmod`, `chown`/`fchown`/`lchown` | ⬜ | `chmod` is `sys_invalid` today (backlog Tier 1) |
| 2 | `umask`, `getgroups`/`setgroups` | ⬜ | |
| 2 | real POSIX `setuid`/`setgid`/`setresuid` semantics | ⬜ | replaces "set any if root" stubs |
| 2 | syscalls wired into both tables (FreeBSD ABI) | ⬜ | |
| 3 | `vfs_access(cred, attr, want)` MI chokepoint | ⬜ | the single access-decision function |
| 3 | enforce in `open`/`exec`/`unlink`/`rename`/`mkdir`/traversal | ⬜ | |
| 3 | FAT `getattr` synthesizes `root:wheel 0755`/`0644` | ⬜ | keeps FAT root usable |
| 3 | `security.enforce` sysctl flipped ON | ⬜ | default-off through Ph 1-2 |
| 4 | `login` drops privilege (`setgid`+`setuid` after auth) | ⬜ | what makes Ph 1-3 *mean* something |
| 4 | ELF loader honours `S_ISUID`/`S_ISGID` | ⬜ | the `su`/`passwd` mechanism |
| 4 | new objects stamped with cred uid/gid + `mode & ~umask` | ⬜ | |
| 5 | sticky bit, full supplementary groups, `wheel` `su` gate | ⬜ | deferred, hooked |
| 5 | ACLs/xattrs | ⬜ | ubfs inode reserves the xattr slot |

Both arches stay green every phase; enforcement is off (behaviour == today) until
Phase 3.

---

## Current state (honest inventory)

| Piece | State | Where |
|---|---|---|
| Per-process credentials | **Loose fields, partly vestigial** — `uid, gid` (u32) + `euid, suid, egid, sgid` (u16) scattered on `kTask_t`; effective creds unused (`sys_getEUID` returns `uid`, not `euid`) | `sys/include/ubixos/sched.h:77-79`, `sys/kern/access.c` |
| `setuid`/`setgid` syscalls | **Trivial** — "set any UID if you're 0", no POSIX saved-uid semantics | `sys/kern/access.c` (`sys_setUID`/`sys_setGID`) |
| Authentication | `login` checks `/etc/userdb`; **but the resulting uid is never bound to the process** (everything runs as the boot identity, effectively root) | `bin/login/`, `tools/userdb` |
| Per-inode owner/mode | **Stored, never read for access** — ubfs inode has `mode` (`S_IF*` \| `rwxrwxrwx`), `uid`, `gid`, `nlink` | `include/fs/ubixfs/ondisk.h:181-185` |
| VFS access checks | **Absent** — `open`/`exec`/`unlink`/`rename`/`mkdir` never consult mode or ownership | `sys/fs/vfs/`, `sys/posix/vfs_calls.c` |
| `chmod`/`chown`/`umask` | **Absent** — `chmod` is `sys_invalid` (already on backlog Tier 1); no `chown`, no `umask` | `sys/posix/`, `backlog-roadmap.md` |
| `setuid`-on-exec | **Absent** — the ELF loader ignores `S_ISUID`/`S_ISGID` | `sys/arch/*/...exec`, `sys/kern/elf64_load.c` |

**Bottom line:** identity exists at both ends — a `userdb` of users and an inode
that names an owner — but nothing in the running kernel ever compares a process's
identity against an object's. The model is ~40% present (the data), 0% enforced.

## Design — SOLID

The load-bearing decisions, in the project's idiom (typed DTOs at boundaries, a
single chokepoint per invariant, dependency-inverted FS access — cf. the ubixfs
"single block-free chokepoint"):

1. **`struct ucred` — a first-class credential DTO** (replaces the loose
   `kTask_t` fields). Real/effective/saved uid + gid, and a supplementary-group
   set. One typed object copied on `fork`, transformed on `exec`/`setuid`, and
   passed *by value of pointer* into the access check. This is the trust-boundary
   DTO the user's SOLID/DTO preference calls for — no more scattered `u_int16_t`s
   whose meaning drifts.

2. **`vfs_access(cred, attr, want)` — the single access chokepoint** (SRP). One
   MI function, the *only* place mode bits + ownership + the root override live.
   Every VFS entry point calls it; no driver re-implements it. Mirrors the
   pool's "one block-free chokepoint" discipline — when the policy changes
   (sticky bit, ACLs), exactly one function changes.

3. **Owner/mode reach the check via the existing `getattr` DTO** (DIP). The VFS
   already has a per-driver attribute path (ubfs `getattr` fills `mode/uid/gid`);
   `vfs_access` consumes that DTO and is therefore **FS-agnostic**. FAT (which
   has no permission model) synthesizes a fixed owner/mode in its `getattr` so
   the check is uniform.

4. **Default-permissive escape hatch during bring-up** (a `security.enforce`
   sysctl). Phases 1-2 land the data + syscalls with enforcement *off* (behaviour
   identical to today); Phase 3 flips it on. This keeps every phase bootable on
   both arches and avoids bricking a root-FS that lacks sane modes.

## Phases

Each phase ends bootable on **both** arches; enforcement stays off until Phase 3.

### Phase 1 — Credential consolidation (no enforcement)
- Introduce `struct ucred` (`sys/include/ubixos/ucred.h`); replace the loose
  `kTask_t` uid/gid/euid/... fields with one `struct ucred cred`.
- Fix the effective-credential bug: `getuid`/`geteuid`/`getgid`/`getegid` return
  the *right* field; the getters in `access.c` read `cred.cr_uid` / `cr_euid`.
- Credential lifecycle: copied on `fork` (fold into the existing
  `proc_fork_inherit_context()` helper), preserved across `exec`.
- **Behaviour-preserving** — values are identical to today, just typed and
  centralised. Verify both arches still boot to a shell.

### Phase 2 — The syscalls (still no enforcement)
- `chmod`/`fchmod`, `chown`/`fchown`/`lchown`, `umask`, `getgroups`/`setgroups`.
- Real POSIX `setuid`/`setgid`/`seteuid`/`setegid`/`setresuid` semantics
  (the saved-set-uid rules), replacing the "set any if root" stubs.
- Wire all into **both** syscall tables at the **FreeBSD ABI numbers** (the
  POSIX table per `feedback_freebsd_abi`; the native table where applicable).
- `chmod`/`chown` route through the VFS to the FS driver's attr-set op (ubfs
  writes the inode + commits a txg). FAT returns `EPERM`/no-op (no mode model).
- **Test:** `chmod 700 f; stat f` round-trips on a ubixfs pool; `umask` affects
  new-file mode; `setuid` honours saved-uid. Enforcement still off, so nothing
  is *blocked* yet — this is the mechanism, not the policy.

### Phase 3 — The access chokepoint (enforcement ON)
- Implement `vfs_access(cred, attr, want)` (the rwx-against-owner/group/other
  decision + `uid 0` bypass) as one MI function.
- Call it from every VFS entry: `open` (R/W/X by flags), `exec`, `unlink`,
  `rename`, `mkdir`, `rmdir`, `chdir`, directory traversal (X on each path
  component).
- FAT `getattr` synthesizes `root:wheel 0755` (dirs) / `0644` (files) so the
  FAT root stays usable; real enforcement bites on ubixfs + devfs.
- Flip `security.enforce` on. **Test:** a non-root shell cannot write
  `/etc/userdb`, cannot read a `0600` file owned by another uid, *can* traverse
  `0755` dirs; root bypasses all. Both arches.
- **Risk control:** land with the sysctl defaulting *off*, flip in a dedicated
  commit, keep a one-flag rollback.

### Phase 4 — Privilege drop + setuid-on-exec
- `login` calls `setgid`/`setuid` after authenticating, so the shell + all
  children run as the real user (the change that makes Phases 1-3 *mean*
  something).
- The ELF loader honours `S_ISUID`/`S_ISGID` on the executable: a `setuid root`
  binary runs with `cr_euid = 0` (the `su`/`passwd` mechanism).
- `create`/`mkdir` stamp new objects with `cred`'s uid/gid and `mode &
  ~umask`.
- **Test:** `whoami` reflects the logged-in user; a `setuid` helper elevates;
  new files are owned by the creator.

### Phase 5 — Deferred, hooked (do not foreclose)
Sticky bit on world-writable dirs (`/tmp`), full supplementary-group checks,
the `wheel`-group `su` gate, and **ACLs/xattrs** (the ubfs inode already
reserves the xattr slot per `ubixfs-pool-plan.md`). Capabilities/privilege
separation beyond uid 0 are out of scope. Keep `vfs_access` the single
chokepoint so each of these is an additive change to one function.

## Interlocks

- **ubixfs-pool** — the inode `mode/uid/gid` is the on-disk source of truth;
  this plan is most meaningful once the pool is a read-write root (K5/M3). Until
  then it enforces on whatever ubixfs/devfs mounts exist.
- **FAT root** — has no native permission model; while `/` is FAT, `vfs_access`
  runs against synthesized attrs (so it never *blocks* exec of the world). Real
  per-user enforcement on `/` arrives with the ubixfs root.
- **clang-selfhost** — self-hosted builds want a real `/usr` owned by root and a
  per-user `$HOME`; this plan is a soft prerequisite.
- **session-plan** — the lock screen / session id pairs naturally with a real
  per-user credential.

## Testing

- A non-root login cannot: write `/etc/userdb`, read another user's `0600`
  file, `chmod` a file it doesn't own.
- A non-root login can: read/traverse `0755`, write its own `$HOME`.
- `setuid root` binary elevates; `root` bypasses every check.
- Regression: enforcement-off boot is byte-for-byte today's behaviour; both
  arches reach a shell with enforcement on.

## Out of scope (initially)

Mandatory access control / SELinux-style labels, capabilities(7), per-process
namespaces, disk quotas, audit logging. These layer on after Phase 5.
