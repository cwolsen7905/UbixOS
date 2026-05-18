# IPC / Pipes / Semaphores Audit Findings

## Summary

14 findings: 3 critical, 4 high, 4 medium, 3 low

---

## Findings

### IPC-001 — `sem_init` does not check `kmalloc` return value
**Severity:** 🔴 Critical
**File:** `sys/kernel/sem.c:66–68`
**Description:** `kmalloc(sizeof(struct sys_sem))` result is stored in `newSem` and the very next line writes `newSem->signaled = count` with no NULL check. If the heap is exhausted, `newSem` is `0x0` and the dereference triple-faults the kernel. The same pattern occurs inside `ubthread_cond_init` (ubthread.c:53) and `ubthread_mutex_init` (ubthread.c:65), both of which are called from `sem_init`.
**Impact:** Kernel triple-fault/panic under low-memory conditions. lwIP calls `sys_sem_new` → `sem_init` during boot and on every new connection; one allocation failure during network startup silently crashes the system.
**Suggested fix:** Check `newSem != NULL` immediately after `kmalloc` and return `ENOMEM`. Apply the same NULL guard in `ubthread_cond_init` and `ubthread_mutex_init`.

---

### IPC-002 — `sys_mbox_free` acquires the mbox lock and then destroys it while holding it
**Severity:** 🔴 Critical
**File:** `sys/net/net/sys_arch.c:203–222`
**Description:** `sys_mbox_free` calls `sys_arch_sem_wait(&mbox->lock, 0)` to acquire the lock, then calls `sem_destroy(&mbox->lock)` while still holding it. `sem_destroy` calls `ubthread_mutex_destroy` → `kfree` on the underlying mutex object. Any thread spinning inside `ubthread_mutex_lock` on the same mbox now spins on freed memory.
**Impact:** Use-after-free of the mutex object; heap corruption; potential kernel panic from timer-driven lwIP callbacks racing the free.
**Suggested fix:** After acquiring the lock, drain messages, then *signal* (release) the lock before calling `sem_destroy`. Set `*mb = SYS_MBOX_NULL` atomically at the start to prevent new callers from entering.

---

### IPC-003 — `mpi_postMessage` MPI_SYNC path reads freed mbox pointer after dropping spinlock
**Severity:** 🔴 Critical
**File:** `sys/mpi/system.c:204–212`
**Description:** After `spinUnlock(&mpiSpinLock)`, the code does `while (mbox->msg != 0x0) sched_yield()` with `mbox` being a raw pointer obtained under the lock. Between the unlock and the spin, another thread can call `mpi_destroyMbox` which `kfree(mbox)`. The spin then reads `mbox->msg` on freed heap memory.
**Impact:** Use-after-free, heap corruption, kernel panic. Silent under single-CPU QEMU but exploitable whenever SMP is active.
**Suggested fix:** Either hold a reference count on mbox objects, or implement synchronous delivery via a per-message semaphore rather than polling the mbox pointer after releasing the global lock.

---

### IPC-004 — `ubthread_cond_wait` does not arm `cond->lock` before releasing the mutex — lost wakeup
**Severity:** 🟠 High
**File:** `sys/kernel/ubthread.c:162–169`
**Description:** `ubthread_cond_wait` does NOT set `ubcond->lock = TRUE` before calling `ubthread_mutex_unlock`. If a signaller calls `ubthread_cond_signal` between the unlock and the `while (ubcond->lock == TRUE)` check, `lock` is already `FALSE` and the waiter never sleeps — a classic lost wakeup. The `cond_wait` helper in `sys_arch.c` correctly arms `ubcond->lock = TRUE` first (line 439), but the core `ubthread_cond_wait` does not. Additionally, `ubthread_cond_broadcast` is identical to `ubthread_cond_signal` — it does not wake multiple waiters.
**Impact:** Threads waiting on `sys_arch_sem_wait` with `timeout==0` (lwIP's tcpip_thread) can sleep forever if a signal races the wait. Manifests as a hung network stack under load.
**Suggested fix:** Add `ubcond->lock = TRUE;` at the start of `ubthread_cond_wait` and `ubthread_cond_timedwait`, before calling `ubthread_mutex_unlock`.

---

### IPC-005 — `sys_mbox_post` full-queue check overflows when `tail` wraps `uint32_t`
**Severity:** 🟠 High
**File:** `sys/net/net/sys_arch.c:234`, also line 271 (`sys_mbox_trypost`)
**Description:** The full check is `(mbox->tail + 1) >= (mbox->head + SYS_MBOX_SIZE)`. Both `head` and `tail` are `uint32_t` and grow monotonically. When `tail` reaches `UINT32_MAX`, `tail + 1` wraps to `0`, which is less than `head + SYS_MBOX_SIZE`, so the full check falsely passes and a new message overwrites `msgs[0]`.
**Impact:** Silent message corruption/loss in the lwIP mailbox after ~4 billion posts.
**Suggested fix:** Use modular difference: `(mbox->tail - mbox->head) >= SYS_MBOX_SIZE`. Unsigned subtraction wraps correctly for monotonic counters.

---

### IPC-006 — Pipe `sys_write` does not check `kmalloc` return values
**Severity:** 🟠 High
**File:** `sys/kernel/vfs_calls.c:395–396`
**Description:** `pBuf = kmalloc(sizeof(struct pipeBuf))` and `pBuf->buffer = kmalloc(uap->nbyte)` are used immediately with no NULL check. If the first fails, `pBuf->buffer = ...` writes through NULL. If the second fails, the subsequent `memcpy(pBuf->buffer, ...)` writes to address 0.
**Impact:** Kernel triple-fault/panic when the heap is exhausted during a write to a pipe.
**Suggested fix:** Check both allocations; on failure free any already-allocated buffer and return `-1`.

---

### IPC-007 — `pipe()` and `sys_pipe2()` do not check `falloc` return values
**Severity:** 🟠 High
**File:** `sys/kernel/pipe.c:51–52`, `sys/kernel/kern_pipe.c:57–58`
**Description:** Both functions call `falloc` twice and never inspect the error return. When `falloc` fails (process at `MAX_FILES`), it returns `EMFILE`, sets `*resultfp = NULL`, and `*resultfd = 0`. The code then writes `nfp1->data = pipeDesc` through a NULL pointer and stores `fd1 = 0` in `pipeDesc->rFD`, silently aliasing stdin to the read end of the pipe.
**Impact:** Kernel panic (NULL-pointer write) or silent stdin corruption for the process.
**Suggested fix:** Check both `falloc` returns; on error, `kfree(pipeDesc)` and return `EMFILE`.

---

### IPC-008 — `mpi_destroyMbox` leaks all queued messages
**Severity:** 🟡 Medium
**File:** `sys/mpi/system.c:275–303`
**Description:** `mpi_destroyMbox` calls `kfree(mbox)` without first walking and freeing the `mpi_message_t` linked list rooted at `mbox->msg`.
**Impact:** One `mpi_message_t` (256 bytes) leaked per unread message at destroy time. Processes that create and destroy mailboxes repeatedly (e.g. the display compositor on reconnect) will gradually exhaust the kernel heap.
**Suggested fix:** Walk `mbox->msg` freeing each node before `kfree(mbox)`.

---

### IPC-009 — Pipe close path never frees `pipeInfo` or `pipeBuf` chain when both ends are closed
**Severity:** 🟡 Medium
**File:** `sys/kernel/vfs_calls.c:123–141`
**Description:** `sys_close` for pipe type (fd_type==3) decrements `rfdCNT` / `wfdCNT` but never checks if both have reached 0. The `pipeInfo` struct and its entire `pipeBuf` chain are therefore never freed.
**Impact:** Heap leak per pipe lifetime; each leaked `pipeBuf` also holds a separately allocated `buffer`.
**Suggested fix:** After both decrements, if `rfdCNT <= 0 && wfdCNT <= 0` free the entire `pipeBuf` chain (and each `buffer`) then `kfree(pFD)`.

---

### IPC-010 — `sys_read` pipe path frees `pipeBuf` node without freeing its `buffer`
**Severity:** 🟡 Medium
**File:** `sys/kernel/vfs_calls.c:218–221`
**Description:** When a `pipeBuf` is fully consumed: `kfree(rpFD)` is called but `rpFD->buffer` (allocated separately in `sys_write`) is never freed.
**Impact:** One heap allocation of `nbytes` bytes leaked per pipe-read that exhausts a buffer node. Shell pipelines that transfer data continuously will slowly exhaust the heap.
**Suggested fix:** Add `kfree(rpFD->buffer);` before `kfree(rpFD);`.

---

### IPC-011 — `ubthread_mutex_unlock` inner busy-loop is logically inverted — spins on double-unlock
**Severity:** 🟡 Medium
**File:** `sys/kernel/ubthread.c:119–138`
**Description:** The unlock spins `while (ubmutex->lock == FALSE) sched_yield()` if the `xchg` returned `FALSE`. This is the condition for a double-unlock (lock was already free) — instead of panicking it liveloops until something else sets `lock = TRUE` and re-locks the mutex, then successfully unlocks it again. The `kpanic("NOT LOCKED?")` check at line 124 should prevent reaching this, but only when `_current` is set; interrupt context could bypass it.
**Impact:** Kernel livelock on any double-unlock; misleading runtime behaviour.
**Suggested fix:** Remove the outer `while(1)` retry loop; a single `xchg_32(&ubmutex->lock, FALSE)` is sufficient for an unlock. Panic if the old value was already `FALSE`.

---

### IPC-012 — `sys_mbox_new` does not free partially-constructed mbox on `sys_sem_new` failure
**Severity:** 🔵 Low
**File:** `sys/net/net/sys_arch.c:168–201`
**Description:** `sys_mbox_new` calls `sys_sem_new` three times with no error check. After IPC-001 is fixed, `sys_sem_new` can return `ENOMEM`. If the second or third call fails, already-allocated semaphores and the `mbox` struct are leaked.
**Impact:** Heap leak on lwIP mbox creation failure.
**Suggested fix:** Check each `sys_sem_new` return; on failure destroy earlier semaphores and `kfree(mbox)` before returning `ERR_MEM`.

---

### IPC-013 — MPI `mpi_spam`/`mpi_postMessage` copy `MESSAGE_LENGTH` bytes from unvalidated user pointer
**Severity:** 🔵 Low
**File:** `sys/mpi/system.c:138`, `189`
**Description:** The syscall wrappers in `message.c` pass user-supplied `data`/`msg` pointers straight into `memcpy(..., data, MESSAGE_LENGTH)` with no bounds check. A process can pass a pointer that is valid for fewer than 248 bytes and cause the kernel to read past the end of the mapping.
**Impact:** Kernel page fault in syscall context → panic. Reliable DoS for any process.
**Suggested fix:** Validate that the user-supplied pointer covers at least `MESSAGE_LENGTH` bytes within the process's mapped address space before copying (or use a `copyin`-style helper once one is available).

---

### IPC-014 — `sys_arch_sem_wait` infinite-wait path returns 0, which lwIP maps to `SYS_ARCH_TIMEOUT`
**Severity:** 🔵 Low
**File:** `sys/net/net/sys_arch.c:130–136`
**Description:** In the `timeout == 0` (infinite wait) branch, `cond_wait` is called and its return value discarded; `time_needed` remains `0`. The caller checks `if (time_needed == 0) return SYS_ARCH_TIMEOUT` — but that guard is only inside the `timeout > 0` block, so the infinite-wait path returns `0` to the lwIP caller. lwIP's `sys_timeouts_mbox_fetch` treats a `0` return from `sys_arch_mbox_fetch` as success, so the impact is minor in current code; however the semantics are fragile if lwIP is upgraded.
**Impact:** Latent semantic mismatch; could cause spurious timeout handling if the lwIP timer code is updated.
**Suggested fix:** In the non-timeout success path, return `1` (any positive non-`SYS_ARCH_TIMEOUT` value) to unambiguously signal "woken by signal, not timed out".
