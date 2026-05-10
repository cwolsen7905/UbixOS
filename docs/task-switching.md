# UbixOS Task Switching Internals

## Overview

UbixOS implements preemptive multitasking on i386 using **hardware TSS-based task switching** — the mechanism provided by the x86 architecture where a single `ljmp` to a TSS descriptor causes the CPU itself to atomically save the outgoing context and load the incoming one. This is distinct from the "software context switch" approach used by Linux and most modern OSes, where the kernel manually pushes and pops registers around an explicit stack swap.

The tradeoff is real: hardware task switching is simpler to implement correctly (the CPU handles the save/restore atomically, including CR3), but it is slower than software switching due to the cost of the full TSS load, and it is incompatible with SMP without significant additional work.

---

## Data Structures

### Task Control Block — `kTask_t` / `taskStruct`

**`sys/include/ubixos/sched.h`**

Each task owns a `kTask_t` (aliased as `taskStruct`). The fields relevant to task switching are:

| Field | Type | Purpose |
|-------|------|---------|
| `tss` | `struct tssStruct` | Full hardware TSS — the CPU's snapshot of this task |
| `i387` | `struct i387Struct` | x87 FPU register state (managed separately, not by the CPU) |
| `td` | `struct thread` | Thread metadata; `td.frame` points to the interrupt trapframe |
| `state` | `tState` | Task lifecycle state: `NEW`, `READY`, `RUNNING`, `FORK`, `WAIT`, `DEAD`, etc. |
| `next` / `prev` | `kTask_t *` | Doubly-linked task list (intrusive) |

The global `kTask_t *_current` always points to the currently running task.

### TSS — `struct tssStruct`

**`sys/include/sys/tss.h:34`**

This is the Intel-defined i386 TSS layout. Fields saved and restored by the CPU during a hardware task switch:

```
back_link           — selector of previous TSS (for nested task calls; unused here)
esp0 / ss0          — kernel stack for ring-0 entry (used on every syscall/interrupt)
esp1 / ss1          — ring-1 stack (unused; zeroed)
esp2 / ss2          — ring-2 stack (unused; zeroed)
cr3                 — page directory physical address (switches address space)
eip                 — instruction pointer
eflags              — CPU flags (IF, DF, etc.)
eax, ecx, edx, ebx — general-purpose registers
esp, ebp            — stack and frame pointers
esi, edi            — index registers
es, cs, ss, ds, fs, gs — segment selectors
ldt                 — LDT selector for this task
trace_bitmap        — debug trap on task switch (set to 0; unused)
io_map              — offset to I/O permission bitmap (set to 0x8000; effectively disables it)
io_space[8192]      — I/O permission bitmap storage (present in the struct but not used)
```

The `struct tssStruct` embeds the full 8192-byte I/O permission bitmap even though the `io_map` field is set to `0x8000` (past the end of the TSS), which tells the CPU to deny all I/O by default. This means every `kTask_t` carries ~8 KB of unused data.

### Trapframe — `struct trapframe`

**`sys/include/sys/trap.h`**

When a task enters the kernel through an interrupt or syscall, the CPU and the interrupt stub together push a `trapframe` onto the kernel stack at `esp0`. This is not used directly by the hardware task switch — it is the mechanism by which fork and exec read the user register state at the point of the syscall.

```
Software-saved (by IDT stubs):  gs, fs, es, ds, edi, esi, ebp, esp, ebx, edx, ecx, eax
Hardware-saved (by CPU):        trapno, err, eip, cs, eflags
Ring-crossing only:             user_esp, user_ss  (present only when entering from ring 3)
```

The pointer to the current trapframe is stored in `_current->td.frame` during syscall dispatch and used by `sys_fork` to snapshot the caller's register state.

---

## The Global Descriptor Table (GDT)

**`sys/init/main.c:94`**, **`sys/include/sys/gdt.h`**

The GDT is a statically allocated 11-entry table (`ubixGDT[11]`). It is loaded once at boot and subsequently modified only to redirect GDT[4] during scheduling. The entries are:

| Index | Selector | Type | Base | Limit | DPL | Purpose |
|-------|----------|------|------|-------|-----|---------|
| 0 | `0x00` | Null | — | — | — | Required dummy entry |
| 1 | `0x08` | Code | `0x00000000` | 4 GB | 0 | Ring-0 (kernel) code segment |
| 2 | `0x10` | Data | `0x00000000` | 4 GB | 0 | Ring-0 (kernel) data segment |
| 3 | `0x18` | LDT | `VMM_USER_LDT` | 4 GB | 0 | Points to per-process LDT region |
| 4 | `0x20` | TSS | *(dynamic)* | `sizeof(tssStruct)` | 3 | **Scheduler TSS** — redirected by `sched()` |
| 5 | `0x28` | Code | `0x00000000` | 4 GB | 3 | Ring-3 (user) code segment |
| 6 | `0x30` | Data | `0x00000000` | 4 GB | 3 | Ring-3 (user) data segment |
| 7 | `0x38` | TSS | `0x5200` | `sizeof(tssStruct)` | 3 | GPF TSS (double-fault handler) |
| 8 | `0x40` | TSS | `0x6200` | `sizeof(tssStruct)` | 0 | Stack-fault TSS |
| 9 | `0x48` | Data | `0x00000000` | 4 GB | 0 | SMP private data (per-CPU) |
| 10 | `0x50` | Data | `0xBFC00000` | 4 GB | 3 | User `%gs` — top of user address space (stack) |

**Key design choices:**

- The kernel and userland both use flat segments spanning the full 4 GB address space (base=0, limit=0xFFFFF with 4K granularity = 4 GB). Segmentation is effectively disabled; all memory protection is done through paging.
- There is only **one TSS descriptor** for the scheduler (GDT[4]). Before each task switch `sched()` rewrites the base address in GDT[4] to point to the next task's `tssStruct` in memory. This avoids reserving a GDT slot per task at the cost of a write to GDT memory on every switch.
- The initial TSS at boot is placed at physical address `0x4200` (hardcoded). After the first `sched()` call, GDT[4] is redirected dynamically and `0x4200` is never used again.
- GDT[4] is created with `dDpl3` (DPL=3). This is necessary for user-space to perform an explicit `int 0x28` or `ljmp $0x20,$0` into the scheduler — though in practice only kernel code calls `sched()`.

### Segment Macros

The GDT descriptors are constructed with two macros in `sys/include/sys/gdt.h`:

```c
#define ubixStandardDescriptor(base, limit, control) \
  { .descriptor = { (limit & 0xffff), (base & 0xffff), ((base>>16)&0xff), \
    ((control+dPresent)>>8), (limit>>16), ((control&0xff)>>4), (base>>24) } }
```

This encodes the Intel descriptor format inline. The `dPresent` flag is OR'd in automatically, so all descriptors are marked present. The `control` word is a bitfield assembled from the constants in `gdt.h` (e.g., `dCode + dRead + dBig + dBiglim + dDpl3`).

---

## The Local Descriptor Table (LDT)

**`sys/init/main.c:98`** — GDT[3]: `ubixStandardDescriptor(VMM_USER_LDT, 0xFFFFF, dLdt)`

The LDT descriptor in GDT[3] points to `VMM_USER_LDT`, a fixed virtual address in the VMM layout where per-process LDT data would be stored. The TSS for every task sets `tss.ldt = 0x18` (selector for GDT[3]).

In practice the LDT is loaded but empty. All code and data accesses use the flat GDT segments (CS=`0x0B`/`0x08`, DS/SS=`0x10`/`0x30`). No LDT descriptors are ever installed by user processes. The LDT entry exists as structural scaffolding — it was likely part of a design that intended per-process segment descriptors — but the system runs fine without it because paging handles isolation.

The TSS `tss.ldt = 0x18` means the CPU loads GDT[3] as the LDT on every hardware task switch. This is correct but wasteful if the LDT is empty.

---

## The Task Switch: Step by Step

### 1. Timer Interrupt

**`sys/arch/i386/timer.S:29`**

The PIT (8254) is programmed to fire IRQ0 at approximately 200 Hz (~5 ms period). The interrupt handler `timerInt`:

1. Pushes all general-purpose registers (`pusha`).
2. Increments the tick counter.
3. Every 200 ticks (approximately once per second with the current PIT divisor — see note below), calls `sched()`.
4. On return, pops registers (`popa`) and issues `iret`.

The timer's coarse 1-second quantum means tasks run for up to one full second before preemption. This is very long by modern standards (Linux defaults to ~4 ms).

### 2. Scheduler — `sched()`

**`sys/arch/i386/sched.c:85`**

```c
void sched() {
  if (spinTryLock(&schedulerSpinLock)) return;  // non-reentrant

  // Round-robin from _current->next, wrapping around
  // Promotes FORK→READY, reaps DEAD tasks
  // Sets _current to the chosen task

  // Redirect GDT[4] base to new task's TSS
  memAddr = (uint32_t) &(_current->tss);
  ubixGDT[4].descriptor.baseLow  = (memAddr & 0xFFFF);
  ubixGDT[4].descriptor.baseMed  = ((memAddr >> 16) & 0xFF);
  ubixGDT[4].descriptor.baseHigh = (memAddr >> 24);
  ubixGDT[4].descriptor.access   = '\x89';  // TSS available (type=9, present=1, DPL=0)

  _current->state = RUNNING;
  spinUnlock(&schedulerSpinLock);

  asm("sti");
  asm("ljmp $0x20,$0");  // THE SWITCH
}
```

The access byte `0x89` is: present=1, DPL=0, S=0, type=`1001` (32-bit TSS, available). The CPU sets the busy bit (type becomes `1011`) automatically when it activates the TSS.

### 3. `ljmp $0x20, $0` — the hardware switch

Selector `0x20` = GDT index 4, TI=0 (GDT), RPL=0. The offset `$0` is ignored for TSS descriptors. When the CPU executes this instruction:

1. **Saves outgoing state** — all GPRs, segment registers, EIP, EFLAGS, ESP, EBP, CR3 — into the *current* CPU's TSS (which is the old `_current->tss`, because GDT[4] was just updated to point to the *new* task).

   Wait — actually the order matters here. `sched()` updates GDT[4] to point to the **new** task's TSS before executing `ljmp`. So at the moment of `ljmp`:
   - GDT[4] → new task's TSS
   - The CPU's active TSS register (`TR`) still points to where it was set last (the previous task's TSS, from the last switch)
   - `ljmp` to a TSS descriptor: CPU saves current state into the TSS pointed to by **TR** (old task), then loads the new TSS from GDT[4] (new task), and updates TR.

2. **Loads incoming state** — CR3 (page directory switches here, atomically), EIP, EFLAGS, all GPRs, all segment registers, ESP, from the new task's TSS.

3. Execution resumes at `new_task->tss.eip` in the context of the new task, with its address space, stack, and registers.

The entire save+load is atomic from the software perspective — no interrupts can fire between saving the old state and loading the new one.

---

## Fork

**`sys/arch/i386/fork.c:41`**

`sys_fork()` is called from the POSIX syscall handler. By the time it runs, the user-space register state has already been captured in the trapframe at `td->frame`. Fork:

1. Allocates a new `kTask_t` via `schedNewTask()`.
2. Manually fills in the new task's TSS from the parent's trapframe:
   - `tss.eip = td->frame->tf_eip` — resumes at the same instruction as parent
   - **`tss.eax = 0x0`** — child returns 0 from `fork()`
   - All other GPRs, segment registers, EBP, ESP copied from the trapframe
   - `tss.esp0 = _current->tss.esp0` — shares parent's kernel stack pointer
   - `tss.ss0 = 0x10` — kernel data segment
   - **`tss.cr3 = vmm_copyVirtualSpace(newpid)`** — new page directory (COW copy)
   - `tss.ldt = 0x18`, `tss.io_map = 0x8000`
3. Sets `newProcess->state = FORK`.
4. Calls `sched_yield()` in a spin loop until the child transitions to `READY`:
   ```c
   while (newProcess->state == FORK)
     sched_yield();
   ```
5. The scheduler sees `FORK` state and promotes it to `READY` (sched.c:98-99).
6. On the child's first scheduling slot, `ljmp` loads its TSS. It "returns" from the syscall at `tf_eip` with EAX=0.

The parent waits in the loop above. Once the scheduler has run the child once (flipping it from `FORK` to `READY`), the parent exits the loop and returns the child's PID.

---

## FPU State — Lazy Save/Restore

**`sys/sys/idt.c:775`**

The x87 FPU state (`struct i387Struct` in `kTask_t`) is **not** saved/restored by the hardware task switch. Instead:

- On every context switch, the kernel sets `CR0.TS` (task switched flag).
- When any task executes a floating-point instruction, the CPU raises `#NM` (INT7, "device not available").
- The INT7 handler (`mathStateRestore`) in `idt.c`:
  1. Saves the current FPU owner's state with `fnsave`.
  2. Restores the new task's state with `frstor` (or `fninit` if this is its first FP use).
  3. Clears `CR0.TS`.

This is the classic lazy FPU approach — tasks that never use floating point pay zero cost.

---

## execThread — Kernel Threads

**`sys/arch/i386/i386_exec.c:149`**

Kernel threads (created by `execThread`) set up a TSS with ring-0 selectors:

```c
newProcess->tss.cs = 0x08;   // kernel code
newProcess->tss.ss = 0x10;   // kernel data
newProcess->tss.esp0 = 0x0;  // no ring transition needed
newProcess->tss.cr3 = (unsigned int) kernelPageDirectory;
```

The thread runs entirely in ring 0 with the shared kernel page directory. There is no user-space component. This is used to run kernel subsystems (drivers, etc.) as independently scheduled entities.

---

## Critical Review and Suggested Improvements

### 1. The 1-Second Scheduling Quantum Is Very Coarse

**`sys/arch/i386/timer.S:47-51`**

The quantum check fires every 200 ticks. If the PIT divisor is set to the standard 11932 (≈100 Hz), 200 ticks = 2 seconds. Even at 200 Hz, 200 ticks = 1 second. Either way this is orders of magnitude coarser than any real-time or interactive system needs. A task can monopolize the CPU for a full second.

**Suggestion:** Reduce the quantum to 10–20 ms (2–4 ticks at 200 Hz, or 1 tick at 100 Hz if you lower the divisor). Better: make the quantum configurable per-task to enable priority classes.

---

### 2. No Per-Task Kernel Stack

**`sys/arch/i386/fork.c:85`**

```c
newProcess->tss.esp0 = _current->tss.esp0;
```

Fork copies the **parent's** `esp0` into the child's TSS. Both tasks now have the same kernel stack pointer. This works on UP because only one task can be in kernel mode at a time, but it is fundamentally broken for SMP and fragile even on UP — if the child is ever preempted while in a syscall, and the parent also enters a syscall, they will corrupt each other's kernel stacks.

**Suggestion:** Allocate a dedicated kernel stack page for each new task in `schedNewTask()` and set `tss.esp0` to the top of that page. This is the standard design (used by Linux, FreeBSD, etc.) and is a prerequisite for correct SMP support.

---

### 3. Single Shared TSS Descriptor in GDT — Race Window

**`sys/arch/i386/sched.c:138-151`**

```c
asm("cli");
// ... update GDT[4] base ...
_current->state = RUNNING;
spinUnlock(&schedulerSpinLock);
asm("sti");
asm("ljmp $0x20,$0");
```

There is a window between `sti` and `ljmp` where an interrupt could fire. If the interrupt handler itself calls `sched()` (e.g., a device interrupt that wakes a higher-priority task), the GDT[4] base has already been updated to the intended new task, but the `ljmp` hasn't happened yet. The reentrant `sched()` call would hit `spinTryLock` and return early, so in practice the race is handled — but the `sti` before `ljmp` is still an unnecessary risk. Moving `sti` to after `ljmp` (or keeping `cli` through `ljmp`) would close the window cleanly.

**Suggestion:**
```c
asm("cli");
// update GDT[4] ...
spinUnlock(&schedulerSpinLock);
asm("ljmp $0x20,$0");
// sti is implicit: the loaded task's EFLAGS.IF determines interrupt state
```

---

### 4. Fork Spin-Wait Is Wasteful

**`sys/arch/i386/fork.c:122`**

```c
while (newProcess->state == FORK)
  sched_yield();
```

The parent busy-polls on the child's state, burning CPU time. Each `sched_yield()` call runs through the full scheduler, finds the child in FORK state, promotes it to READY, then switches back. It works, but it means fork always takes at least two full context switches.

**Suggestion:** Set `newProcess->state = READY` directly at the end of `sys_fork()`, before returning. The child is fully initialized at that point — the FORK state exists only to prevent the scheduler from switching to the child before the TSS is ready, and the TSS is ready before the `while` loop. The loop and the FORK state could be eliminated entirely.

---

### 5. The LDT Descriptor Is Misleading

**`sys/init/main.c:98`**

```c
ubixStandardDescriptor(VMM_USER_LDT, 0xFFFFF, (dLdt)),
```

GDT[3] is described as an LDT and every task's TSS points `tss.ldt = 0x18` at it, but no actual LDT entries are ever installed. The CPU loads GDT[3] as the active LDT on every task switch (because `tss.ldt` is non-zero), which does a memory access to `VMM_USER_LDT` and a validity check.

If LDTs are not being used, `tss.ldt = 0x0` (null selector) would suppress the LDT load entirely and avoid the wasted work. If you do intend to use LDTs per-process someday, each task needs a separate LDT mapped at a known address — a single shared GDT descriptor cannot point to per-process data unless the base is updated per-switch (same pattern as GDT[4]).

**Suggestion:** Either zero `tss.ldt` across the board and remove GDT[3], or document clearly that this is a placeholder for future per-process LDT support.

---

### 6. The I/O Permission Bitmap Wastes 8 KB Per Task

**`sys/include/sys/tss.h:70`**

```c
char io_space[8192];
```

Every `kTask_t` carries an 8192-byte IOPB even though `tss.io_map = 0x8000` means the CPU never consults it (an `io_map` offset past the TSS limit causes all I/O port accesses from ring 3 to `#GP`). The struct is ~8.2 KB; without `io_space` it would be ~100 bytes.

**Suggestion:** Remove `io_space[8192]` from `struct tssStruct` and keep `tss.io_map = 0x8000`. If you later need selective I/O permissions for a task (e.g., a userspace driver), allocate the bitmap separately and set `io_map` to the correct offset only for that task.

---

### 7. `schedNewTask` Leaks File Descriptors on Kernel Threads

**`sys/arch/i386/sched.c:179-184`**

```c
for (i = 0; i < 3; i++) {
  fp = (void *) kmalloc(sizeof(struct file));
  tmpTask->td.o_files[i] = (void *) fp;
  fp->f_flag = 0x4;
}
```

Every new task (including kernel threads via `execThread`) gets stdin/stdout/stderr `struct file` entries allocated. Kernel threads have no use for these and the memory is never freed. `execThread` also accesses `newProcess->files[0]` (a different field!) and panics if it is non-null — a sign that `files[]` and `td.o_files[]` are two partially-overlapping file descriptor systems.

**Suggestion:** Only allocate the default stdio FDs for user processes. Kernel thread creation should go through a separate path or pass a flag to `schedNewTask`.

---

### 8. `goto schedStart` Reentrant Loop Is Hard to Follow

**`sys/arch/i386/sched.c:94-128`**

The scheduler uses a `goto` to restart the task-list scan after reaping a dead task. While functionally correct, the loop structure is confusing — it's a `for` loop that can restart itself via `goto` and will infinite-loop if all tasks are dead (the only exit is finding a READY task, and there's no explicit "no tasks available, run idle" path).

**Suggestion:** Refactor into a cleaner loop with an explicit idle task that is always READY, eliminating the need for `goto` and providing a well-defined "nothing to run" state.

---

## Summary: What's Good

- **Correctness for UP**: The hardware TSS switch is correct, atomic, and handles CR3 automatically. For a single-core hobby OS it works reliably.
- **Simplicity**: The scheduler fits in ~150 lines. The entire context switch mechanism is three lines of C plus one assembly instruction.
- **FPU handling**: Lazy save/restore is the right approach and is implemented correctly.
- **Descriptor macro system**: The `ubixStandardDescriptor` / `ubixGateDescriptor` macros cleanly encode the fiddly Intel descriptor format in a readable way.
- **Spinlock on scheduler**: The `spinTryLock` guard prevents re-entrant `sched()` calls correctly.
