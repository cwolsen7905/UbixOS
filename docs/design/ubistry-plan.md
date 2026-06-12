# uBixOS ubistry Modernization Plan

> `ubistry` (`bin/ubistry/`, client lib `lib/ubix_api/ubistry.c`, contract
> `include/ubistry/ubistry.h`) is uBixOS's **registry**: a hierarchical,
> path-addressed tree of typed nodes served by one daemon over MPI and persisted
> as text to `/var/db/ubistry.db`. It already *is* a registry in the Windows /
> dconf sense — typed values, hierarchical paths, a system/user layer. What it
> lacks are the four things every mature settings store grew into: **change
> notification**, **acknowledged writes**, **enforced** (not by-convention)
> per-user isolation, and a **schema** that validates what goes in. This plan
> adds them, proportionate to a console-first hobby OS, and in dependency order
> with the two plans ubistry sits between.
>
> **Companion plans.** ubistry is a *native service* (see the identity section of
> `mpi-modernization-plan.md`) and stays on MPI. It therefore inherits MPI's
> roadmap and the security model wholesale rather than reinventing either:
> - `mpi-modernization-plan.md` — Phase 1 (blocking receive + variable-length
>   payload) and Phase 4 (first-class request/reply + correlation IDs) are hard
>   dependencies for ubistry's notification and acknowledged-write work. ubistry
>   is named there as a Phase-2 consumer whose `UB_NAMES_MAX` truncation hack
>   goes away.
> - `multiuser-security-plan.md` — owns `struct ucred` and the one MI access
>   chokepoint. ubistry's "enforce the layering" work is a *consumer* of that
>   credential, not a second copy of it.
>
> Cross-arch by construction: ubistry is pure userland (a `bin/` daemon + a
> `lib/` veneer) talking MPI, so nothing in this plan touches `sys/arch/`. Both
> architectures stay green throughout.

## North Star

A setting is a **typed key with a declared contract**, not a free-form string at
a free-form path. Writing one is **acknowledged** — a failed write (full tree,
bad path, denied by policy, schema violation) returns an error instead of
vanishing into a fire-and-forget post. Reading one is **layered** — the caller's
*own* credential selects the user layer; a process cannot read or write another
user's overrides, and that is *enforced*, not trusted. A running program can
**subscribe** to a subtree and be told the moment it changes, so config edits
take effect live instead of at next boot. Persistence stays **human-readable
text** — the one thing the Windows binary hive got wrong and we will not copy.
Same daemon, same MPI, both arches.

## Identity: ubistry is the registry, not a config-file pile

uBixOS deliberately has a registry rather than scattering `*.conf` files across
`/etc`. The reasons are the same ones Windows and GNOME landed on, and stating
them keeps later work from drifting back toward "just write a dotfile":

1. **One namespace, one daemon, one format.** Every subsystem's settings are
   reachable by path through one API. No per-app parser, no per-app file format,
   no "where does *this* program keep its config" scavenger hunt.
2. **Layering is a first-class property of the store**, not something each app
   re-implements by reading two files. `ubistry_get_for()` already resolves
   user-over-system in *one* place; that is the whole point.
3. **A single chokepoint for notification, validation, and access control.**
   You cannot put a `chmod`-and-`inotify`-and-schema story on a pile of ad-hoc
   files without rebuilding most of a registry anyway. Having the daemon means
   those features have *somewhere to live*.

What we are **not** copying from Windows: a binary hive (text is more debuggable
and survives a half-written flush better), an unbounded global dumping ground
with no ownership (schemas give keys owners), and "any app writes anywhere"
(the security phase ends that). The model we *are* copying is **GNOME
dconf/GSettings**: a dconf-style storage tree (what ubistry is today) under a
GSettings-style typed-schema layer with a `changed` signal (what this plan
adds).

## Status Matrix

Legend: ✅ done & verified · 🟡 partial · ⬜ not started

| Phase | Item | Status | Notes |
|-------|------|--------|-------|
| — | Hierarchical typed tree, text persistence, MPI GET/SET/ENUM/DEL | ✅ | as-built in `bin/ubistry/` |
| — | System/user layering (`get_for`/`set_user`, `DISPLAY_SET_USER`) | ✅ | by convention at `/users/<name>/<key>` |
| 0 | Acknowledged `SET`/`DEL` (reply with `{ok, err}`) | ⬜ | needs MPI Phase 1 blocking receive to be cheap |
| 1 | **Change notification**: `WATCH`/`UNWATCH`/`CHANGED` on a subtree | ⬜ | the load-bearing feature; needs MPI Phase 1 |
| 1 | Convert `netcfg`/`sndcfg` from boot-appliers to live watchers | ⬜ | first consumers of notification |
| 2 | Credential-derived user layer (stop trusting client-supplied `/users/<name>`) | ⬜ | **delegated** enforcement to `multiuser-security-plan.md` |
| 2 | System-default writes gated on `uid 0` | ⬜ | same dependency |
| 3 | Schema layer: declared `{type, default, range/enum}` per key; reject violations | ⬜ | the dconf/GSettings lesson |
| 3 | Retire type-inference-on-read (persist explicit types) | ⬜ | rides the schema work |
| 4 | List value type (`UB_STRLIST`); bump value cap | ⬜ | kills the `/0 /1 /2` numbered-container hack |
| 4 | Monotonic revision counter in replies | ⬜ | cheap cache-validation; etcd's trick |

---

## Prior Art (and what we deliberately do *not* copy)

| Property | Windows Registry | macOS `defaults`/CFPreferences | GNOME dconf/GSettings | ubistry today | This plan |
|----------|------------------|--------------------------------|------------------------|---------------|-----------|
| Structure | Hives → keys → values | Per-app plist domains | One path tree | One path tree | **Keep the tree** |
| Value types | SZ/DWORD/QWORD/BINARY/MULTI_SZ/EXPAND_SZ | plist types (incl. arrays) | Full GVariant (incl. lists, tuples, enums) | STR/INT/BOOL only | **Add list type** (Phase 4); GVariant is overkill |
| Persistence | Binary hive | Text/binary plist | Compact binary db | **Text file** | **Keep text** — more debuggable |
| Layering | HKCU over HKLM (separate hive files) | `NSUserDefaults` search list | user db over system db + admin profiles | `/users/<n>/` over `/<key>` (convention) | **Enforce** via `ucred` (Phase 2) |
| Validation | None (any garbage anywhere) | None | **Schema: type + default + range + enum** | None (type *inferred*) | **Schema layer** (Phase 3) |
| Lockdown | Per-key ACL | Managed prefs (MDM) | Per-key `lockdown` | None | Out of scope (note below) |
| Change notify | `RegNotifyChangeKeyValue` | KVO / distributed notifications | `"changed"` signal | **None** | **WATCH/CHANGED** (Phase 1) |
| Write result | Returns status | Returns status | Returns status | **Fire-and-forget** | **Acknowledged** (Phase 0) |
| Revisions | — | — | — | None | **Monotonic counter** (Phase 4) |

**Explicitly out of scope** (the heavyweight half we are *not* building):
per-key ACLs / lockdown (Phase 2's user-vs-system + root-for-defaults split is
the proportionate substitute — uBixOS does not need MDM-grade managed-policy);
transactions / multi-key atomic commit (Windows added it late, almost nobody
uses it; a single `SET` is already atomic); GVariant's full type algebra (a
string-list covers the one real gap); networked / remote registry; and a
mandatory IDL for the schema (a plain declarative table is enough). Service
**supervision** of the daemon is `init`'s job, not ubistry's.

---

## Phase 0 — Acknowledged writes (smallest, unblocks the rest)

Today `UB_MSG_SET` and `UB_MSG_DEL` are fire-and-forget: a write that fails for
*any* reason (out of memory in the daemon, malformed path, and — after Phases
2–3 — a denied or schema-invalid write) is silently lost. The caller's
`ubistry_set_str()` returns 0 having only confirmed the *post* succeeded, never
that the *write* did.

- Add an ack reply `UB_MSG_ACK { int32_t ok; int32_t err; }` (header `0x107`).
  `SET`/`DEL` requests carry a `reply_mbox` (like GET/ENUM already do); the
  daemon replies after applying the write.
- `ubistry_set_*`/`ubistry_del` block for the ack and surface `err` to the
  caller. A NULL/empty `reply_mbox` keeps the old fire-and-forget behaviour for
  callers that genuinely do not care (boot-time seeding).
- **Dependency:** this only becomes *cheap* once MPI Phase 1 gives a real
  blocking receive — until then the client would spin-wait the ack the same way
  GET does today (200k yields). It is correct either way; it is *efficient* only
  after MPI Phase 1. Land the protocol now, let it inherit the idle win.
- **Test:** a `SET` to a path the daemon rejects returns non-zero to the caller;
  a normal `SET` round-trips and the value reads back. Both arches boot.
- **Risk:** low — additive opcode; old fire-and-forget path stays for seeders.

## Phase 1 — Change notification (the load-bearing feature)

The single biggest capability gap, and the reason `netcfg`/`sndcfg` are
*boot-time appliers* rather than live daemons: nothing in uBixOS can react to a
setting change except by polling or restarting.

- **`UB_MSG_WATCH { char reply_mbox[]; char path[]; }`** (header `0x108`): the
  daemon records `(reply_mbox, path-prefix)` in a watcher table.
- On any `SET`/`DEL` whose path is at or under a watched prefix, the daemon posts
  **`UB_MSG_CHANGED { uint8_t op; char path[]; uint8_t type; char value[]; }`**
  (header `0x109`) to each matching watcher. `op` is set/deleted; the new value
  rides along so a watcher needs no follow-up GET for the common case.
- **`UB_MSG_UNWATCH`** (header `0x10A`) drops a registration; watcher entries are
  also reaped when the watcher's mailbox disappears (the daemon already learns
  this when a post to a dead mailbox fails — tie reaping to that).
- Client API: `int ubistry_watch(const char *path, void (*cb)(const char *path,
  ...))` plus an integration point for the caller's event loop. The veneer owns
  the watch mailbox; this is the same "MPI stays invisible to the app" pattern
  the rest of `lib/ubix_api/ubistry.c` follows.
- **First consumers:** convert `bin/netcfg` and `bin/sndcfg` from one-shot
  appliers (run once by `init.d`, re-run by Settings) into small resident
  daemons that watch `/net/*` and `/aural/*` and apply on change. `views` can
  watch `/users/<me>/views/*` and re-theme live (wallpaper changes without a
  relog — the dconf `changed`-signal experience).
- **Dependency:** the watcher daemons should **block** on their watch mailbox,
  which is MPI Phase 1's blocking receive. Before that they would busy-poll;
  acceptable as a bridge but the point is the idle-when-quiescent win.
- **Interaction with the security phase:** a watch is a *read* — Phase 2's
  layering means a watcher only sees `CHANGED` for paths it would be allowed to
  GET (its own user layer + system), never another user's overrides.
- **Test:** edit `/aural/volume` in Settings; the running audio path reflects the
  new master volume with no restart. A watcher on `/views/desktop` is notified
  on wallpaper change and *not* notified on an unrelated `/net` write. Both
  arches.
- **Risk:** medium — first stateful per-client registration in the daemon;
  watcher-table lifetime (reaping dead watchers) is the thing to get right.

## Phase 2 — Enforce the layering (delegated credential, local policy)

Today the user layer is **convention**: `ubistry_set_user("alice", …)` writes
`/users/alice/…`, but nothing stops *any* process from writing
`/users/alice/…` directly or stomping a system default. That is fine while the
system is single-user-at-a-time; it is not acceptable once
`multiuser-security-plan.md` lands real users.

- The daemon derives the user from the **caller's `ucred`** (delivered on the
  MPI post per the security plan / MPI credential metadata), **not** from a
  client-supplied `/users/<name>/` path. `ubistry_set_user(user, …)` becomes a
  convenience that must agree with the caller's identity; a mismatch is denied.
- **Read resolution** uses the caller's own user for the `/users/<me>/` layer,
  then falls back to system. A process cannot read another user's overrides.
- **System-default writes** (bare `/<key>`) require `uid 0`. Unprivileged
  writes are confined to the caller's own `/users/<me>/` subtree.
- This is the HKCU-vs-HKLM split made enforceable, and it is the *same* shape as
  the VFS access chokepoint in the security plan — one place, one credential
  check, applied to the registry.
- **Delegation boundary:** ubistry does **not** define `ucred` or the credential
  wire format — it *consumes* what the security plan + MPI metadata provide.
  Until that exists, this phase is design-complete but not implementable; the
  Phase-0 ack reply is what lets a denied write surface as an error.
- **Test:** a non-root process cannot write a bare system key (gets the Phase-0
  error); user A cannot read or write user B's overrides; `views` running as a
  user still resolves its own theme.
- **Risk:** low-medium on ubistry's side (mechanical once the credential
  arrives); the real work is the security plan's.

## Phase 3 — Schema layer (the dconf/GSettings lesson)

Right now any path accepts any value, and type is **inferred on read**
(`persist.c` `classify_value()`: quotes → string, `true/false` → bool, digits →
int). That is fragile two ways: a value like the string `"true"` vs the bool
`true` is ambiguous on round-trip, and nothing rejects `/aural/volume = 9999`.

- A **schema** declares each well-known key's contract: `type`, `default`, and an
  optional constraint (`int` range, `enum` of allowed strings). Modeled on
  GSettings schemas but kept to a plain declarative table — **no IDL/compiler**.
- Each owning subsystem registers its schema (or ships a schema fragment loaded
  at daemon start). A `SET` to a schema'd key is **validated**: wrong type or
  out-of-range → denied (surfaced via the Phase-0 ack). Unknown paths can stay
  permissive (registry as scratch space) or be rejected under a strict flag —
  start permissive.
- **Persist explicit types** instead of inferring them, removing the
  `"true"`-the-string ambiguity. The on-disk format stays human-readable text;
  it just records the type rather than guessing it.
- **Defaults from schema** mean a fresh system needs no seeded `ubistry.db` for
  schema'd keys — a missing key resolves to its declared default, the way
  GSettings ships defaults in the schema rather than the user db.
- **Fits the DTO discipline:** a schema entry *is* the typed contract for one
  key — validate at the trust boundary (the daemon), encapsulate the invariant
  (the range/enum) where it lives. This is the SOLID-correct home for the
  per-key invariants currently spread across every client's assumptions.
- **Test:** `SET /aural/volume = 9999` is rejected; a missing schema'd key reads
  its default; a string-valued `"true"` round-trips as a string, not a bool.
- **Risk:** medium — touches persistence format (migration: read old inferred
  format, write new typed format once) and adds the validation path.

## Phase 4 — Richer values + revisions (quality pass)

- **List type `UB_STRLIST`**: the start-menu entries are currently modeled as a
  numbered-integer container (`/views/startmenu/0/…`, `/1/…`) — Windows
  `REG_MULTI_SZ`'s exact use case. A first-class string-list type removes the
  hack. Bump `UB_VAL_MAX` past 120 bytes at the same time (MPI Phase 1's
  variable-length payload makes the old 248-byte-frame arithmetic moot).
- **Monotonic revision counter**: one global int, bumped per write, returned in
  every reply. A client can cache a value and later ask "still revision N?"
  cheaply — etcd's revision trick at single-node scale. Cheap, optional, enables
  smarter client caching without diffing.
- **Test:** start-menu round-trips as one list value; a client detects staleness
  via the revision without re-reading every key.
- **Risk:** low — both additive.

---

## Sequencing summary

Phase 0 (acknowledged writes) is the smallest change and is the prerequisite for
*every* later phase that can deny a write (2 and 3) — land it first, even though
it is only *efficient* after MPI Phase 1. Phase 1 (notification) is the
headline feature and the one users feel; it shares MPI Phase 1's blocking-receive
dependency and converts `netcfg`/`sndcfg` into live daemons. Phase 2
(enforcement) is gated on `multiuser-security-plan.md` delivering `ucred` and is
design-complete until then. Phase 3 (schema) is independent of the security
work and can land any time after Phase 0 gives it a way to report rejections.
Phase 4 is pure quality and can slot in whenever.

Both architectures stay green after every phase — ubistry is userland-only, so
no `sys/arch/` tree is touched. The two hard external dependencies are both in
`mpi-modernization-plan.md` Phase 1 (blocking receive for Phases 0/1) and the
credential delivery in `multiuser-security-plan.md` (for Phase 2); neither blocks
starting the protocol/schema design now.
