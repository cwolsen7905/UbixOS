# Audit Findings: Networking Bridge

## Summary

21 findings: 4 critical, 7 high, 7 medium, 3 low

---

## Findings

### NET-1 — `getfd` checks the pointer-to-pointer, not the pointer 🔴
**File:** sys/kernel/descrip.c:270 (called from sys/net/net/sys_arch.c throughout)
**Severity:** Critical
**Category:** Null pointer dereference

`getfd()` does `if (fp == 0x0)` where `fp` is `struct file **` — a stack-variable address, never NULL. The actual file pointer is `*fp`. This means `getfd` always returns success even when `td->o_files[fd]` is NULL. Every socket syscall (sys_sendto, sys_recvfrom, sys_connect, sys_bind, sys_listen, sys_accept, sys_sendmsg, sys_recvmsg, sys_setsockopt) then calls `fd->socket` on a NULL fd pointer. Any invalid file descriptor from userland triggers a kernel null dereference and triple-fault.

**Suggested fix:** Fix `getfd` in `descrip.c`: change `if (fp == 0x0)` to `if (*fp == 0x0)`. Add NULL checks on the returned `fd` in syscalls that don't yet have them.

---

### NET-2 — `sys_socket` error check inverted: wrong fd destroyed, lwIP socket leaked 🔴
**File:** sys/net/net/sys_arch.c:540–551
**Severity:** Critical
**Category:** Logic error / resource leak

```c
if (nfp->fd == 0x0 && nfp->socket) {
```
fd 0 is a valid descriptor. This condition fires spuriously for the first socket allocation (fd 0) and destroys it. When `lwip_socket` genuinely fails (returns -1), the condition only fires if fd also happens to be 0 — otherwise the zombie lwIP socket is silently retained in `nfp->socket`.

**Suggested fix:** Change to `if (nfp->socket < 0)`.

---

### NET-3 — `sys_thread_new`: NULL kmalloc not checked; thread linked before creation 🔴
**File:** sys/net/net/sys_arch.c:396–413
**Severity:** Critical
**Category:** Null pointer dereference / list corruption

`kmalloc` at line 396 can return NULL. `memset(new_thread, ...)` at line 397 immediately dereferences it. Worse, `new_thread` is linked into the global `threads` list (lines 400–403) before `ubthread_create` is called. If `kmalloc` fails and execution somehow continues, a NULL node corrupts the linked list walked by every `current_thread()` call thereafter.

**Suggested fix:** Check `kmalloc` return and return NULL on failure. Link `new_thread` into `threads` only after `ubthread_create` succeeds.

---

### NET-4 — `sys_mbox_new`: sem_init failure silently ignored; mbox used with NULL semaphores 🔴
**File:** sys/net/net/sys_arch.c:188–190
**Severity:** Critical
**Category:** Null pointer dereference / uninitialized state

All three `sys_sem_new` calls at lines 188–190 ignore the return value. If any semaphore allocation fails, `mbox->lock/empty/full` remain NULL. The first `sys_arch_sem_wait(&mbox->lock, 0)` in `sys_mbox_post` dereferences `*sem` where `*sem == NULL`.

**Suggested fix:** Check each `sys_sem_new` return; free already-allocated semaphores and the mbox, then return `ERR_MEM`.

---

### NET-5 — `sys_setsockopt`: `getfd` return ignored; unconditional NULL deref 🟠
**File:** sys/net/net/sys_arch.c:554–561
**Severity:** High
**Category:** Null pointer dereference

`getfd` is called but its return value is not checked, and there is no NULL guard on `fd`. Due to the broken `getfd` (NET-1), `fd` will be NULL for any invalid descriptor, and `lwip_setsockopt(fd->socket, ...)` immediately faults.

**Suggested fix:** Add `if (!fd) { td->td_retval[0] = -1; return (-1); }` after `getfd`.

---

### NET-6 — `sys_recvfrom`/`sys_sendto`: user-supplied `len` passed directly to `kmalloc`, no upper bound 🟠
**File:** sys/net/net/sys_arch.c:606, 641
**Severity:** High
**Category:** Missing bounds check / denial of service

`kmalloc(args->len)` with no cap. A process can pass `len = 0xFFFFFFFF`, exhausting the kernel heap.

**Suggested fix:** Cap `args->len` to a sensible maximum (e.g. 65536). Return `EMSGSIZE` if exceeded.

---

### NET-7 — `sys_sendmsg`/`sys_recvmsg`: iovec length accumulation integer-overflow on i386 🟠
**File:** sys/net/net/sys_arch.c:781–783, 832–834
**Severity:** High
**Category:** Integer overflow / heap overflow

`total` is `size_t` (32-bit on i386). Summing attacker-controlled `iov_len` values can wrap to a small value. `kmalloc(total)` allocates a small buffer; the subsequent gather loop writes the full large payload into it — kernel heap overflow.

**Suggested fix:** Check for overflow after each addition, or cap total at 65536.

---

### NET-8 — `sys_recvfrom`: `args->buf` not validated as non-NULL before `memcpy` 🟠
**File:** sys/net/net/sys_arch.c:650–651
**Severity:** High
**Category:** Null pointer dereference

`memcpy(args->buf, kbuf, ret)` with no NULL check on `args->buf`. A null or wild pointer from userland writes `ret` bytes anywhere.

**Suggested fix:** Validate `args->buf != NULL` before the copy.

---

### NET-9 — `ethernetif.c`: hardcoded MAC — hardware MAC never read 🟠
**File:** sys/net/netif/ethernetif.c:96–101
**Severity:** High
**Category:** Incorrect behavior / security

`low_level_init` hardcodes `08:00:27:73:C1:B6` regardless of the actual LNC hardware MAC. If this netif is ever active, the NIC presents the wrong identity to the network. Two hosts using this code would collide.

**Suggested fix:** Read the MAC from the `lnc` hardware structure. If this file is dead code, remove it from the build.

---

### NET-10 — `ethernetif.c` `low_level_input`: stale global `tmpBuf` read without synchronization 🟠
**File:** sys/net/netif/ethernetif.c:203–204
**Severity:** High
**Category:** Race condition

`tmpBuf->length` and `tmpBuf->buffer` are read with no lock or memory barrier. An ISR updating `tmpBuf` between reads produces a mixed/corrupt packet copy.

**Suggested fix:** Use an atomic swap or semaphore to take ownership of `tmpBuf` before reading.

---

### NET-11 — `bot.c`: `while(1)` makes recv loop unreachable; `netconn` leaked forever 🟡
**File:** sys/net/net/bot.c:70–79
**Severity:** Medium
**Category:** Dead code / resource leak

`while(1);` at line 70 causes an infinite spin. All receive and cleanup code is permanently unreachable. The `netconn` created at line 64 leaks its lwIP PCB for the lifetime of the system.

**Suggested fix:** Remove the spin. Add `netconn_delete(conn)` in cleanup. Consider removing this file entirely.

---

### NET-12 — `shell.c`: `buf` leak from `kmalloc(1500)` overwritten by `netconn_recv`; `buffer` global may be NULL 🟡
**File:** sys/net/net/shell.c:72–79
**Severity:** Medium
**Category:** Use-after-free / null pointer dereference / memory leak

`buf = kmalloc(1500)` at line 72 is immediately orphaned when `netconn_recv(conn, &buf)` at line 75 overwrites `buf` with a new `struct netbuf *`. The 1500-byte allocation leaks. Also, `netbuf_copy(buf, buffer, 1024)` at line 77 uses the global `buffer` pointer, which is NULL until `shell_thread` initializes it at line 98 — but `shell_main` can be reached before that assignment completes.

**Suggested fix:** Remove the `kmalloc(1500)` line; initialize `buf = NULL`. Guard `buffer` against NULL before `netbuf_copy`.

---

### NET-13 — `shell.c`: `buffer[len-2]` with no length guard; underflow when `len < 2` 🟡
**File:** sys/net/net/shell.c:80
**Severity:** Medium
**Category:** Integer underflow / memory corruption

`len` is `uInt32` (unsigned). If `netbuf_len` returns 0 or 1, `len - 2` wraps to `0xFFFFFFFE`/`0xFFFFFFFF`, writing a null terminator gigabytes past the buffer.

**Suggested fix:** Add `if (len < 2) continue;` before computing the null terminator offset.

---

### NET-14 — `udpecho.c`: `netconn_recv` return not checked; NULL `buf` immediately dereferenced 🟡
**File:** sys/net/net/udpecho.c:54–55
**Severity:** Medium
**Category:** Null pointer dereference

`buf = netconn_recv(conn)` is not checked for NULL. A closed/errored connection returns NULL, and `netbuf_fromaddr(buf)` dereferences it immediately.

**Suggested fix:** Add `if (!buf) break;` after `netconn_recv`.

---

### NET-15 — `cond_wait`: lost-wakeup window between arming `lock` and releasing the mutex 🟡
**File:** sys/net/net/sys_arch.c:428
**Severity:** Medium
**Category:** Race condition / incorrect synchronization

`ubcond->lock = TRUE` at line 439 arms the struct that is shared with the signaling side. If `sys_sem_signal` is called between the assignment `ubcond->lock = TRUE` and `ubthread_mutex_unlock(mutex)`, the broadcast clears `lock` before the waiter spins — the waiter then sees `lock == FALSE` immediately and returns without actually waiting. This is a classic lost-wakeup window.

**Suggested fix:** Arm `lock` before releasing the mutex on the signaling side, or use an OS-provided condition variable primitive.

---

### NET-16 — `sys_mbox_post`: `wait_send` decremented outside the full-queue window 🟡
**File:** sys/net/net/sys_arch.c:235–239
**Severity:** Medium
**Category:** Race condition

`wait_send` is incremented before releasing `lock` but decremented after re-acquiring it. Between these two points, a concurrent `sys_mbox_fetch` may signal `empty` based on a stale `wait_send > 0`, waking this thread prematurely when the mailbox is still full.

**Suggested fix:** Decrement `wait_send` immediately before releasing the lock in the retry path, or use a single condvar for the full-queue condition.

---

### NET-17 — `e1000netif.c` `low_level_output`: `tx_scratch` is static — not re-entrant 🟡
**File:** sys/net/netif/e1000netif.c:78
**Severity:** Medium
**Category:** Race condition

`static uint8_t tx_scratch[1518]` is shared across all calls. Safe in the single-tcpip-thread model today, but fragile if any timer callback triggers a retransmit while a send is already in progress.

**Suggested fix:** Make `tx_scratch` a local variable, or protect with the tcpip core lock.

---

### NET-18 — `sys_accept`: negative `*anamelen` not rejected before unsigned cast 🟡
**File:** sys/net/net/sys_arch.c:745
**Severity:** Medium
**Category:** Integer truncation

`unsigned int outlen = (unsigned int)*args->anamelen` — a negative `anamelen` wraps to a huge unsigned value. The copy length is correctly clamped to `kfromlen` (max 28 bytes), so no buffer overrun results, but the syscall silently accepts a semantically invalid argument instead of returning `EINVAL`.

**Suggested fix:** Reject negative `*args->anamelen` with `EINVAL` before the cast.

---

### NET-19 — `sys_arch_timeouts`: non-lwIP callers trigger unconditional `kpanic` 🔵
**File:** sys/net/net/sys_arch.c:501–505
**Severity:** Low
**Category:** Availability

`current_thread()` calls `kpanic("ABORT")` if the calling thread is not registered in the `threads` list. Any unregistered kernel thread that touches an lwIP API panics the system.

**Suggested fix:** Return a static fallback timeout struct and a `kprintf` warning instead of panicking.

---

### NET-20 — `bot.c`/`udpecho.c`: `sys_thread_new` called with obsolete 2-argument signature 🔵
**File:** sys/net/net/bot.c:84, sys/net/net/udpecho.c:65
**Severity:** Low
**Category:** API mismatch / dead code

Current prototype is `sys_thread_new(name, thread, arg, stacksize, prio)` — 5 arguments. Both files call it with 2 arguments. This is a compile error; these files cannot be built as-is.

**Suggested fix:** Update to `sys_thread_new("bot", bot_thread, NULL, 0x1000, 1)` etc., or remove from the build.

---

### NET-21 — `loopif.c`: `netif->input` return value ignored; pbuf leaked on error 🔵
**File:** sys/net/netif/loopif.c:64
**Severity:** Low
**Category:** Ignored error return / memory leak

`netif->input(r, netif)` return value is discarded. On error, lwIP does not take ownership of `r`, so `r` leaks. Compare `e1000netif.c:150–153` which correctly frees `p` on error.

**Suggested fix:** `if (netif->input(r, netif) != ERR_OK) { pbuf_free(r); }`.
