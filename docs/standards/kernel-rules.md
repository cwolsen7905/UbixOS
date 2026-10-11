# Kernel Rules

For code under `sys/` (and `sys/arch/*`). These are in addition to
[coding.md](coding.md). Rule IDs `K-n`.

## No FP, SIMD or MMX in the kernel

- **K-1** The kernel never saves/restores vector/FP state on entry. Kernel
  objects are built with `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow` on x86_64 and
  with FP/NEON disabled on aarch64 (`-mgeneral-regs-only`, see
  `share/mk/ubix.target.*.mk`); x86_64 also builds `-mno-red-zone`, so kernel
  code and interrupt handlers may not use the red zone. No `float`/`double`
  in kernel code, and beware compiler-generated vector code for struct copies,
  `memcpy` inlining and division — a stray XMM instruction is `#UD` (x86) or an
  FP-trap (aarch64) at the worst possible time.
- **K-2** Third-party code integrated into the kernel (lwIP, etc.) is built
  with the same flags; a library that insists on SSE/NEON is not suitable.

## Stacks and recursion

- **K-10** The ring-0 / EL1 stack is 8 KiB per task. No VLAs, no recursion, no
  buffers over ~256 bytes on the stack — use `kmalloc` or a per-task/static
  buffer. (A 4 KiB stack was overflowed by FAT `chdir`; it corrupted
  `sig_pending`.)
- **K-11** Interrupt/exception handlers use only what is documented safe in
  interrupt context: no `kmalloc`, no sleeping, no taking a lock that thread
  context can hold with interrupts enabled.

## Locking and concurrency

- **K-20** Know your context before you touch shared state: boot, thread,
  syscall (cooperative, IRQ-masked on aarch64), interrupt, or SMP AP. State
  the locking requirement in the function's Doxygen block ("caller holds X",
  "takes X", "context: thread only").
- **K-21** Every shared structure has one named lock (or is per-CPU, or is
  explicitly single-writer and says so). Lock order is global and documented
  where the second lock is taken; never take a lock you already hold.
- **K-22** Do not sleep, call into the scheduler, or call a function that may,
  while holding a spinlock or with interrupts masked — unless the function is
  documented as the sanctioned way to do so (e.g. `sched_*` wait paths).
- **K-23** Data shared between interrupt and thread context needs the lock
  *and* correct interrupt masking around it. `volatile` is not a lock. On
  SMP, ring buffers shared across contexts use explicit barriers.
- **K-24** Wakeup channels are real sleep/wakeup (`sleep`/`wakeup` style)
  — never `while (!flag) ;` busy-waits on a flag set elsewhere, except in
  early boot with a documented bound.
- **K-25** Run-queue manipulation: a READY task must be dequeued and
  re-enqueued to change priority; changing it in place corrupts the buckets.
  Use the sched API, never poke queue fields.

## Memory

- **K-30** Kernel allocation is `kmalloc`/`kfree` (`sys/lib/kmalloc.c`) or the
  VMM page API. Never `malloc` in the kernel. Every allocation is checked
  (C-21), has one owner, and is freed on the error path.
- **K-31** MMIO / device frames are **not** RAM. A physical address at or
  above the RAM top (`numPages * PAGE_SIZE`) has no entry in `vmmMemoryMap`.
  Anything that walks page tables to free, COW-mark or refcount frames must
  check this first (fork/COW, `vmm_cleanVirtualSpace`, `freePage`). New
  code that handles PTEs inherits the same obligation.
- **K-32** Page-table edits are followed by the arch TLB-invalidate
  (`invlpg` / `tlbi`) and, on aarch64, the barrier sequence
  (`dsb`/`isb`). Instruction/data cache maintenance after writing code or DMA
  buffers (aarch64) is not optional.
- **K-33** DMA buffers: physically contiguous where the device needs it,
  aligned, and flushed/invalidated per the arch rules. Do not hand a stack or
  `kmalloc` pointer to a device without checking the above.
- **K-34** Never dereference a physical address as if it were virtual. Use the
  VMM mapping helpers; the direct map exists only where the arch doc says so.

## User pointers and untrusted input

- **K-40** A pointer or length that arrives from userland is untrusted. It
  is validated against the user address range for the calling process and
  copied with an explicit length into kernel memory before use. **Check and use
  must not be separated by anything that can sleep or change the mapping** (TOCTOU).
- **K-41** There is no dedicated `copyin`/`copyout` yet. Existing code that
  dereferences user pointers directly relies on the cooperative, IRQ-masked,
  same-address-space model; that is a known gap, not a pattern. New syscall
  code must go through one helper per direction (add `copyin`/`copyout`
  in `sys/include/ubixos/` if none exists when you need it) so the validation
  has a single home and the gap can be closed in one place.
- **K-42** Lengths from userland are clamped (`min(len, KBUF_MAX)`) before
  they size an allocation or a copy. Strings from userland are bounded
  (`strnlen` with a limit) and terminated explicitly.
- **K-43** Anything copied *to* userland is fully initialised (C-24).
- **K-44** File names/paths from userland go through the VFS path layer;
  they are not parsed ad hoc. Do not call filesystem driver functions from
  outside `sys/fs/vfs/` — go through the VFS function-pointer table.

## Errors and panics

- **K-50** `kpanic` is for broken kernel invariants. Bad input, absent
  hardware and `ENOMEM` return errors.
- **K-51** Syscall handlers report failure as **negative errno** in
  `td->td_retval[0]` (and as the return value where the table dispatcher
  forwards it): musl tests for a negative value, not a carry flag. Follow
  [abi-and-syscalls.md](abi-and-syscalls.md); never mix conventions in one
  handler.
- **K-52** Never loop forever on a hardware condition; every poll has a bound
  and a timeout error.

## Device drivers

- **K-60** Drivers register through the device model (`deviceAdd` / newbus) —
  follow `docs/drivers/writing-a-driver.md`. No hard-coded port/MMIO
  addresses outside the arch/board tables; discover via PCI/DTB/ACPI.
- **K-61** MMIO is accessed through accessor macros/inline functions with the
  correct width and barriers (`volatile` pointer + barrier on aarch64). No
  byte-swapping assumptions: use explicit `le32toh`-style conversions for
  device/disk formats.
