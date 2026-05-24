# ISA Drivers Audit Findings

## Summary
14 findings: 2 critical, 5 high, 5 medium, 2 low

## Findings

### ISA-001 — ne2k ISR calls kmalloc (sleep-capable) from interrupt context
**Severity:** 🔴 Critical
**File:** `sys/isa/ne2k.c:219` (call chain: `ne2kHandler` → `dp_recv` → `dp_pkt2user` → `ne2kAllocBuffer` → `kmalloc`)
**Description:** `ne2kHandler()` runs in ISR context and eventually calls `kmalloc()`. `kmalloc` acquires `mallocSpinLock` via `spinLock()`, which calls `sched_yield()` when contended (`sys/arch/i386/spinlock.c:69`). Calling `sched_yield()` from interrupt context corrupts the scheduler's task-switch state. Even when uncontended, `kmalloc` may call `vmm_getFreeMallocPage()` which can also block.
**Impact:** Kernel crash or silent stack/state corruption on any received NE2000 packet when the malloc spinlock is contended.
**Suggested fix:** Pre-allocate a fixed pool of `nicBuffer` slots at `ne2k_init` time. The ISR copies packet data into a ring of pre-allocated slots and signals a task-context consumer; no `kmalloc` in ISR path.

---

### ISA-002 — fdc.c: waitFdc timeOut counter never decrements — potential infinite busy-loop
**Severity:** 🔴 Critical
**File:** `sys/isa/fdc.c:296–324`
**Description:** `waitFdc()` sets `timeOut = 50000` and spins on `while (!done && timeOut)`. The variable `timeOut` is never decremented inside the loop. The only exit is the floppy ISR setting `done = TRUE`. If the FDC never raises IRQ6 (no floppy present, hardware timeout, QEMU with no floppy image configured), the kernel hangs in this loop permanently.
**Impact:** System lockup on any floppy operation when the FDC does not respond.
**Suggested fix:** Decrement `timeOut` in the loop body: `while (!done && timeOut--)`. Treat `timeOut == 0` as a hard error and return `FALSE`.

---

### ISA-003 — tty_change called from keyboard ISR — 9 KB memcpy in interrupt context
**Severity:** 🟠 High
**File:** `sys/isa/atkbd.c:331`, `sys/kernel/tty.c:105–108`
**Description:** `keyboardHandler()` (ISR context) calls `tty_change()` directly for bare Alt+F1–F4. `tty_change()` does two `memcpy()` calls of 9 600 bytes each (to/from VGA text buffer at `0xB8000`) while holding `atkbdSpinLock`, blocking all other IRQs on the CPU for several milliseconds.
**Impact:** IRQ latency spikes, dropped keyboard/timer/serial interrupts, races with task-context VGA writes (no `tty_spinLock` held).
**Suggested fix:** Mirror the existing Ctrl+Alt+Fn pattern (`vesa_text_slot` deferred flag): set a `pending_tty_switch` variable in the ISR and perform the actual `tty_change()` in task context (PIT scheduler tick or `sys_read` poll loop).

---

### ISA-004 — setLED busy-waits on PS/2 status port with no timeout in ISR context
**Severity:** 🟠 High
**File:** `sys/isa/atkbd.c:372–379`
**Description:** `setLED()` polls `inportByte(0x64) & 2` in two unbounded `while` loops. It is called from `keyboardHandler()` (ISR context, spinlock held) on every caps/num/scroll lock key press. There is no iteration limit or timeout. A slow or stuck PS/2 controller stalls the system indefinitely.
**Impact:** System lockup on any lock-key press if the PS/2 controller stalls. On real hardware with firmware quirks this is a real risk.
**Suggested fix:** Add a retry counter (e.g., 100 000 iterations). If exhausted, skip the LED update. Better: use a deferred task for LED updates rather than doing them in ISR context.

---

### ISA-005 — fdc motorOff never writes to DOR — motor physically never turns off
**Severity:** 🟠 High
**File:** `sys/isa/fdc.c:243–251`
**Description:** `motorOff()` sets `motor = FALSE` in software but both `outportByte(fdcDor, ...)` lines are commented out. The motor-enable bits in the Digital Output Register are never cleared, so the floppy motor stays energized indefinitely. The software `motor` flag says `FALSE` while the hardware is still spinning.
**Impact:** Continuous motor wear on real hardware. Incorrect state causes `motorOn()` to skip its re-enable write on subsequent calls (guarded by `motor == FALSE`), potentially causing command timeouts.
**Suggested fix:** Uncomment `outportByte(fdcDor, 0x0C)` in `motorOff()`. Consider a delayed motor-off timer (2 s after last access) to avoid repeated spin-up delays.

---

### ISA-006 — fdcWrite missing spinlock — concurrent FDC writes are unprotected
**Severity:** 🟠 High
**File:** `sys/isa/fdc.c:359–363`
**Description:** `fdcRead()` acquires `fdcSpinLock` before calling `readBlock()`. `fdcWrite()` calls `writeBlock()` with no lock at all. Concurrent write calls, or a simultaneous read and write, manipulate the shared FDC hardware registers and the DMA buffer at `0x80000` without synchronization.
**Impact:** Corrupted DMA transfers, incorrect sector writes, hardware state-machine confusion causing kernel panic.
**Suggested fix:** Add `spinLock(&fdcSpinLock)` / `spinUnlock(&fdcSpinLock)` around `writeBlock()` in `fdcWrite()`, matching `fdcRead`.

---

### ISA-007 — sysTicks uint32 deadline arithmetic wraps at 248-day uptime
**Severity:** 🟠 High
**File:** `sys/kernel/syscall.c:269–270`, `sys/kernel/descrip.c:426, 490, 557, 621`
**Description:** Deadline calculations use `deadline = systemVitals->sysTicks + N` (uint32). At 200 Hz, `UINT32_MAX` is reached after ≈248.5 days. When `sysTicks` is near `UINT32_MAX`, adding `N` wraps to a small value. Subsequent comparisons `sysTicks < deadline` or `sysTicks >= deadline` evaluate backwards: a just-set deadline looks already expired or never expires.
**Impact:** After long uptime, `sys_nanosleep` returns immediately, and `sys_select`/`sys_poll` timeouts either fire instantly or block forever. All timeout-driven I/O is affected.
**Suggested fix:** Use the standard `time_after` wrap-safe pattern: `(int32_t)(systemVitals->sysTicks - deadline) >= 0` for "has deadline passed". Define a `TICKS_AFTER(a, b)` macro and use it throughout.

---

### ISA-008 — ne2k EOI sent master-first for a slave IRQ — wrong cascade order
**Severity:** 🟡 Medium
**File:** `sys/isa/ne2k.c:205–206`
**Description:** IRQ10 is a slave 8259A interrupt. The correct cascade EOI order is slave first (`outportByte(0xA0, 0x20)`), then master (`outportByte(0x20, 0x20)`). The code does the reverse (master at line 205, slave at line 206). `irq.c:irq_dispatch()` already does this correctly.
**Impact:** On real 8259A hardware, the brief window between master EOI and slave EOI can cause a spurious IRQ2 assertion. Stable under QEMU but incorrect for real hardware.
**Suggested fix:** Swap the lines: slave EOI first, then master EOI.

---

### ISA-009 — fdcRw unbounded recursion on disk-change or timeout
**Severity:** 🟡 Medium
**File:** `sys/isa/fdc.c:168, 206`
**Description:** `fdcRw()` calls itself recursively when a disk-change flag is detected (line 168) and when `waitFdc()` returns timeout (line 206). If the condition persists across multiple calls (no disk present, hardware continuously signals "disk changed"), the recursion is unbounded. Each frame holds several local variables and array pointers on the kernel stack.
**Impact:** Kernel stack overflow → triple fault or memory corruption, reproducible by inserting/removing a floppy disk rapidly or running with no floppy media.
**Suggested fix:** Replace both recursive calls with an iterative outer retry loop. Return failure after a fixed number of retries (e.g., 3).

---

### ISA-010 — ne2kGetBuffer compares struct spinLock to integer 0x1 — type error
**Severity:** 🟡 Medium
**File:** `sys/isa/ne2k.c:375`
**Description:** `ne2kGetBuffer()` tests `if (ne2k_spinLock == 0x1)` where `ne2k_spinLock` is `struct spinLock`. Comparing a struct to an integer is a C type error. GCC either rejects it or compares the struct's address to `0x1` (always false). This guard therefore never fires. Furthermore, `ne2kGetBuffer` accesses `ne2kBuffer` without holding `ne2k_spinLock` at all, racing with `ne2kAllocBuffer` (which does use the lock) called from the ISR.
**Impact:** Race condition on `ne2kBuffer` pointer; can produce use-after-free or double-consumption of the same buffer node.
**Suggested fix:** Replace the comparison with `spinLockLocked(&ne2k_spinLock)` (declared in `spinlock.h`). Wrap the body with `spinLock`/`spinUnlock` to protect `ne2kBuffer` access.

---

### ISA-011 — ne2k ISR re-enables interrupts mid-handler without reentrance guard
**Severity:** 🟡 Medium
**File:** `sys/isa/ne2k.c:208, 229`
**Description:** `ne2kHandler()` issues `asm("sti")` at line 208 (after already sending EOI) before processing packets, then `asm("cli")` at line 229. Because the EOI was already sent, a new NE2000 IRQ can immediately re-enter `ne2kHandler` while the first invocation is still running. The handler is not reentrant: local variable `isr` and the shared `mDev` pointer are accessed without a reentrance guard.
**Impact:** Under network load, re-entrant execution corrupts `isr` (the interrupt status snapshot), causing packets to be processed twice or not at all; potential null-pointer dereference.
**Suggested fix:** Remove the `sti`/`cli` pair. Add a `spinTryLock` guard at the entry of `ne2kHandler` (as `keyboardHandler` and `rs232_handler` do) and return immediately if already locked.

---

### ISA-012 — ne2k ISR trampoline lacks segment register save/restore
**Severity:** 🟡 Medium
**File:** `sys/isa/ne2k.c:52–58`
**Description:** The `ne2kISR` assembly trampoline saves/restores only the general-purpose registers (`pusha`/`popa`) but does not save/restore `%ds`, `%es`, `%fs`, `%gs` or establish kernel data segment selectors before calling `ne2kHandler`. Both `atkbd_isr` and `rs232_isr` save all four segment registers and push `%esp` for the handler argument. Additionally, `mDev` is NULL until `ne2k_init()` runs; if the ISR fires before init (e.g., PCI probe changes IRQ routing), `ne2kHandler` dereferences a null pointer at line 210.
**Impact:** If `ne2kISR` fires while a user-space task is running, `ne2kHandler` executes with user-space segment selectors, causing a GPF or reading wrong memory. The null `mDev` dereference is an unconditional kernel panic.
**Suggested fix:** Add segment register push/pop and kernel `%ds` setup to the trampoline, matching `atkbd_isr`. Add a null-check on `mDev` at the top of `ne2kHandler`.

---

### ISA-013 — pit_init: inconsistent port-delay on PIT counter byte writes
**Severity:** 🔵 Low
**File:** `sys/isa/pit.c:66–67`
**Description:** The low byte of the PIT divisor is written with `outportByteP()` (includes a port-0x80 ISA delay), but the high byte uses `outportByte()` (no delay). Both writes to port `0x40` should be uniform. Some 8254 clones require a brief delay between consecutive counter byte writes.
**Impact:** On real hardware with a slow 8254 clone the high byte may be latched at the wrong phase, producing an incorrect timer frequency. Benign under QEMU.
**Suggested fix:** Change line 67 to `outportByteP(0x40, (((1193180 / PIT_TIMER) >> 8) & 0xFF))` for consistency.

---

### ISA-014 — 8259.c irqEnable_old/irqDisable_old maintain a stale irqMask shadow
**Severity:** 🔵 Low
**File:** `sys/isa/8259.c:33, 67–113`
**Description:** `irqMask` (line 33) is updated only by the `_old` variants. The active `irqEnable`/`irqDisable` functions read the hardware IMR directly and do not touch `irqMask`. The shadow therefore drifts out of sync from the first `irqEnable` call. The `_old` functions are still exported; any driver that mistakenly calls them will program incorrect masks from the stale shadow.
**Impact:** Low risk currently since no active driver calls the `_old` functions. If any new driver accidentally uses `irqEnable_old`, it will silently mask or unmask wrong IRQs.
**Suggested fix:** Remove `irqEnable_old`, `irqDisable_old`, and the `irqMask` global entirely. All callers use the hardware read-modify-write variants.
