# VFS / Syscalls Audit Findings

## Summary
17 findings: 3 critical, 5 high, 6 medium, 3 low

---

## Findings

### VFS-001 — `getfd()` does not bounds-check `fd` before array access
**Severity:** 🔴 Critical
**File:** `sys/kernel/descrip.c:268`
**Description:** `getfd()` dereferences `td->o_files[fd]` without first checking that `fd` is in the range `[0, O_FILES)`. A negative fd or a value >= 512 (O_FILES) causes an out-of-bounds read/write of the `o_files` array. Additionally, the NULL check `if (fp == 0x0)` is logically wrong: `fp` is a stack pointer passed by the caller and will never be NULL — the intent is `if (*fp == 0x0)`.
**Impact:** A malicious or buggy userland program supplying a large or negative fd to any syscall that calls `getfd()` (read, write, close, lseek, fchdir, select, poll…) can read or corrupt kernel task-structure memory adjacent to `o_files`, leading to kernel panic or privilege escalation.
**Suggested fix:**
```c
int getfd(struct thread *td, struct file **fp, int fd)
{
    if (fd < 0 || fd >= O_FILES) {
        *fp = NULL;
        return (-1);
    }
    *fp = (struct file *)td->o_files[fd];
    return (*fp == NULL ? -1 : 0);
}
```

---

### VFS-002 — `sys_read()` and `sys_pread()` dereference `fd` without NULL check for regular files
**Severity:** 🔴 Critical
**File:** `sys/kernel/vfs_calls.c:227`, `sys/kernel/vfs_calls.c:318`
**Description:** In `sys_read`, the `else if (args->fd > 3)` branch calls `fread(args->buf, 1, args->nbyte, fd->fd)` without checking whether `fd` (the result of `getfd`) is NULL. If `getfd` returns NULL (e.g. the slot was freed between calls, or fd == 4 but not open), this is a NULL dereference. The same pattern exists in `sys_pread`: `fd->fd->offset` is dereferenced without a guard.
**Impact:** Kernel panic / triple fault from NULL dereference in any process that passes a closed or uninitialized fd > 3 to `read()` or `pread()`.
**Suggested fix:** After `getfd()`, add `if (fd == NULL) { td->td_retval[0] = -1; return (-1); }` before any branch that uses `fd`.

---

### VFS-003 — `fwrite()` calls `vfsWrite` without checking for NULL function pointer
**Severity:** 🔴 Critical
**File:** `sys/fs/vfs/file.c:434`
**Description:** `fwrite()` calls `fd->mp->fs->vfsWrite(...)` without verifying that `vfsWrite` is non-NULL. `fputc()` at line 479 has the same issue. Not every filesystem driver must implement `vfsWrite` (the comment in `vfs.h` says "not sure if we should allow function to point to NULL"), so a read-only filesystem will cause a NULL function-pointer call.
**Impact:** Kernel panic on any write to a filesystem that did not register `vfsWrite`.
**Suggested fix:**
```c
if (fd->mp->fs->vfsWrite == NULL)
    return (0);
res = fd->mp->fs->vfsWrite(fd, ptr, fd->offset, size * nmemb);
```
Apply the same guard to `fputc`, `vfsUnlink` in `unlink()`, and `vfsMakeDir` in `sysMkDir()`.

---

### VFS-004 — `sys_close()` pipe path: use-after-free after `fdestroy`, and possible double-`fdestroy`
**Severity:** 🟠 High
**File:** `sys/kernel/vfs_calls.c:124–140`
**Description:** In the pipe close path (`fd_type == 3`), `fdestroy(td, fd, args->fd)` is called when the refcount is < 2, freeing `fd`. The code then unconditionally decrements `pFD->rfdCNT--` / `pFD->wfdCNT--` — but `pFD` was read through `fd->data`, and `fd` is now freed (use-after-free). Also, if `args->fd` matches both `pFD->rFD` and `pFD->wFD`, `fdestroy` is called twice for the same slot (double-free).
**Impact:** Heap corruption or kernel panic from use-after-free or double-free of kernel pipe state.
**Suggested fix:** Read `pFD = fd->data` before calling `fdestroy`, set `fd = NULL` after, and restructure the rFD/wFD checks to ensure `fdestroy` is called at most once per `sys_close`.

---

### VFS-005 — `sys_write()` pipe path: `kmalloc` result not checked before use
**Severity:** 🟠 High
**File:** `sys/kernel/vfs_calls.c:395–396`
**Description:**
```c
pBuf = (struct pipeBuf *)kmalloc(sizeof(struct pipeBuf));
pBuf->buffer = kmalloc(uap->nbyte);
```
Neither allocation is checked for NULL. If `kmalloc` fails under OOM, dereferencing `pBuf` or `pBuf->buffer` causes a NULL pointer dereference kernel panic.
**Impact:** Kernel panic under memory pressure any time a process writes to a pipe.
**Suggested fix:**
```c
pBuf = kmalloc(sizeof(struct pipeBuf));
if (pBuf == NULL) { td->td_retval[0] = -1; return (-1); }
pBuf->buffer = kmalloc(uap->nbyte);
if (pBuf->buffer == NULL) { kfree(pBuf); td->td_retval[0] = -1; return (-1); }
```

---

### VFS-006 — `sys_write()` stdout/stderr path: no NULL check on `kmalloc` result
**Severity:** 🟠 High
**File:** `sys/kernel/vfs_calls.c:418`, `sys/kernel/vfs_calls.c:440`
**Description:** Both the `fd == 2` and `fd == 1` branches allocate `buffer = kmalloc(uap->nbyte + 1)` then immediately call `memset` and `memcpy` without checking for NULL. Under memory pressure this is a NULL dereference.
**Impact:** Kernel panic whenever stdout/stderr write coincides with an OOM condition.
**Suggested fix:** Add `if (buffer == NULL) { td->td_retval[0] = -1; return (-1); }` after each `kmalloc`.

---

### VFS-007 — `sys_select()` timeout: `tv_sec * 1000` overflows `uint32_t` on large values
**Severity:** 🟠 High
**File:** `sys/kernel/descrip.c:424`
**Description:**
```c
uint32_t ms = (uint32_t)(args->tv->tv_sec * 1000 + args->tv->tv_usec / 1000);
deadline = systemVitals->sysTicks + ms * PIT_TIMER / 1000 + 1;
```
`args->tv->tv_sec` is a signed `long`; multiplying by 1000 before casting overflows for values > ~2 million seconds. The resulting `deadline` may be less than `sysTicks`, causing `select()` to return immediately instead of blocking.
**Impact:** `select()` with large timeouts returns immediately (silent incorrect behavior). Daemons using long-timeout selects spin at full CPU.
**Suggested fix:** Clamp `tv_sec` to a reasonable maximum (e.g. 3600 s) or use 64-bit arithmetic for the deadline calculation.

---

### VFS-008 — `sys_select()` silently drops non-socket, non-stdin read fds (e.g. pipes)
**Severity:** 🟠 High
**File:** `sys/kernel/descrip.c:383–398`
**Description:** The read-fd scan in `sys_select` handles only `fd == 0` (stdin) and `fd_type == 2` (socket). Any other fd (regular file, pipe, directory) is silently removed from the result `args->in`. A `select()` on a pipe read end will block forever even when data is available.
**Impact:** Programs that use `select()` on pipe fds hang indefinitely.
**Suggested fix:** Add a case for `fd_type == 3` (pipe): check `((struct pipeInfo *)f->data)->bCNT > 0` and mark ready in the result. Regular files should always be marked ready for reading.

---

### VFS-009 — `fopen()` calls `spinUnlock` in error path that never acquired the lock
**Severity:** 🟡 Medium
**File:** `sys/fs/vfs/file.c:615`
**Description:** In the `kmalloc(4096)` failure path for `tmpFd->buffer`:
```c
spinUnlock(&fdTable_lock);   /* lock was never acquired here */
```
The `spinLock(&fdTable_lock)` at line 628 comes *after* this check. Calling `spinUnlock` without a preceding `spinLock` corrupts the spinlock state.
**Impact:** Under memory pressure the global `fdTable_lock` is corrupted, causing the kernel to deadlock or panic on the next `fopen`/`fclose` call.
**Suggested fix:** Remove the `spinUnlock` call from the buffer-allocation failure path.

---

### VFS-010 — `sysMkDir()` dereferences `fopen()` return without NULL check
**Severity:** 🟡 Medium
**File:** `sys/fs/vfs/file.c:741`
**Description:**
```c
tmpFD = fopen(rootPath, "rb");
if (tmpFD->mp == 0x0) {   /* NULL deref if tmpFD itself is NULL */
```
`fopen` can return NULL. The check is on `tmpFD->mp`, not on `tmpFD` itself.
**Impact:** `sys_mkdir` on a path where the parent does not yet exist causes a kernel NULL dereference panic.
**Suggested fix:**
```c
tmpFD = fopen(rootPath, "rb");
if (tmpFD == NULL || tmpFD->mp == NULL) {
    kprintf("sysMkDir: invalid mount point for %s\n", rootPath);
    return;
}
```

---

### VFS-011 — `sys_fchdir()` uses aliased `sprintf` and format-string injection via filename
**Severity:** 🟡 Medium
**File:** `sys/fs/vfs/file.c:314`, `sys/fs/vfs/file.c:317`
**Description:**
```c
sprintf(_current->oInfo.cwd, "%s%s", _current->oInfo.cwd, fd->fileName);
/* ... */
sprintf(_current->oInfo.cwd, fd->fileName);  /* format-string injection */
```
The first call has `cwd` as both destination and source argument — undefined behavior on overlapping buffers. The second uses `fd->fileName` directly as a format string, enabling format-string injection via a crafted filename.
**Impact:** Potential memory corruption (aliased sprintf) and arbitrary kernel memory read/write via format-string exploit.
**Suggested fix:** Use a temporary buffer and `snprintf(..., "%s", fd->fileName)` in both cases.

---

### VFS-012 — `unlink()` uses `strtok` on a `const char *`, mutating the caller's string
**Severity:** 🟡 Medium
**File:** `sys/fs/vfs/file.c:763`
**Description:**
```c
path = (char *)strtok((char *)node, "@");
```
`strtok` modifies its argument in place. Casting away `const` and passing the caller's string to `strtok` mutates the caller's buffer (undefined behavior), and panics if the string is in read-only memory.
**Impact:** Corruption of the path argument; kernel panic if the string is read-only.
**Suggested fix:** Copy `node` into a local `char work[1024]` buffer before calling `strtok`.

---

### VFS-013 — `MAX_FILES` (256) vs `O_FILES` (512): select/poll miss upper fd slots
**Severity:** 🟡 Medium
**File:** `sys/kernel/descrip.c:373`, `sys/kernel/descrip.c:535`
**Description:** `MAX_FILES` is 256 while `O_FILES` is 512. `sys_select` clamps `nd` to `MAX_FILES` and sizes its `kern_to_lwip_r/w` arrays to `MAX_FILES`. Socket fds allocated in slots 256–511 are invisible to `select`/`poll`.
**Impact:** Silent missed readiness for high-numbered fds; processes with many open files have incorrect select/poll results.
**Suggested fix:** Unify both constants to the same value, or replace both with a single `NOFILE` macro used consistently everywhere.

---

### VFS-014 — `dup2()` bounds check off-by-one: allows `to == MAX_FILES`
**Severity:** 🟡 Medium
**File:** `sys/kernel/descrip.c:637`
**Description:**
```c
if (to > MAX_FILES)   /* should be >= */
```
`to == MAX_FILES` (256) passes the check. Valid indices are `0..MAX_FILES-1`. Slot 256 can be written but is never visible to other code that uses `MAX_FILES` as the exclusive upper bound.
**Impact:** Off-by-one; dup2 into slot 256 leaks a file struct until task exit.
**Suggested fix:** Change to `if (to >= MAX_FILES)`.

---

### VFS-015 — `sys_getdirentries()` writes to unvalidated user pointer `args->buf`
**Severity:** 🟡 Medium
**File:** `sys/kernel/vfs_calls.c:499–530`
**Description:** `args->buf` is a raw user-supplied pointer. The kernel writes directory entries directly into it without checking that it is non-NULL or within user address space. A NULL or kernel-space pointer corrupts kernel memory.
**Impact:** A userland program passing NULL or a kernel address as the `getdirentries64` buffer corrupts kernel memory.
**Suggested fix:** Validate `args->buf != NULL` and that `args->buf + args->count` does not exceed the user-space boundary. At minimum: `if (args->buf == NULL) { td->td_retval[0] = EFAULT; return (EFAULT); }`.

---

### VFS-016 — `fcntl` cmd=17: `dup++` then `fclose` immediately decrements it; dangling pointer in dup
**Severity:** 🔵 Low
**File:** `sys/kernel/descrip.c:84–91`
**Description:**
```c
fp->fd->dup++;
fclose(fp->fd);      /* net: dup back to 0, may free fileDescriptor_t */
fdestroy(td, fp, uap->fd);
```
`dup` is incremented then immediately decremented by `fclose`, so the underlying `fileDescriptor_t` may be freed. The newly-allocated `dup_fp` inherits `fp->fd` which is now a dangling pointer.
**Impact:** Use-after-free of the underlying `fileDescriptor_t`; the duplicated fd holds a dangling `fd` pointer.
**Suggested fix:** Remove the `fclose` call — the slot is destroyed by `fdestroy` and the dup inherits the reference. The `dup` increment is the correct way to transfer ownership.

---

### VFS-017 — `sys_close()` `kprintf` missing format specifier for `args->fd`
**Severity:** 🔵 Low
**File:** `sys/kernel/vfs_calls.c:107`
**Description:**
```c
kprintf("COULDN'T FIND FD: ", args->fd);
```
No `%i` in the format string; `args->fd` is passed but never printed.
**Impact:** Misleading kernel log output when close is called with an invalid fd.
**Suggested fix:** `kprintf("COULDN'T FIND FD: %i\n", args->fd);`

---

### VFS-018 — Dead `return (0x0)` at end of `fopen()` is unreachable
**Severity:** 🔵 Low
**File:** `sys/fs/vfs/file.c:650`
**Description:** The final `return (0x0)` at the end of `fopen` is unreachable because the preceding `if/else` already returns in both branches.
**Impact:** No runtime impact; dead code.
**Suggested fix:** Remove the unreachable `return`.

---

## Scope Notes

- **User pointer validation** is broadly absent across all syscalls (`args->buf`, `args->path`, `args->data` in ioctl, `args->fds` in poll, `args->tv` in select, etc.). These are not individually listed above because UbixOS currently lacks a user-address validation primitive. A single `copyin()`/`copyout()` helper implementation would address the entire class at once.
- **`sys_access()`** always returns 0 (success) — any file appears accessible. Documented with `XXX` but is a security concern if multi-user enforcement is ever added.
- **`sys_rename()`** is a stub that always returns 0 without performing any rename. Callers silently believe the rename succeeded.
