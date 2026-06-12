# UbixOS MPI Modernization Plan

> MPI (`sys/mpi/`) is uBixOS's primary in-kernel IPC: named mailboxes, FIFO
> queues, owner-only reads. The bugs the old architecture doc tracked are all
> fixed (see `docs/architecture/mpi.md`, rewritten June 2026 to match the code).
> What remains are **design** gaps, not bugs — and every one of them is
> something macOS (Mach) and Windows (ALPC) solved decades ago. This plan closes
> them in dependency order, proportionate to a console-first hobby OS: it adopts
> the *principles* (blocking receive, credentials, variable payloads, waitable
> endpoints) without the heavyweight machinery (capability transfer, IDL stub
> compilers, service supervision).
>
> Cross-arch by construction — MPI lives entirely in the MI `sys/mpi/` layer;
> the i386 and aarch64 syscall front-ends (`sys/kern/syscalls.c`,
> `sys/arch/aarch64/kern/syscall.c`) only dispatch into it. Companion to
> `multiuser-security-plan.md` (owns the credential + ACL work this plan defers
> to it), `smp-plan.md` (the true-spinlock/`preempt_count` work the MPI lock
> inherits), and `console-and-arch-convergence-plan.md` (owns the input
> subsystem the unified-wait phase depends on).

## North Star

A mailbox is a **descriptor** in the owning process's file-descriptor table.
Receiving **blocks** (or polls) like any other fd, so an idle daemon sleeps
instead of spinning and the SoC reaches true idle. Payloads are
**variable-length** with a hard cap, so control data is never silently
truncated and a 4-byte opcode costs 4 bytes. Queues are **bounded** with
backpressure. Request/reply is **first-class** with correlation IDs. Posting is
still **name-addressed** (the service-discovery property is the good part of
MPI and is kept), but who may post a *reserved* name is enforced by the
credential layer. Bulk data still travels through **shared memory**; MPI
carries the handle. Same kernel, both arches.

## Identity: MPI is native IPC; POSIX is a compatibility layer

The modernization items below were derived by comparing MPI to macOS (Mach) and
Windows (ALPC). It is worth stating *why* that comparison leads to "keep and
improve MPI" rather than "replace MPI with `AF_UNIX` sockets" — because the
fd-shaping in Phase 1 makes MPI look enough like a unix-domain datagram socket
to invite exactly that wrong conclusion.

**The XNU model is the one uBixOS follows.** macOS's kernel is Mach (native:
ports, tasks, VM) *plus* a BSD layer (POSIX syscalls, pipes, signals, unix
sockets). Apple's own services — launchd, WindowServer, the frameworks — run on
**Mach ports and XPC**, the *native* IPC, and Apple has invested *more* in that
native side over 20+ years (XPC arrived in 2011 and keeps growing), while
steering developers *away* from raw POSIX IPC for first-class apps. The POSIX
layer exists so the large body of Unix software *ports and runs* — it is a
**compatibility personality, not the OS's identity**. Apple did not abandon Mach
for POSIX; the opposite.

uBixOS makes the same split, and the modernization plan is consistent with it:

| uBixOS | macOS analogue | Role |
|--------|----------------|------|
| **MPI** (native IPC, `int 0x81`) | **Mach ports / XPC** | Native, identity-defining. Native services and native apps speak it. **Keep + modernize.** |
| POSIX layer (`int 0x80`, lwIP, any future `AF_UNIX`) | **BSD layer** | Compatibility personality for *ported* apps. Coexists with MPI; does **not** replace it. |

Consequences for this plan, made explicit so later work does not drift toward
"build another Unix":

1. **Native services stay on MPI.** `ubistry`, `authd`, `automountd`, `views`,
   `init` are native services and speak the native IPC. They are **not** migrated
   to sockets. (`AF_UNIX` does not even exist in the tree today — the only
   sockets are lwIP `AF_INET`.)
2. **A future `AF_UNIX`, if built, is a POSIX-compat item** for ported software,
   sitting *beside* MPI — never a replacement for it.
3. **Ported POSIX apps reach native services through a thin compat library**, the
   way macOS frameworks wrap Mach/XPC. `lib/ubix_api/ubistry.c` already *is* this
   pattern: the app sees a clean C API; MPI stays invisible to it.
4. **The modernization items make MPI a _better native IPC_, not a socket in
   disguise.** Each one mirrors a feature Mach already has — blocking receive
   (`mach_msg`), descriptor + `poll()` (ports are kqueue sources), variable-length
   payloads (OOL messages), credentials/post-ACL (send rights). Phase 1's
   fd-shaping is *fd-integration for native IPC*, exactly as Mach did it — not a
   step toward deleting MPI.

## Status Matrix

Legend: ✅ done & verified · 🟡 partial · ⬜ not started

| Phase | Item | Status | Notes |
|-------|------|--------|-------|
| — | All `BUG-MPI-01..07` fixed | ✅ | verified in `system.c`; arch doc rewritten |
| — | `mpi_destroyProcessMboxes` (no leak on exit) | ✅ | called from `endTask` |
| — | `mpi_mbox_exists` (boot-time readiness wait) | ✅ | |
| 0 | Remove dead `message.c` `sysMpi*` wrappers | ⬜ | zero callers; not in either syscall table |
| 0 | Document ubistry enum truncation as as-built | ✅ | added to arch-doc Current Limitations |
| 1 | **v2 core API**: mailbox-as-descriptor + blocking receive + variable-length payload (one API break) | ⬜ | the load-bearing phase |
| 1 | Mailbox lives in the process fd table; `close()` releases it | ⬜ | subsumes `destroyProcessMboxes` via normal fd cleanup |
| 1 | Hard payload cap (one page); above it → use shared memory | ⬜ | |
| 2 | Migrate all consumers to v2; drop compat shims | ⬜ | removes `UB_NAMES_MAX`/`AUTH_HOME_MAX` truncation hacks |
| 3 | Bounded queues + backpressure (`EAGAIN` async / block sync) | ⬜ | closes the heap-exhaustion DoS |
| 4 | First-class request/reply + correlation IDs; retire `MPI_SYNC` | ⬜ | replaces reply-mailbox-by-name hack |
| 4 | `mpi_mint_reply_mbox()` unique private-name helper | ⬜ | kills login/vlogin copy-paste |
| 5 | Reserved well-known names header + `reserved` flag metadata | ⬜ | single source of truth for service names |
| 6 | Unified wait: `poll()` over mailbox-fds + input + sockets | ⬜ | gated on input subsystem; converts `views` |
| — | Credentials (`ucred`) on post + per-mailbox post-ACL | ⬜ | **delegated** to `multiuser-security-plan.md` |

---

## Prior Art (and what we deliberately do *not* copy)

| Property | Mach (macOS) | ALPC / pipes (Windows) | MPI today | This plan |
|----------|--------------|------------------------|-----------|-----------|
| Addressing | Capability (send right) | Securable handle + ACL | Global string name, no caps | **Keep name-addressing**; gate *reserved* names by cred |
| Who may send | Only right-holders | ACL-enforced | Anyone who knows the name | **Post-ACL + ucred** (via security plan) |
| Receive | Blocks (`mach_msg`) | Blocks (`ReadFile`/`Wait…`) | Non-blocking → busy-poll | **Blocking receive** (Phase 1) |
| Payload | Variable + out-of-line VM | Inline + section objects | Fixed 248, truncated | **Variable-length + hard cap** (Phase 1) |
| Large data | OOL memory, transparent | Shared sections | Shared mem, manual+separate | **Keep shared mem**; cap is the boundary |
| Waitable w/ fds | Yes (kqueue/GCD) | Yes (`WaitForMultipleObjects`) | No — not a descriptor | **Mailbox-as-fd + `poll()`** (Phases 1, 6) |
| Service namespace | launchd (managed) | SCM / named-pipe ns | First-create-wins, leaks | **Reserved-names header**; supervision = non-goal |
| Sync/async | — / both | `Send`/`Post` | `MPI_SYNC`/`MPI_ASYNC` | **Request/reply w/ correlation** (Phase 4) |

**Explicitly out of scope** (the heavyweight half of Mach/ALPC we are *not*
building): passing capabilities/descriptors *inside* a message (Mach send-right
transfer); an IDL/stub compiler (MIG/MIDL); networked or remote MPI; per-message
priority classes; out-of-line VM message bodies (our shared-memory + handle
pattern is the deliberate substitute). Service **supervision/restart** is also
out of scope for MPI — that is an `init`/service-manager concern, not the IPC
layer's.

---

## Phase 0 — Cleanup (no behaviour change)

- Remove the dead `sysMpi*` wrappers in `sys/mpi/message.c` (and their
  declarations in `sys/include/ubixos/syscalls.h`): zero callers, wired into
  neither syscall table. The live thunks are `sys_mpi*` in
  `sys/mpi/mpi_syscalls.c` (i386) and the `NATIVE_MPI_*` cases in
  `sys/arch/aarch64/kern/syscall.c`.
- The arch-doc rewrite and the ubistry-truncation note are already done.
- **Test:** both arches build and boot unchanged.
- **Risk:** none.

## Phase 1 — The v2 core API (the load-bearing change)

Descriptor-shaping, blocking receive, and variable-length payloads all change
the API surface. Landing them **together as one break** means every call site
is touched **once**, not three times. This is the phase everything else builds
on.

### 1a. Mailbox becomes a descriptor

- `int mpi_create(const char *name, int flags)` creates a mailbox, makes the
  caller the owner, and returns a **file descriptor** allocated from the
  process fd table (same allocator pipes/sockets use). `flags` carries
  `O_NONBLOCK` (and later `reserved`-name intent — Phase 5).
- The owner receives via the fd: a blocking `mpi_recv(int fd, void *buf,
  size_t len, ...)` (and `read(fd)`-compatibility where it makes sense).
- `close(fd)` releases the mailbox and frees queued messages. Because the
  mailbox now lives in the fd table, the **existing fd-teardown path in
  `endTask` reclaims it automatically** — `mpi_destroyProcessMboxes` becomes a
  fallback, then redundant.
- **Posting stays name-addressed.** Senders keep
  `mpi_post(const char *name, u_int32_t type, const void *data, size_t len)` —
  no descriptor needed to *send*, preserving service discovery. (A
  descriptor-based send for an already-opened handle can come later if a hot
  path wants it; not required.)

### 1b. Blocking receive

- `mpi_recv` on an empty queue **sleeps** on a per-mailbox wait channel; `mpi_post`
  **wakes** a waiter. Reuses the existing `sched_io`-style sleep/wakeup
  primitives. `O_NONBLOCK` preserves today's `-EAGAIN`/`-1`-immediately
  behaviour for callers that still want to poll.
- This deletes the `sched_yield()` poll loop from every single-source daemon
  (`authd`, `automountd`, `ubistry`, the `ubixfs` thread, `systemtask`) and lets
  the scheduler reach a real idle (WFI/HLT). **This is the SoC-can-idle win.**
- Interaction with `smp-plan.md`: the per-mailbox sleep must be re-checked under
  the MPI lock after wakeup (no lost-wakeup race); when Phase 3 of smp-plan makes
  `spinLock` a true preemption-disabling spinlock, MPI inherits it safely because
  it never sleeps *while holding* the lock — the sleep is on the wait channel,
  lock released.

### 1c. Variable-length payload + hard cap

- `mpi_post`/`mpi_recv` carry an explicit `len`. The kernel `kmalloc`s
  `sizeof(header) + len`, so a 4-byte opcode costs ~4 bytes, not 248.
- A **hard cap** (one page, 4 KB) bounds a single message. Above the cap the
  contract is explicit: **use shared memory and post the handle.** The cap is
  the principled boundary the fixed-248 number was pretending to be.
- Note: variable-length **raises** the truncation cliff (from 224 B to ~4 KB);
  it does not abolish it. For genuinely unbounded sets (a registry node with
  thousands of children) the `truncated` flag stays as the honest overflow
  signal, or enumeration gains a cursor/continuation (Phase 4 territory). Do not
  claim variable-length makes truncation impossible — it moves the cliff.

### 1d. Compatibility shims

- Keep thin `mpi_createMbox`/`mpi_postMessage`/`mpi_fetchMessage` shims over the
  v2 core (fixed-248, name-based, non-blocking) so the tree keeps building while
  consumers migrate. They are removed in Phase 2.
- **Test:** a daemon converted to `mpi_recv` blocks (no CPU spin — observable as
  cpu0 `idle_ticks` climbing, once Phase 3.5 of smp-plan lands accounting);
  unconverted daemons still work via shims; both arches boot to desktop.
- **Risk:** medium — fd-table integration + sleep/wakeup are the first real new
  kernel surface. Contained because shims keep old paths alive.

## Phase 2 — Migrate consumers; drop the truncation hacks

- Convert `ubistry`, `authd`, the display protocol, and every other consumer to
  the v2 API. Re-size their DTOs to their *natural* size instead of
  back-computing from 248: `UB_NAMES_MAX`, `AUTH_HOME_MAX`/`AUTH_SHELL_MAX`, and
  `display_claim_req`'s twin 64-byte strings stop being capacity-driven.
- `ubistry`'s `UB_MSG_CHILDREN` reply uses a right-sized (or cursor-based) name
  list; the common case no longer truncates.
- Remove the Phase-1 compat shims and the fixed-248 assumptions.
- **Test:** enumerate a registry node with > 224 bytes of child names — full
  list returns, `truncated == 0`. Login/auth round-trips unchanged.
- **Risk:** low-medium — mechanical, but touches several daemons; do it
  per-daemon, both arches green after each.

## Phase 3 — Bounded queues + backpressure

- Per-mailbox depth cap. On overflow: async `mpi_post` returns `-EAGAIN`; a
  blocking sender sleeps until space frees (the symmetric counterpart to Phase
  1b's blocking receive). Closes the unbounded-heap-exhaustion DoS.
- **Test:** a sender flooding a slow receiver gets backpressured, not OOM; kernel
  heap stays bounded.
- **Risk:** low.

## Phase 4 — First-class request/reply

- A `mpi_request()`/`mpi_reply()` pair with a kernel-assigned **correlation
  token**, so a client can match a reply to its request without minting a
  private mailbox and polling it. Retire `MPI_SYNC` (it only ever confirmed the
  queue *drained*, never that the message was *handled*, and has zero callers).
- `mpi_mint_reply_mbox()` — a helper that returns a guaranteed-unique private
  mailbox name (e.g. `reply.<pid>.<seq>`), replacing the hand-rolled reply-name
  code copy-pasted in `bin/login/main.c` and `bin/vlogin/vlogin.cc`.
- **Test:** concurrent requests from one client get correctly matched replies; no
  reply-name collisions across instances.
- **Risk:** low-medium.

## Phase 5 — Service-namespace hardening (proportionate; *not* launchd)

uBixOS has ~6 well-known service mailboxes and a swarm of per-instance reply
mailboxes. It does not need on-demand activation or a dependency graph. The
real defects are scattered names, collision-prone private names, and name
squatting — all fixable cheaply:

- **Reserved-names header** (`include/mpi/well_known.h`): the well-known service
  mailbox names (`init`, `system`, `ubixfs`, `ubistry`, authd's, automountd's)
  as constants — a single source of truth that kills the scattered string
  literals and typo'd-name silent failures.
- **`reserved` flag** in mailbox metadata: a mailbox created with a reserved
  name is marked; the credential layer (security plan) enforces that **only
  `uid 0` may create a reserved name**, which is the actual anti-squatting
  defense. The MPI layer supplies the metadata; the *policy* lives in
  `multiuser-security-plan.md`.
- Private/reply names are minted by the Phase-4 helper, so they are unique by
  construction and never collide with reserved names.
- **Supervision/restart is explicitly a non-goal here** — if `authd` dies,
  restarting it is `init`'s (or a future `servicd`'s) job, not MPI's. Noted so
  the namespace work does not smuggle in a supervisor.
- **Test:** a non-root process cannot create `authd`'s mailbox; reserved names
  resolve from the header in every daemon.
- **Risk:** low (the enforcement half rides the security plan's schedule).

## Phase 6 — Unified wait (`poll()` over mailbox-fds + input + sockets)

The one capability gap macOS/Windows have that we lack. **It is not redundant
with blocking receive — for a multiplexer the two conflict:** the `views`
compositor services three independent sources in one loop — keyboard
(`_sys_getkbd`, slot 46), mouse (`_sys_getmouse`, slot 44), and its MPI mailbox
— and busy-spins all three with `ubix::yield()`. Blocking `mpi_recv` would make
it *block on MPI and miss input*. The only correct fix is to wait on all
sources at once.

- Make mailbox descriptors (Phase 1) participate in `poll()`/`select()`:
  `POLLIN` when the queue is non-empty.
- **Cross-cutting dependency:** keyboard and mouse are *not* fds today — they are
  native syscall pollers. The unified wait only pays off once input also becomes
  waitable, which is owned by `console-and-arch-convergence-plan.md`. This phase
  is therefore **gated on that input rework**, and is the point at which the
  `views` loop converts from a three-way busy-poll to a single `poll()`.
- **Test:** `views` blocks in one `poll()` and wakes on kbd, mouse, *or* a client
  display message; CPU idles when the desktop is quiescent.
- **Risk:** medium-high — the largest surface, but the descriptor seam from
  Phase 1 means it is a wire-up, not a rewrite of every call site.

## Delegated — Credentials + post-ACL

Stamping `ucred` on posts and enforcing a per-mailbox post-ACL is owned by
`multiuser-security-plan.md` (it owns `struct ucred` and the one MI access
chokepoint). This plan's contribution is the **metadata hooks**: the owner
credential on the mailbox and the `reserved` flag (Phase 5) that the security
layer's policy reads. Sequencing: the MPI metadata can land independently; the
*enforcement* tracks the security plan's Phase 1–2.

---

## Sequencing summary

Phase 0 (cleanup) and the credential metadata are independent and can land any
time. Phase 1 is the gate for everything: descriptor + blocking + variable-length
as one API break. Phases 2–5 layer on Phase 1 and are each low-to-medium risk.
Phase 6 (unified wait) is last and is gated on the input-subsystem rework in the
convergence plan. Both architectures stay green after every phase; the MI-only
footprint means neither `sys/arch/` tree changes except where a syscall slot is
already dispatched.
