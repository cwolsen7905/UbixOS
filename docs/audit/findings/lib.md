# Audit Findings: Kernel Lib

## Summary

19 findings: 3 critical, 6 high, 7 medium, 3 low

---

## Findings

### LIB-1 — kprintf fixed 512-byte stack buffer can overflow 🔴
**File:** sys/lib/kprintf.c:317–322
**Severity:** Critical
**Category:** Buffer overflow

`kprintf` formats the entire message into `char buf[512]` via `kvprintf`. The `PCHAR` macro does `*d++ = cc` with no bounds check against `buf`. Any formatted output longer than 511 bytes silently overwrites adjacent stack memory, corrupting the return address of the calling function.

**Suggested fix:** Refactor `kvprintf` to accept a `maxlen` output-context (an `{char *buf; size_t rem;}` struct) so `PCHAR` stops writing at the limit. This fixes LIB-1, LIB-2, and LIB-3 simultaneously.

---

### LIB-2 — vsprintf has no length limit — unbounded writes to caller buffer 🔴
**File:** sys/lib/vsprintf.c:135–291 (called from sys/arch/i386/kpanic.c:47)
**Severity:** Critical
**Category:** Buffer overflow

`vsprintf` writes directly to the caller's buffer with no bounds check anywhere in the function or in the `number()` helper. `kpanic` allocates `char buf[512]` on the stack and calls `vsprintf(buf, fmt, args)`. A panic message with a long path or task name overflows that buffer, potentially crashing the machine before the panic handler can halt it.

**Suggested fix:** Replace `vsprintf` with a length-aware `vsnprintf` and update `kpanic` to call it. Alternatively, route `kpanic` through the existing `kvprintf`/`snprintf` path already available in `kprintf.c`.

---

### LIB-3 — snprintf uses an unguarded 2048-byte internal staging buffer 🔴
**File:** sys/lib/kprintf.c:347–361
**Severity:** Critical
**Category:** Buffer overflow

`snprintf` allocates `char tmp[2048]` on the stack and passes it to `kvprintf` with no length limit. If the formatted result exceeds 2047 bytes, `tmp` overflows before `snprintf` even truncates into `buf`. The 2048-byte ceiling is arbitrary and `kvprintf` can exceed it for wide format strings.

**Suggested fix:** Pass `size` into `kvprintf` via an output-context struct so `PCHAR` stops at the limit. Remove the `tmp` staging buffer.

---

### LIB-4 — `uInt16 pages` truncates large allocations, corrupting VMM page count 🟠
**File:** sys/lib/kmalloc.c:185, 246, 262, 267
**Severity:** High
**Category:** Integer overflow / truncation

`pages` is declared `uInt16` (16-bit). For any allocation above `65535 × 4096 = 268,369,920` bytes the page count wraps to a small value. `vmm_getFreeMallocPage` is asked for far fewer pages than needed, the returned region is too small, and the subsequent `memset(tmpDesc1->baseAddr, 0x0, tmpDesc1->limit)` at line 273 writes zeros far beyond the allocated region — a kernel heap overflow.

**Suggested fix:** Change `pages` to `uInt32`. Add an explicit overflow guard: `if (len > (uInt32)(65535u * 4096u)) kpanic("kmalloc: request too large\n");`.

---

### LIB-5 — MALLOC_ALIGN macro wraps to near-zero on near-UINT32_MAX inputs 🟠
**File:** sys/include/lib/kmalloc.h:40 (used at sys/lib/kmalloc.c:189)
**Severity:** High
**Category:** Integer overflow

`MALLOC_ALIGN(size)` expands to `size + (32 - size%32)`. For `size` in `[0xFFFFFFE1, 0xFFFFFFFF]` the addition overflows and wraps to a tiny value (0–30). The `if (len == 0x0)` check at line 191 only catches exact zero; wrapped values like 16 pass through, causing `kmalloc` to allocate and return a pointer to a 16-byte block for a request the caller believes is much larger.

**Suggested fix:** Before `MALLOC_ALIGN`, check `if (size > UINT32_MAX - MALLOC_ALIGN_SIZE) kpanic(...)`.

---

### LIB-6 — Deadlock: `getEmptyDesc` acquires `emptyDescSpinLock` while `mallocSpinLock` is held 🟠
**File:** sys/lib/kmalloc.c:55–97, 214, 239, 264
**Severity:** High
**Category:** Race condition / deadlock

`kmalloc` acquires `mallocSpinLock` (line 187), then calls `getEmptyDesc` at lines 214, 239, and 264. `getEmptyDesc` unconditionally acquires `emptyDescSpinLock` (line 60). If a second CPU simultaneously enters `getEmptyDesc` holding `emptyDescSpinLock` and is waiting for `mallocSpinLock`, the two locks form a cycle. On a uniprocessor kernel with interrupt-driven tasks, any interrupt handler that calls `kmalloc` while the base context holds `mallocSpinLock` spins forever.

**Suggested fix:** Merge the two locks into a single `mallocSpinLock` used for all descriptor list accesses. Bracket `kmalloc`/`kfree` with `cli`/`sti` to prevent IRQ re-entrancy.

---

### LIB-7 — `returnEmptyDesc` modifies `emptyKernDesc` without holding `emptyDescSpinLock` 🟠
**File:** sys/lib/kmalloc.c:101–110, 157, 168
**Severity:** High
**Category:** Race condition

`insertFreeDesc` (called from `kmalloc`/`kfree` under `mallocSpinLock`) calls `returnEmptyDesc`, which modifies `emptyKernDesc` without taking `emptyDescSpinLock`. On SMP, a concurrent CPU in `getEmptyDesc` holding only `emptyDescSpinLock` races with this write, corrupting the empty descriptor list.

**Suggested fix:** Same as LIB-6 — unify under a single lock.

---

### LIB-8 — `new_trieNode` dereferences unchecked `kmalloc` return value 🟠
**File:** sys/lib/kern_trie.c:38–45
**Severity:** High
**Category:** Null pointer dereference

`kmalloc` can return NULL on OOM. `new_trieNode` writes `node->isLeaf = 0` immediately without checking. If `kmalloc` returns NULL, the write targets address 0x0, which is unmapped, triggering a page fault and triple-fault.

**Suggested fix:** `if (node == NULL) kpanic("new_trieNode: kmalloc failed\n");` immediately after the allocation.

---

### LIB-9 — Trie index uses raw char arithmetic without range check — array out-of-bounds 🟠
**File:** sys/lib/kern_trie.c:59–65, 88–90, 117
**Severity:** High
**Category:** Buffer overflow / out-of-bounds array access

`character[CHAR_SIZE]` has 26 elements. The index `*str - 'a'` is used without validation. On i386 `char` is signed; any byte outside `['a'..'z']` produces a negative index or an index ≥ 26. For example `'A'` (0x41) gives index -32; `0x80` gives -97. These access memory far before or after the struct on the heap. Any caller passing uppercase letters, digits, path separators, or non-ASCII bytes causes heap corruption.

**Suggested fix:** Add `if (*str < 'a' || *str > 'z') { /* error or skip */ }` before using `*str - 'a'` as an index.

---

### LIB-10 — `strtol` never reads the first character — broken parsing 🟡
**File:** sys/lib/strtol.c:41–66
**Severity:** Medium
**Category:** Logic error / incorrect behavior

The `isspace` skip loop is commented out and `c` is initialized to `0x0`. The sign check `if (c == '-')` tests this zero byte, not the first character of `nptr`. The loop eventually reads from `nptr` starting at `s`, but because sign and base detection ran against `c == 0`, any non-zero leading character is treated as the unknown character class and the loop breaks immediately, returning 0. `strtol("16", NULL, 10)` returns 0. The `inet_aton` in `sys/lib/net.c:80` calls `strtol` and would produce wrong IP addresses.

**Suggested fix:** Uncomment the whitespace-skip loop and initialize `c = *s++` before the sign check.

---

### LIB-11 — `vsprintf` `%s` with NULL argument faults without protection 🟡
**File:** sys/lib/vsprintf.c:232–245
**Severity:** Medium
**Category:** Null pointer dereference

The `'s'` case calls `strlen(s)` directly without a NULL check. `kvprintf` (kprintf.c:615) substitutes `"(null)"` for NULL `%s` arguments; `vsprintf` does not. Any call to `vsprintf` with a NULL `%s` argument — including `kpanic` during error paths — triple-faults.

**Suggested fix:** Add `if (s == NULL) s = "(null)";` before `strlen(s)`.

---

### LIB-12 — `kvprintf` returns 0 from unreachable tail — wrong character count if reached 🟡
**File:** sys/lib/kprintf.c:752
**Severity:** Medium
**Category:** Logic error / incorrect return value

The function has a `return (0)` after `#undef PCHAR`. The only correct exit is via `return (retval)` inside the loop at line 432. If this tail were reached, `kprintf` would null-terminate `buf` at position 0, silently dropping all output.

**Suggested fix:** Change `return (0)` to `return (retval); /* NOTREACHED */`.

---

### LIB-13 — `strtok` uses a static last-pointer — not safe across task switches 🟡
**File:** sys/lib/strtok.c:83–87
**Severity:** Medium
**Category:** Race condition

`strtok` stores parse state in `static char *last`. A context switch between two tasks both calling `strtok` (even on the same CPU) corrupts both parse sessions. The safe `strtok_r` is available in the same file.

**Suggested fix:** Remove `strtok` or add a `kpanic` in its body to force callers to use `strtok_r`.

---

### LIB-14 — `atan` and `sqrt` are stub implementations returning input unchanged 🟡
**File:** sys/lib/atan.c:31, sys/lib/sqrt.c:29
**Severity:** Medium
**Category:** Silent incorrect behavior

Both functions return their argument unchanged. Any code path that calls `sqrt(x)` receives `x`, not `√x`. Silent wrong results in any algorithm depending on these functions, including potential use by lwIP optional paths.

**Suggested fix:** Implement correct versions or add `kpanic("sqrt/atan: not implemented\n")` bodies to catch accidental calls during development.

---

### LIB-15 — `__udivdi3` and `__divdi3` return 0 for all 64-bit division 🟡
**File:** sys/lib/divdi3.c:31–37
**Severity:** Medium
**Category:** Silent incorrect behavior

GCC emits calls to `__udivdi3`/`__divdi3` for every 64-bit `/` and `%` operator on i386. Both stubs return 0. All 64-bit division in the kernel — timer arithmetic, disk byte offsets, network sequence numbers — silently returns 0. The correct `__qdivrem` implementation exists in `kprintf.c` but is never called from these stubs.

**Suggested fix:** Implement `__udivdi3(a, b) { return __qdivrem(a, b, NULL); }` using the existing `__qdivrem`. Implement `__divdi3` with sign handling. Remove the stubs.

---

### LIB-16 — `kfree` does not detect double-free via free list scan 🔵
**File:** sys/lib/kmalloc.c:285–320
**Severity:** Low
**Category:** Double-free (silent)

When `kfree` is called on a pointer not found in `usedKernDesc`, it logs a message but does not check `freeKernDesc`. A double-free will silently re-insert an already-free descriptor, corrupting free-list linkage without panic.

**Suggested fix:** After failing to find the pointer in `usedKernDesc`, scan `freeKernDesc`; if found, `kpanic("kfree: double-free 0x%X\n", baseAddr)`.

---

### LIB-17 — `kern_trie.h` header guard closing comment has typo 🔵
**File:** sys/include/lib/kern_trie.h:51
**Severity:** Low
**Category:** Code quality

`#endif _LIB_LKERN_TRIE_H_` (extra `L`) does not match `#ifndef _LIB_KERN_TRIE_H_`. Informational only but misleading.

**Suggested fix:** `#endif /* _LIB_KERN_TRIE_H_ */`.

---

### LIB-18 — `kfree` O(n) scan holds spinlock for unbounded time 🔵
**File:** sys/lib/kmalloc.c:295–316
**Severity:** Low
**Category:** Latency / performance

`kfree` walks the entire `usedKernDesc` list under `mallocSpinLock`. With many live allocations, IRQ-disabled hold time grows linearly with allocation count.

**Suggested fix:** Index descriptors by base address in a hash table for O(1) lookup.

---

## Cross-cutting Notes

1. **Root cause of LIB-1/2/3:** `kvprintf` has no length parameter. One fix — a `{char *p; size_t rem;}` context struct used by `PCHAR` — closes all three criticals.
2. **Two overlapping printf implementations** (`kvprintf` in `kprintf.c` and `vsprintf` in `vsprintf.c`) with different safety properties increase maintenance risk. Consolidate on `kvprintf`.
3. **`strtol` is effectively broken** (LIB-10). All numeric parsing that uses it returns 0 silently. Treat as High in any subsystem that parses user-supplied numbers through this path.
4. **`__divdi3`/`__udivdi3` stubs** (LIB-15) affect every 64-bit division in the entire kernel. The fix is two lines using existing code.
