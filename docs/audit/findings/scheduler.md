# Audit Findings: Scheduler / Exec

## Summary

18 findings: 4 critical, 6 high, 5 medium, 3 low

---

## Findings

### SCHED-1 — Null pointer write on first `sched()` invocation 🔴
**File:** `sys/arch/i386/sched_switch.c:77`
**Severity:** Critical
**Category:** Null pointer dereference / memory corruption

`_current` is initialised to `0x0` in `sched_core.c:47` and is never written before the first timer ISR fires. On the first `sched()` call, `tmpTask` starts as `NULL` (line 69), falls through to `schedStart`, and iterates `taskList`. When the first `READY` task is found, line 77 executes `_current->state = ... DEAD ... READY` with `_current == NULL`. On i386 the first 4 MB is identity-mapped and writable, so the write silently corrupts the BIOS interrupt vector table at physical address `~offsetof(kTask_t, state)`. The system survives because the overwritten IVT entries happen to be unused at runtime, but the corruption is non-deterministic across kernel builds.

**Suggested fix:** Guard the write: `if (_current != NULL) _current->state = ...;` before `_current = tmpTask;`. Or set `_current = taskList` at the end of `sched_init()`.

---

### SCHED-2 — Use-after-free: `binaryHeader` read after `kfree` in `execFile` 🔴
**File:** `sys/arch/i386/i386_exec.c:515–522`
**Severity:** Critical
**Category:** Use-after-free

`kfree(binaryHeader)` at line 515, then `tmp[0] = binaryHeader->e_entry` at line 522. `kmalloc` may recycle the freed block between those two lines; the new process gets a garbage entry point.

**Suggested fix:** Save `e_entry` to a local before the `kfree` calls, or move the `kfree`s after the `tmp[]` writes.

---

### SCHED-3 — Null deref: `tty_find()` result used unconditionally in `execFile` 🔴
**File:** `sys/arch/i386/i386_exec.c:311–317`
**Severity:** Critical
**Category:** Null pointer dereference

`tty_find(console)` at line 311 can return `NULL`. The error print at line 312–313 does not `return`. Line 317 then writes `newProcess->term->owner` — null dereference → triple fault.

**Suggested fix:** Add `return` (with proper cleanup) in the `term == NULL` branch before the `->owner` write.

---

### SCHED-4 — `sys_exec` error paths wipe process VM then return `-1` 🔴
**File:** `sys/arch/i386/i386_exec.c:688, 712–729`
**Severity:** Critical
**Category:** Unrecoverable process state corruption

`vmm_cleanVirtualSpace(VMM_USER_START)` at line 688 irrevocably destroys all user mappings. If ELF validation fails afterward (lines 712–729), `sys_exec` returns `-1` to the syscall handler — the process now has no user VM, is still running, and has leaked `argv_out`/`args_out`/`envp_out`/`envs_out` kernel buffers. Any return to ring 3 faults immediately.

**Suggested fix:** All ELF validation must happen *before* `vmm_cleanVirtualSpace`. Validate header, magic, `e_type`, and `e_machine` while the original VM is intact.

---

### SCHED-5 — Race: `delList` unprotected between timer ISR and `systemTask` 🟠
**File:** `sys/kernel/sched_core.c:143–163` (writer in ISR) / `sys/arch/i386/systemtask.c:124` (reader)
**Severity:** High
**Category:** Race condition

`sched_addDelTask` (called from `sched()` — the PIT ISR — while holding `schedulerSpinLock`) prepends to `delList`. `sched_getDelTask` (called from `systemTask`, a normal scheduled context) reads `delList` with no lock. A timer interrupt can fire between `sched_getDelTask`'s read of `delList` and its write of `delList = delList->next`, leaving a half-updated list head.

**Suggested fix:** Protect `delList` accesses in `sched_getDelTask` with `schedulerSpinLock` (or a `cli`/`sti` pair for single-CPU correctness).

---

### SCHED-6 — Buffer overflow: unchecked argv/envp total size in `args_copyin` / `envs_copyin` 🟠
**File:** `sys/arch/i386/i386_exec.c:101–113, 132–143`
**Severity:** High
**Category:** Heap buffer overflow (user-controlled)

`args_tmp` is `kmalloc(ARGV_PAGE)` = 256 bytes. The `strcpy` loop accumulates all argument strings into it with no bounds check on `sp`. A caller with combined argv strings > 255 bytes overflows into adjacent kernel heap. Same for `envs_copyin` (256 bytes). This is reachable via the `execve` syscall — user-controlled.

**Suggested fix:** Guard `sp + strlen(arg) + 1 <= ARGV_PAGE` inside the loop; return an error on overflow.

---

### SCHED-7 — Integer overflow in ELF header count `kmalloc` (attacker-controlled) 🟠
**File:** `sys/arch/i386/i386_exec.c:388, 736–748`
**Severity:** High
**Category:** Integer overflow → heap buffer overflow

`kmalloc(sizeof(Elf_Phdr) * binaryHeader->e_phnum)` with no upper bound check. At 65 535 entries the alloc is 2 MB; if the file is crafted to have entries whose offsets point outside the buffer, later dereferences corrupt kernel heap. Same for `e_shnum`.

**Suggested fix:** Reject `e_phnum > 256` and `e_shnum > 1024` before allocating.

---

### SCHED-8 — `execFile` error returns leave zombie task in `taskList` 🟠
**File:** `sys/arch/i386/i386_exec.c:335–382`
**Severity:** High
**Category:** Resource leak / zombie task

After `schedNewTask()` inserts `newProcess` into `taskList` and CR3 has been switched to the new process's page directory (line 324), error paths at lines 335, 345, 363, 371, 378 simply `return` without: (a) calling `sched_setStatus(newProcess->id, DEAD)`, (b) restoring the caller's CR3, or (c) freeing the kernel stack. The task lingers in state `NEW` forever, and CR3 is left pointing at the new (largely empty) page directory.

**Suggested fix:** All failure exits must restore CR3 (`asm("movl %0,%%cr3" :: "r"(kernelPageDirectory))`), mark the task DEAD, and deallocate resources.

---

### SCHED-9 — `remove_wait_queue` infinite loop when entry not in queue 🟠
**File:** `sys/kernel/sched_core.c:219–222`
**Severity:** High
**Category:** Infinite loop (deadlock) inside `cli` section

```c
tmp = wait;
while (tmp->next != wait)
    tmp = tmp->next;
```

If `wait` is not in the circular list (already removed, double-remove, or corruption), `tmp->next` never equals `wait` and the loop spins forever with interrupts disabled — hard-locking the CPU.

**Suggested fix:** Add a lap-detection guard: compare `tmp` to the starting node to detect full traversal without finding `wait`.

---

### SCHED-10 — Unchecked `kmalloc` in `schedNewTask` stdin/stdout/stderr init 🟡
**File:** `sys/kernel/sched_core.c:92–98`
**Severity:** Medium
**Category:** Null pointer dereference

`fp = kmalloc(sizeof(struct file))` return is not checked; `memset(fp, ...)` and `fp->f_flag = ...` immediately follow. Under heap pressure, `fp == NULL` causes a null dereference during task creation.

**Suggested fix:** Check `fp != NULL` after each `kmalloc`; `kpanic` or propagate an error on failure.

---

### SCHED-11 — Unchecked `kmalloc` in `args_copyin` / `envs_copyin` 🟡
**File:** `sys/arch/i386/i386_exec.c:98–101, 129–132`
**Severity:** Medium
**Category:** Null pointer dereference

`argv_tmp` and `args_tmp` are used immediately after allocation without null checks. `argv_tmp[0] = argc` derefs null if the allocation failed.

**Suggested fix:** Null-check each `kmalloc` return; return an error code on failure.

---

### SCHED-12 — `interp` buffer: unchecked `kmalloc`, never freed 🟡
**File:** `sys/arch/i386/i386_exec.c:885–891`
**Severity:** Medium
**Category:** Null dereference + memory leak

`interp = kmalloc(programHeader[i].p_filesz)` is unchecked; `fread(NULL, ...)` and `ldEnable(NULL)` would follow on failure. `interp` is never `kfree`'d — every dynamic-linked exec leaks this buffer.

**Suggested fix:** Null-check `interp`; add `kfree(interp)` before all return paths.

---

### SCHED-13 — `sectionHeader` allocated, read, but never used or freed in `sys_exec` 🟡
**File:** `sys/arch/i386/i386_exec.c:747–753`
**Severity:** Medium
**Category:** Memory leak / dead code

`sectionHeader` is fully populated from the ELF file but never referenced again. It is never freed — each `execve` leaks `sizeof(Elf_Shdr) * e_shnum` bytes permanently.

**Suggested fix:** Either use it or remove the allocation.

---

### SCHED-14 — `ef` struct allocated in `sys_exec` but never freed 🟡
**File:** `sys/arch/i386/i386_exec.c:756–757`
**Severity:** Medium
**Category:** Memory leak

`ef = kmalloc(sizeof(struct elf_file))` is never freed on any code path.

**Suggested fix:** Add `kfree(ef)` before all `sys_exec` return paths.

---

### SCHED-15 — `schedEndTask` silently ignores its `pid` parameter 🔵
**File:** `sys/arch/i386/sched_switch.c:137–140`
**Severity:** Low
**Category:** Dead code / logic error

`schedEndTask(pid)` calls `endTask(_current->id)` — the `pid` argument is never used. Any caller that passes a different PID will kill itself instead of the intended target.

**Suggested fix:** Use `endTask(pid)` or remove the parameter and rename to `schedEndCurrentTask()`.

---

### SCHED-16 — ELF magic check skips byte 0 (`EI_MAG0 = 0x7F`); no `e_machine` check 🔵
**File:** `sys/arch/i386/i386_exec.c:360–363, 708–711`
**Severity:** Low
**Category:** Incomplete validation

Only `e_ident[1..3]` are checked. `e_ident[0]` (`0x7F`) is not verified. Neither `e_machine` (EM_386) nor `e_ident[EI_CLASS]` (ELFCLASS32) are validated. A 64-bit or ARM ELF binary passes all checks and is loaded as i386.

**Suggested fix:** Add `e_ident[0] == ELFMAG0`, `e_ident[EI_CLASS] == ELFCLASS32`, and `e_machine == EM_386` checks.

---

### SCHED-17 — `int x` vs `uint32_t p_memsz` loop in `execFile` risks infinite loop 🔵
**File:** `sys/arch/i386/i386_exec.c:404–419, 432–445`
**Severity:** Low
**Category:** Integer overflow

`x` is `int`; the loop condition `x < programHeader[i].p_memsz` promotes `x` to unsigned. When `p_memsz >= 0x80000000`, a wrapping signed `x` always compares less than `p_memsz` under unsigned promotion, creating an infinite loop. `sys_exec` avoids this via `round_page()` but `execFile` does not.

**Suggested fix:** Declare `x` as `uint32_t` in `execFile`, or apply `round_page()` and cap `p_memsz`.

---

### SCHED-18 — `schedFindTask` result unchecked before dereference in `systemTask` 🔵
**File:** `sys/arch/i386/systemtask.c:67`
**Severity:** Low
**Category:** Null pointer dereference

`schedFindTask(myMsg.pid)->term = tty_find(*x)` — if no task matches the PID (stale or crafted MPI message), `schedFindTask` returns `NULL` and `->term` faults.

**Suggested fix:** Store the return value, check for `NULL` before dereferencing.
