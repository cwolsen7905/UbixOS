# UbixOS Message Passing Interface (MPI)

**Source:** `sys/mpi/`  
**Kernel header:** `sys/include/mpi/mpi.h`  
**Userland header:** `include/sys/mpi.h`  
**Syscall table:** native (`int 0x81`), slots 50–53

---

## Overview

MPI is UbixOS's primary IPC mechanism. It provides named **mailboxes** —
kernel-managed FIFO queues identified by a string name. Any process can post
a message to a mailbox by name; only the owning process (the one that
created the mailbox) can read from it.

MPI sits alongside POSIX pipes and semaphores but is the mechanism used by
`init`, `ubistry`, `ttyd`, `ubixfs`, and the kernel system task to
communicate at boot and during normal operation.

---

## Data Structures

All types are defined in `sys/include/mpi/mpi.h`.

### `mpi_message_t` — a queued message

```c
struct mpi_message {
    char            data[248];      /* payload — MESSAGE_LENGTH bytes */
    uInt32          header;         /* message type / opcode           */
    pidType         pid;            /* sender PID (filled by kernel)   */
    struct mpi_message *next;       /* intrusive singly-linked list    */
};
```

`MESSAGE_LENGTH` is 248. The kernel always copies messages into a new
`mpi_message_t` allocation on post, so the caller's buffer does not need to
remain valid after `mpi_postMessage` returns.

### `mpi_mbox_t` — a mailbox

```c
struct mpi_mbox {
    struct mpi_mbox  *next;         /* global list — next mailbox     */
    struct mpi_mbox  *prev;         /* global list — prev mailbox     */
    struct mpi_message *msg;        /* head of message queue          */
    struct mpi_message *msgLast;    /* tail of message queue (O(1) append) */
    char              name[64];     /* mailbox name (63 chars + NUL)  */
    pidType           pid;          /* owner PID (set at creation)    */
};
```

### Global state (`sys/mpi/system.c`)

```c
static mpi_mbox_t  *mboxList    = NULL;              /* doubly-linked mailbox list */
static struct spinLock mpiSpinLock = SPIN_LOCK_INITIALIZER;
```

All operations acquire `mpiSpinLock` for their entire duration. Mailboxes
are inserted at the head of `mboxList`; newest mailbox is at the front.

---

## Kernel Functions

### `mpi_findMbox(char *name)` — internal

Linear scan of `mboxList` by name. **Not safe to call without holding
`mpiSpinLock`** — all callers must hold the lock.

### `mpi_createMbox(char *name)` → 0 / -1

Creates a new mailbox owned by `_current`.

1. Acquires spinlock.
2. Rejects duplicate names via `mpi_findMbox`.
3. Allocates `mpi_mbox_t` via `kmalloc`.
4. Copies `name` into `mbox->name` with `sprintf` (see [bugs](#known-bugs)).
5. Sets `mbox->pid = _current->id`.
6. Inserts at head of `mboxList`.
7. Releases spinlock. Returns 0 on success, -1 if name already exists.

### `mpi_destroyMbox(char *name)` → 0 / -1

Destroys the named mailbox. Only the owner PID may destroy it.

1. Acquires spinlock.
2. Linear-searches `mboxList` for `name`.
3. Checks `mbox->pid == _current->id`; returns -1 if unauthorized.
4. Unlinks from doubly-linked list and calls `kfree(mbox)`.
5. Returns 0 on success, -1 if not found or unauthorized.

> **Known bug:** NULL dereference if the mailbox is the first or last in the
> list — see [BUG-MPI-01](#known-bugs).

### `mpi_postMessage(char *name, uint32_t type, mpi_message_t *msg)` → 0 / 1

Posts one message to the named mailbox.

1. Acquires spinlock.
2. Finds mailbox by name; returns 1 if not found.
3. Allocates a new `mpi_message_t` and copies `msg->header` and `msg->data`.
4. Sets `message->pid = _current->id` (records sender).
5. Appends to the tail of the mailbox queue via `msgLast`.
6. Releases spinlock.
7. If `type == 0x2` (synchronous send): busy-spins on `mbox->msgLast != NULL`
   until the receiver drains the queue. This is an unbounded spin without
   `sched_yield()` — see [bugs](#known-bugs).

Returns 0 on success, 1 if mailbox not found.

### `mpi_fetchMessage(char *name, mpi_message_t *msg)` → 0 / -1

Retrieves the next message from the named mailbox. Non-blocking — returns -1
immediately if the queue is empty.

1. Acquires spinlock.
2. Finds mailbox; returns -1 if not found.
3. Returns -1 if `mbox->msg == NULL` (no messages).
4. Returns -1 if `mbox->pid != _current->id` (not the owner).
5. Copies `header`, `data`, and `pid` fields from `mbox->msg` to `msg`.
6. Dequeues `mbox->msg` and calls `kfree` on the old head.
7. Releases spinlock. Returns 0 on success.

> **Known bug:** `msgLast` is not reset to NULL when the queue drains —
> see [BUG-MPI-04](#known-bugs).

### `mpi_spam(uint32_t type, void *data)` — kernel-only

Broadcasts a message to every mailbox in `mboxList`. Not reachable from
userland (no syscall slot is wired to it). Used internally by the kernel
to signal all registered processes at once.

Holds `mpiSpinLock` for the entire broadcast loop — duration is proportional
to the number of mailboxes.

---

## Syscall Interface

MPI is accessed from userland via the **native syscall table** (`int 0x81`).

| Slot | Name | Kernel handler |
|------|------|----------------|
| 50 | `mpiCreateMbox` | `sys_mpiCreateMbox` |
| 51 | `mpiDestroyMbox` | `sys_mpiDestroyMbox` |
| 52 | `mpiPostMessage` | `sys_mpiPostMessage` |
| 53 | `mpiFetchMessage` | `sys_mpiFetchMessage` |

The dispatch functions live in `sys/mpi/mpi_syscalls.c` and are thin
wrappers that pull arguments from the `args` struct and call the
corresponding `mpi_*` function.

> `sys_mpiPostMessage` contains a debug `kprintf("mPM: %s", args->name)`
> that fires on every post. Remove this before production use.

---

## Userland API

Assembly stubs in `lib/libc/sys/`:

```c
/* lib/libc/sys/mpi_creatembox.S */
int mpi_createMbox(char *name);    /* syscall 50 via int 0x81 */

/* lib/libc/sys/mpi_postmessage.S */
int mpi_postMessage(char *name, uint32_t type, mpi_message_t *msg); /* syscall 52 */

/* lib/libc/sys/mpi_fetchmessage.S */
int mpi_fetchMessage(char *name, mpi_message_t *msg); /* syscall 53 */
```

`mpi_destroyMbox` exists in the kernel but has no assembly stub in libc —
it must be called directly or via inline asm if needed from userland.

**Typical usage pattern:**

```c
#include <sys/mpi.h>

/* Receiver: create mailbox and poll */
mpi_message_t msg;

if (mpi_createMbox("myservice") != 0)
    panic("mailbox already exists");

for (;;) {
    if (mpi_fetchMessage("myservice", &msg) == 0) {
        /* process msg.header and msg.data */
    } else {
        sched_yield();   /* no messages — yield rather than spin */
    }
}

/* Sender: post a message */
mpi_message_t out;
out.header = 42;
memcpy(out.data, payload, sizeof(payload));
mpi_postMessage("myservice", 0x1, &out);
```

---

## System Mailboxes

These mailboxes are created at boot by the named processes:

| Mailbox name | Created by | Purpose |
|--------------|-----------|---------|
| `"init"` | `bin/init/main.c` | PID 1 command mailbox (receive loop currently disabled) |
| `"system"` | `sys/arch/i386/systemtask.c` | Kernel system task |
| `"ubixfs"` | `sys/fs/ubixfs/thread.c` | UbixFS background thread |
| `"ubistry"` | `bin/ubistry/main.c` | Key-value registry service |

`ttyd` posts to `"system"` but does not create its own mailbox.

---

## Message Types (type argument to `mpi_postMessage`)

No named constants are defined in any header. The only documented value is:

| Value | Meaning |
|-------|---------|
| `0x1` | Asynchronous — post and return immediately |
| `0x2` | Synchronous — post and busy-spin until receiver drains the queue |

All other values are treated identically to `0x1`. The type is not stored
in the message; it only controls the post-call spin behavior.

---

## Known Bugs

These are tracked in [`BUGS.md`](../../BUGS.md) under the MPI section.

| ID | Location | Summary |
|----|----------|---------|
| BUG-MPI-01 | `system.c:248` | `mpi_destroyMbox`: NULL dereference when destroying the head or tail mailbox |
| BUG-MPI-02 | `system.c:79` | `mpi_createMbox`: `mbox->msgLast` never initialized — crash on second post |
| BUG-MPI-03 | `system.c:165` | `mpi_postMessage`/`mpi_spam`: `msgLast` not set when appending to empty queue |
| BUG-MPI-04 | `system.c:220` | `mpi_fetchMessage`: `msgLast` not reset when queue drains |
| BUG-MPI-05 | `system.c:81` | `mpi_createMbox`: `sprintf(mbox->name, name)` — no bounds check on 64-byte field |
| BUG-MPI-06 | `system.c:175` | Synchronous-send busy-spin reads `msgLast` after unlock without re-acquiring lock |
| BUG-MPI-07 | multiple | No NULL check on `kmalloc` return in create/post/spam paths |

---

## Design Notes and Limitations

- **No blocking receive.** `mpi_fetchMessage` returns -1 immediately when
  the queue is empty. Receivers must poll. The conventional pattern is
  `while (mpi_fetchMessage(...) != 0) sched_yield()`, but none of the
  system daemons use `sched_yield()` in their fetch loops.

- **No queue depth limit.** An unlimited number of messages can be queued,
  exhausting kernel heap memory.

- **No cleanup on process exit.** When the owner process dies, its mailbox
  and any queued messages remain in `mboxList` forever. The name cannot be
  reused and memory leaks.

- **Owner-only reads.** Only the creating process can fetch messages.
  There is no concept of shared or multi-reader mailboxes.

- **`mpi_spam` not exposed to userland.** No syscall slot is wired to
  `mpi_spam`. It is callable only from kernel C code.

- **Single global lock.** All operations — create, destroy, post, fetch,
  spam — serialize on `mpiSpinLock`. Fine for the current low-traffic
  usage but does not scale.
