# Audit Findings: TTY

## Summary
14 findings: 2 critical, 5 high, 4 medium, 3 low

---

### TTY-1 — tty_change bounds check is off-by-one 🔴
**File:** sys/kernel/tty.c:101
**Severity:** Critical
**Category:** Buffer overflow / out-of-bounds array access

`if (tty > TTY_MAX_TERMS)` should be `>=`. `terms[]` has valid indices 0–4; passing `tty == 5` passes the guard and accesses `terms[5]`, one element past the heap allocation. The ISR path (`atkbd.c:331`) uses `kc & 0xFF`, so a crafted keymap entry targeting slot 5 corrupts heap metadata.

**Suggested fix:** `if (tty >= TTY_MAX_TERMS) kpanic(…);`

---

### TTY-2 — tty_find has no bounds check 🔴
**File:** sys/kernel/tty.c:182–184
**Severity:** Critical
**Category:** Buffer overflow / missing bounds check

`tty_find(uInt16 tty)` returns `&terms[tty]` with no range validation. Any caller with an attacker-influenced index returns a pointer into arbitrary kernel heap memory, enabling a forged `tty_term *` anywhere in kernel address space.

**Suggested fix:** Return `NULL` for `tty >= TTY_MAX_TERMS`; all callers must null-check.

---

### TTY-3 — ISR calls tty_change (memcpy/VGA path) with atkbdSpinLock held 🟠
**File:** sys/isa/atkbd.c:331
**Severity:** High
**Category:** Race condition / unsafe ISR behaviour

`keyboardHandler` calls `tty_change` at line 331 while holding `atkbdSpinLock`. `tty_change` performs two 9600-byte `memcpy`s and updates `tty_foreground` non-atomically. A concurrent `tty_print` on another CPU observes `tty_foreground` mid-update, causing writes through a stale foreground pointer.

**Suggested fix:** Move VTY switching fully to task context using the same deferred-flag pattern already used for Ctrl+Alt+Fn (`vesa_text_slot`). Never call `tty_change` from ISR context.

---

### TTY-4 — tty_inject modifies stdin ring without a lock; reachable from ISR 🟠
**File:** sys/isa/atkbd.c:392; sys/kernel/tty.c:194–269
**Severity:** High
**Category:** Race condition

`tty_inject` writes `tty->stdinSize`, `tty->stdin[]`, `tty->t_linelen`, and `tty->t_linebuf[]` without holding any lock. `sys_read` simultaneously reads and decrements `stdinSize` and shuffles `stdin[]`. A TOCTOU race can corrupt the input ring or underflow `stdinSize` to a large positive value, causing an out-of-bounds read of `stdin[]`.

**Suggested fix:** Protect all accesses to these fields with `tty_spinLock` (or a per-tty lock) in both `tty_inject` and all consumers in `vfs_calls.c`/`descrip.c`.

---

### TTY-5 — getchar() drains stdin[] without any lock 🟠
**File:** sys/isa/atkbd.c:410–416
**Severity:** High
**Category:** Race condition / data corruption

`getchar()` reads and modifies `tty_foreground->stdinSize` and `stdin[]` without holding `tty_spinLock`. A concurrent ISR or serial-path `tty_inject` can decrement `stdinSize` between the zero-check and the decrement, wrapping it to a large value and causing the for-loop to iterate far past the 512-byte `stdin` array.

**Suggested fix:** Disable IRQs or hold a spinlock around the entire read-and-shift block in `getchar()`.

---

### TTY-6 — Canonical newline flush silently drops the '\n' on a full-length line 🟠
**File:** sys/kernel/tty.c:235–238
**Severity:** High
**Category:** Buffer overflow / logic error

The canonical flush loop guards each byte with `stdinSize < 511`, but `stdin` is 512 bytes (indices 0–511). When exactly 511 printable characters fill the buffer the trailing `'\n'` check fails and the newline is silently discarded, causing `sys_read` to block forever.

**Suggested fix:** Use `stdinSize < (sizeof(tty->stdin) - 1)` consistently and hold `tty_spinLock` around the entire flush.

---

### TTY-7 — tty_print scroll region can access past the logical 25-row limit 🟡
**File:** sys/kernel/tty.c:154–163
**Severity:** Medium
**Category:** Buffer overflow (latent)

`tty_buffer` is 9600 bytes (60 rows) but `tty_print` scrolls at the 25-row boundary. If the CRTC cursor Y read at init (`tty_init:71–73`) returns a value > 24 on unusual hardware, the first scroll computes `bufferOffset -= 160` to a value still above 3840, and subsequent writes land past the 25-row mark into the unused heap region. If `tty_buffer` is ever reduced to the correct 4000 bytes, this becomes an immediate overflow.

**Suggested fix:** Clamp the CRTC Y value to `[0, 24]` in `tty_init` and document the 25-row invariant.

---

### TTY-8 — tty_change copies 9600 bytes from/to a 4000-byte VGA framebuffer 🟡
**File:** sys/kernel/tty.c:105–108
**Severity:** Medium
**Category:** Buffer overread / MMIO clobber

The standard VGA text framebuffer is `80 * 25 * 2 = 4000` bytes. Both `memcpy` calls use `80 * 60 * 2 = 9600`, reading/writing 5600 bytes of MMIO space beyond the text buffer, potentially corrupting VGA attribute registers or other MMIO-mapped memory.

**Suggested fix:** Change both sizes to `80 * 25 * 2`.

---

### TTY-9 — falloc and dup2 do not null-check kmalloc return value 🟡
**File:** sys/kernel/descrip.c:130–131, 660
**Severity:** Medium
**Category:** Null pointer dereference

`kmalloc(sizeof(struct file))` is immediately passed to `memset`/`memcpy` without checking for `NULL`. An OOM condition causes an immediate kernel NULL dereference.

**Suggested fix:** Check for `NULL` and return `ENOMEM` before `memset`/`memcpy`.

---

### TTY-10 — getfd() checks the wrong pointer for NULL; also missing fd range check 🟡
**File:** sys/kernel/descrip.c:268–271
**Severity:** Medium
**Category:** Logic error / ineffective null check

```c
*fp = (struct file *)td->o_files[fd];
if (fp == 0x0)   /* fp is a stack address — never NULL */
    error = -1;
```

The check always evaluates false. The intent is `if (*fp == NULL)`. Additionally, `fd` is not validated against `O_FILES` before indexing `td->o_files[fd]`, allowing out-of-bounds array access from any syscall that delegates to `getfd` without its own prior range check.

**Suggested fix:**
```c
int getfd(struct thread *td, struct file **fp, int fd) {
    if (fd < 0 || fd >= O_FILES) { *fp = NULL; return (-1); }
    *fp = (struct file *)td->o_files[fd];
    return (*fp == NULL) ? -1 : 0;
}
```

---

### TTY-11 — tty_print does not validate its term pointer 🔵
**File:** sys/kernel/tty.c:130, 135
**Severity:** Low
**Category:** Null pointer dereference (latent)

`tty_print` dereferences `term->tty_y` immediately at line 135 without a null guard. All current callers are safe, but the echo paths in `tty_inject` (lines 245, 263) pass the already-validated `tty` pointer — if a future refactor introduces a different code path with a possibly-NULL term, this becomes a kernel NULL dereference.

**Suggested fix:** Add `if (term == NULL) return (-1);` at the top of `tty_print`.

---

### TTY-12 — sys_ioctl writes to userland data pointer without validation 🔵
**File:** sys/kernel/descrip.c:285, 335
**Severity:** Low
**Category:** Missing user-pointer validation

`TIOCGETA` and `TIOCSETA` cast `args->data` directly to `struct termios *` without checking for NULL or kernel-space addresses. A NULL or malicious pointer causes a kernel write to an arbitrary address.

**Suggested fix:** Validate `args->data != NULL` before dereferencing; add a user-address range check when address-space isolation is enforced.

---

### TTY-13 — tty_colour initialised with `0x0A + i`; breaks if TTY_MAX_TERMS increases 🔵
**File:** sys/kernel/tty.c:61
**Severity:** Low
**Category:** Logic error / fragile initialisation

Colours `0x0A`–`0x0E` are fine for 5 slots, but incrementing `TTY_MAX_TERMS` beyond 6 would produce attribute byte `0x10` (blinking black-on-black). The serial slot (index 4) also receives a colour value that is never used, creating misleading state.

**Suggested fix:** Use a fixed colour table; clamp to valid VGA attribute range; document which slots are VGA vs serial.

---

### TTY-14 — sys_pread TTY path is dead code with an off-by-one and improper echo 🔵
**File:** sys/kernel/vfs_calls.c:305–365
**Severity:** Low
**Category:** Dead code / logic error

`sys_pread`'s stdin branch reimplements a subset of the VGA input loop using raw `getchar()` and `kprintf()` for echo (bypassing the line discipline), and has an off-by-one: when `x == args->nbyte` and `c == '\n'` both hold, `buf[x++] = '\n'` writes one byte past the caller-supplied buffer. `pread` on a TTY is semantically undefined (POSIX requires `ESPIPE`); this code is almost certainly never exercised.

**Suggested fix:** Return `ESPIPE` for `pread` on a TTY fd and remove the dead input loop.
