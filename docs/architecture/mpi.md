# UbixOS Message Passing Interface (MPI)

**Source:** `sys/mpi/`
**Kernel header:** `sys/include/mpi/mpi.h`
**Userland header:** `include/sys/mpi.h`
**Syscall table:** native (`int 0x81` on i386, `svc` native path on aarch64), slots 50–53

> **Status:** This document describes MPI **as built** on branch
> `wip/aarch64-port` (verified against `sys/mpi/system.c` June 2026). It is a
> description of current behaviour, not a wish list. Known shortcomings are
> listed in [Current Limitations](#current-limitations); the plan to address
> them lives in `docs/design/mpi-modernization-plan.md`.

---

## Overview

MPI is UbixOS's primary in-kernel IPC mechanism. It provides named
**mailboxes** — kernel-managed FIFO queues identified by a string name. Any
process can post a message to a mailbox by name; only the owning process (the
one that created the mailbox) can read from it.

MPI sits alongside POSIX pipes and semaphores but is the mechanism used by
`init`, `views`, `vlogin`, `authd`, `automountd`, `ubistry`, the `ubixfs`
background thread, and the kernel system task to communicate at boot and during
normal operation.

In practice MPI is the system's **control plane**: it carries small opcodes and
fixed-size control structs. Bulk data (framebuffer pixels, audio rings) is
never streamed through MPI — it travels through shared memory
(`vmm_share_region`), and MPI carries only the signal that the shared region is
ready. The 248-byte payload reflects that role.

---

## Data Structures

All types are defined in `sys/include/mpi/mpi.h`.

### `mpi_message_t` — a queued message

```c
struct mpi_message {
    char                data[MESSAGE_LENGTH];   /* payload — 248 bytes        */
    u_int32_t           header;                 /* message type / opcode      */
    pidType             pid;                    /* sender PID (filled by kernel) */
    struct mpi_message *next;                   /* intrusive singly-linked list  */
};
```

`MESSAGE_LENGTH` is 248. The kernel always copies the message into a freshly
`kmalloc`'d `mpi_message_t` on post, so the caller's buffer does not need to
remain valid after `mpi_postMessage` returns. The sender's PID is stamped by
the kernel (`message->pid = _current->id`), not supplied by the caller.

### `mpi_mbox_t` — a mailbox

```c
struct mpi_mbox {
    struct mpi_mbox    *next;       /* global list — next mailbox            */
    struct mpi_mbox    *prev;       /* global list — prev mailbox            */
    struct mpi_message *msg;        /* head of message queue                 */
    struct mpi_message *msgLast;    /* tail of message queue (O(1) append)   */
    char                name[64];   /* mailbox name (63 chars + NUL)         */
    pidType             pid;        /* owner PID (set at creation)           */
};
```

### Message-type constants

```c
#define MPI_ASYNC 0x1   /* post and return immediately                       */
#define MPI_SYNC  0x2   /* post and wait until the receiver drains the queue */
```

The type controls only the post-call behaviour (see `mpi_postMessage`). It is
**not** stored in the message — receivers cannot tell how a message was sent.
Any value other than `MPI_SYNC` behaves as `MPI_ASYNC`.

### Global state (`sys/mpi/system.c`)

```c
static mpi_mbox_t      *mboxList   = 0x0;                 /* doubly-linked mailbox list */
static struct spinLock  mpiSpinLock = SPIN_LOCK_INITIALIZER;
```

All operations acquire `mpiSpinLock` for their entire duration. Mailboxes are
inserted at the head of `mboxList`; the newest mailbox is at the front.

> **Note on the lock:** today `spinLock()` is a *yielding* lock — under
> contention it calls `sched_yield()` rather than busy-spinning. This is
> correct and deadlock-free on the current uniprocessor kernel. The SMP +
> in-kernel-preemption work (`docs/design/smp-plan.md`, Phase 3) replaces it
> with a true spinning, preemption-disabling lock governed by `preempt_count`;
> MPI inherits that change for free because it holds the lock for short,
> non-sleeping critical sections only.

---

## Kernel Functions

### `mpi_findMbox(char *name)` — internal

Linear scan of `mboxList` by name. **Not task-safe** — every caller must
already hold `mpiSpinLock`.

### `mpi_mbox_exists(const char *name)` → 1 / 0

Public, task-safe wrapper around `mpi_findMbox` (takes and releases the lock).
Used by the boot path to wait for a daemon (e.g. `ubistry`) to come up before
launching clients that query it. Returns 1 if the mailbox exists, 0 otherwise.

### `mpi_createMbox(char *name)` → 0 / -1

Creates a new mailbox owned by `_current`.

1. Acquires the lock.
2. Rejects duplicate names via `mpi_findMbox` → -1.
3. `kmalloc`s the mailbox; returns -1 if allocation fails.
4. Copies the name with `strncpy` + explicit NUL (bounded to `sizeof name`).
5. Sets `pid = _current->id`, initialises `msg`/`msgLast` to NULL.
6. Inserts at the head of `mboxList` (handles both empty-list and
   non-empty-list cases, maintaining `prev` links).
7. Releases the lock. Returns 0 on success.

### `mpi_destroyMbox(char *name)` → 0 / -1

Destroys the named mailbox. Only the owner PID may destroy it.

1. Acquires the lock.
2. Linear-searches `mboxList` for `name`.
3. Returns -1 if `mbox->pid != _current->id` (not the owner).
4. Unlinks from the doubly-linked list (correct for head, tail, and middle).
5. Frees every queued message, then frees the mailbox.
6. Releases the lock. Returns 0 on success, -1 if not found or unauthorized.

### `mpi_destroyProcessMboxes(pidType pid)` → void

Destroys **every** mailbox owned by `pid`, freeing all queued messages.
Called from `endTask()` so a process's mailboxes do not leak when it exits
(cleanly or via a crash). Unlike `mpi_destroyMbox`, it does **not** gate on
`_current->id` — the owner is already gone.

This closes a real correctness hole: a leaked mailbox keeps its dead owner's
PID, so `mpi_createMbox` would refuse to recreate the name and
`mpi_fetchMessage` (owner-PID-gated) would reject the new owner. Without this,
relaunching a `views` app would hang waiting for a `DISPLAY_ACK` it could never
fetch.

### `mpi_postMessage(char *name, u_int32_t type, mpi_message_t *msg)` → 0 / 1

Posts one message to the named mailbox.

1. Acquires the lock.
2. Finds the mailbox; returns 1 if not found.
3. `kmalloc`s a new message; returns 1 if allocation fails.
4. Copies `header` and `data`, stamps `pid = _current->id`.
5. Appends to the queue tail via `msgLast` (O(1); handles the empty-queue case).
6. Releases the lock.
7. If `type == MPI_SYNC`: loops `sched_yield()` then **re-looks-up the mailbox
   by name under the lock** each iteration, breaking when the mailbox is gone
   or its queue is empty. Re-finding by name (rather than reusing the stale
   `mbox` pointer) is what makes this safe against a concurrent
   `mpi_destroyMbox` that frees the mailbox.

Returns 0 on success, 1 if the mailbox was not found.

> `MPI_SYNC` guarantees only that the message was **dequeued**, not that it was
> processed, and it has no callers in the current tree. See
> [Current Limitations](#current-limitations).

### `mpi_fetchMessage(char *name, mpi_message_t *msg)` → 0 / -1

Retrieves the next message from the named mailbox. **Non-blocking** — returns
-1 immediately if the queue is empty.

1. Acquires the lock.
2. Returns -1 if the mailbox is not found.
3. Returns -1 if the queue is empty (`mbox->msg == NULL`).
4. Returns -1 if `mbox->pid != _current->id` (not the owner).
5. Copies `header`, `data`, and the sender `pid` into the caller's `msg`.
6. Dequeues the head, resetting `msgLast` to NULL when the queue drains, and
   frees the old head.
7. Releases the lock. Returns 0 on success.

### `mpi_spam(u_int32_t type, void *data)` → 0

Broadcasts a copy of `data` (a full `MESSAGE_LENGTH` payload) to **every**
mailbox in `mboxList`, holding the lock for the entire loop. **Kernel-only** —
its former syscall slot (54) was retired and reused for `fbpresent`, so there
is no userland path to it.

---

## Syscall Interface

MPI is accessed from userland via the **native syscall table**, slots 50–53.
The mechanism differs per architecture but the slot numbers and semantics are
identical:

| Slot | Name | i386 handler | aarch64 handler |
|------|------|--------------|-----------------|
| 50 | `mpiCreateMbox`  | `sys_mpiCreateMbox`  | `NATIVE_MPI_CREATE`  |
| 51 | `mpiDestroyMbox` | `sys_mpiDestroyMbox` | `NATIVE_MPI_DESTROY` |
| 52 | `mpiPostMessage` | `sys_mpiPostMessage` | `NATIVE_MPI_POST`    |
| 53 | `mpiFetchMessage`| `sys_mpiFetchMessage`| `NATIVE_MPI_FETCH`   |

- **i386** registers the four `sys_mpi*` thunks in `sys/kern/syscalls.c` (the
  native `int 0x81` table). The thunks live in `sys/mpi/mpi_syscalls.c` and
  simply place the return value in `td->td_retval[0]`.
- **aarch64** dispatches the same slots from a hand-rolled `switch` in
  `sys/arch/aarch64/kern/syscall.c`, calling the `mpi_*` functions directly.
  The syscall runs in the caller's address space with IRQs masked, so the user
  `name`/`msg` pointers are valid without a copyin.

> **Dead code:** `sys/mpi/message.c` defines an older set of `sysMpi*` wrappers
> (`sysMpiCreateMbox`, `sysMpiPostMessage`, …) with different signatures. These
> have no callers anywhere in the tree and are not wired into either syscall
> table. They should be removed.

---

## Userland API

Assembly stubs live in **`lib/ubix_api/`** (dual-arch — each `.S` has an
`#ifdef __aarch64__` path):

```c
int mpi_createMbox(char *name);                                   /* slot 50 */
int mpi_destroyMbox(char *name);                                  /* slot 51 */
int mpi_postMessage(char *name, u_int32_t type, mpi_message_t *); /* slot 52 */
int mpi_fetchMessage(char *name, mpi_message_t *);                /* slot 53 */
```

All four operations have stubs, including `mpi_destroyMbox`.

**Typical usage pattern** (every system daemon follows this shape):

```c
#include <sys/mpi.h>

/* Receiver: create a mailbox and poll it. */
mpi_message_t msg;

if (mpi_createMbox("myservice") != 0)
    panic("mailbox already exists");

for (;;) {
    if (mpi_fetchMessage("myservice", &msg) == 0) {
        /* process msg.header and msg.data */
    } else {
        sched_yield();   /* no messages — yield (see Current Limitations) */
    }
}

/* Sender: post a message. */
mpi_message_t out;
out.header = 42;
memcpy(out.data, payload, sizeof(payload));
mpi_postMessage("myservice", MPI_ASYNC, &out);
```

---

## System Mailboxes

These mailboxes are created at boot by the named processes:

| Mailbox name | Created by | Purpose |
|--------------|-----------|---------|
| `"init"`     | `bin/init/main.c`              | PID 1 command mailbox |
| `"system"`   | `sys/arch/*/…/systemtask.c`    | Kernel system task (V86/BIOS, etc.) |
| `"ubixfs"`   | `sys/fs/ubixfs/thread.c`       | UbixFS background thread |
| `"ubistry"`  | `bin/ubistry/message.c`        | Key-value registry service |
| `AUTHD_MBOX` | `bin/authd/main.c`             | Authentication service |
| `AUTOMOUNTD_MBOX` | `bin/automountd/main.c`   | Storage arrive/depart events |

GUI clients (`views`, `vlogin`, `login`, `muffin`, `hello`, `vdoom`, …) create
private per-instance mailboxes for replies and display events.

---

## Concurrency model

- **One global lock.** Create, destroy, post, fetch, spam, and `exists` all
  serialize on `mpiSpinLock`. Every operation is an O(n) string scan of the
  global list under that lock. Adequate for the current handful of mailboxes
  and low message rates; it will become a contention point under SMP (tracked
  in `docs/design/smp-plan.md`).
- **Owner-gated reads.** Only the creating process (by PID) may fetch from a
  mailbox. Any process may post to any mailbox by name.
- **Sender identity** is stamped by the kernel from `_current->id`. There is no
  other authentication on either post or fetch.

---

## Current Limitations

These are **true of the code today** and are the subject of
`docs/design/mpi-modernization-plan.md`:

- **No blocking receive.** `mpi_fetchMessage` returns -1 immediately on an
  empty queue, so every receiver is a `sched_yield()` poll loop (`authd`,
  `automountd`, `ubistry`, the `ubixfs` thread, `systemtask`). A yielding
  receiver stays permanently runnable, so the scheduler never reaches a true
  idle (WFI/HLT) state — a real power/thermal cost on the forward bare-metal
  targets (Raspberry Pi / Orange Pi).

- **No authentication on posts.** Any process can post to any mailbox by name;
  there is no credential check and no per-mailbox post ACL. Security-sensitive
  mailboxes (`authd`, `system`, `ubistry`) are wide open. This is a gap
  relative to `docs/design/multiuser-security-plan.md`.

- **Fixed 248-byte payload, truncating real data.** `data[]` is a fixed
  array, so every message — even a 4-byte opcode — costs 248 bytes, and
  payloads that should be larger are filed down to fit. This is not
  hypothetical: `AUTH_HOME_MAX` (128) and `UB_NAMES_MAX` (224) are
  back-computed from 248, and `ubistry`'s `UB_MSG_CHILDREN` reply
  (`struct ub_children_rsp`) carries an explicit `truncated` flag because a
  registry node with many children **does not fit** — its child list is
  silently cut off. The payload should be variable-length with a hard cap.

- **No flow control / unbounded queues.** A mailbox can be filled without
  limit, exhausting kernel heap. There is no depth cap and no backpressure.

- **No first-class request/reply.** `MPI_SYNC` only confirms the queue drained,
  not that the message was handled, and is unused. Real RPC is faked in
  userland by creating a private reply mailbox and polling it (e.g.
  `bin/login/main.c`, `bin/vlogin/vlogin.cc`), with no correlation IDs.

- **String-keyed, not handle-keyed.** Every call re-scans the list by name
  under the global lock; mailboxes are not descriptors, so a daemon cannot
  `poll()`/`select()` across a mailbox and a socket together — which is *why*
  everything polls.

- **PID-based identity is weak.** Both the owner gate and the sender stamp use
  `pidType`, which is recycled. `mpi_destroyProcessMboxes` mitigates the
  worst leak case, but the underlying identity is still a recyclable PID.
