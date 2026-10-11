# uBixOS Task Switching Internals

master is 64-bit only. Both live architectures use the same model: a **software
context switch** of callee-saved registers plus a stack-pointer swap, driven by the
arch-neutral scheduler in `sys/kern/sched_core.c` / `sched_dispatch.c`. The 32-bit
i386 implementation (also software switching since 2026, earlier hardware TSS
switching) is frozen on `releng/2` and is kept at the end as
[labelled history](#history-i386-task-switching).

---

## Common model

| Piece | Where | Role |
|-------|-------|------|
| `kTask_t` | [sys/include/ubixos/sched.h](../../sys/include/ubixos/sched.h) | Task control block; the global `_current` is the running task |
| `struct md_proc` (`task->md`) | `sys/include/<arch>/proc.h` | Arch state: saved kernel SP (`md_kstack`), address-space root, entry/user SP, mmap/brk cursors, TLS base, FP save area |
| `sched()` | `sys/kern/sched_dispatch.c` | Picks the next task from per-priority run queues (32 bands, O(1) pick via a ready bitmask) |
| `switch_to(prev, next)` | `sys/arch/<arch>/kern/proc.c` | Swaps address space, TLS and FP state, then calls the register-level switch |
| register switch | `aarch64_ctx_switch` (`context.S`), `x86_64_ctx_switch` (`cpu_switch.S`) | Push callee-saved regs, store SP into `prev->md.md_kstack`, load `next`'s, pop, `ret` |
| `md_setup_initial_frame()` | `proc.c` | Seeds a new task's kernel stack so the first switch `ret`s into a trampoline |

Scheduling is cooperative inside the kernel and preemptive only at user-mode
boundaries: a 100 Hz timer tick that interrupted **user** code calls `sched()`;
a tick that interrupted kernel code never switches (the kernel is non-preemptible),
so kernel code runs to its next voluntary `sched_yield()` or block. Every task has its
own 64 KB kernel stack (`KSTACK_SIZE` in each arch's `proc.c`, matching
`schedNewTask`'s `kmalloc`).

The run queues are per-CPU (`CONFIG_SCHED_PERCPU = 1` in
`sys/include/ubixos/sched_internal.h`, `SCHED_MAX_CPUS = 8`). A new task is first run
under the run-queue lock, so each trampoline calls `sched_resume_unlock()` before doing
anything else.

Both arches switch FP/SIMD state **eagerly** in `switch_to` (no lazy trap): the kernel
itself is built without FP/SIMD, so the live vector registers always belong to the
outgoing user task.

---

## aarch64

[sys/arch/aarch64/kern/context.S](../../sys/arch/aarch64/kern/context.S),
[proc.c](../../sys/arch/aarch64/kern/proc.c), `sys/include/aarch64/proc.h`.

### `aarch64_ctx_switch(save_sp, next_sp)`

Pushes `x19`–`x30` (12 registers, 96 bytes), stores `sp` through `x0`, loads `sp` from
`x1`, pops the same set and `ret`s. The AAPCS64 caller has already preserved
`x0`–`x18`, so this is a complete cooperative switch.

### `switch_to(prev, next)`

1. If `next->md.md_ttbr0` differs from `prev`'s, `pmap_switch()` loads the new TTBR0.
   (TTBR1 — the kernel physmap — is global.)
2. Saves `TPIDR_EL0` (musl's TLS thread pointer) into `prev->md.md_tpidr` and loads
   `next`'s.
3. Saves/restores `v0`–`v31` + `FPSR`/`FPCR` through the 16-byte-aligned region of
   `md_fpu[]`.
4. Calls `aarch64_ctx_switch(&prev->md.md_kstack, next->md.md_kstack)`.

### New tasks

`md_setup_initial_frame()` zeroes a callee-saved frame at the top of the kernel stack and
sets the saved `lr` slot to `user_trampoline` (an EL0 task: `md_usp != 0`; it
`aarch64_eret_to_el0(md_entry, md_usp)`) or `kthread_trampoline` (a kernel thread: unmasks
IRQs with `daifclr`, then calls `md_entry(md_arg)`).

### Per-CPU state and preemption

Per-CPU data is reached through `TPIDR_EL1` (`curcpu()` in `include/aarch64/pcpu.h`).
The timer is the EL1 virtual generic timer at 100 Hz (`SCHED_HZ`). In
[exceptions.c](../../sys/arch/aarch64/kern/exceptions.c), a tick that interrupted EL0 runs
`signal_check()` then `sched()` on the BSP; secondaries preempt their own EL0 task when
`CONFIG_SCHED_PERCPU` is set. A tick that interrupted EL1 only advances time-based
callouts (under a try-lock) and never switches.

### Fork

[proc.c](../../sys/arch/aarch64/kern/proc.c) `aarch64_fork_common()`:
`pmap_fork_copy()` builds the child's TTBR0 (COW), the parent's saved register frame is
copied for the child with `x0 = 0`, FP state is copied into the child's `md_fpu`, and the
child's `md_kstack` is set so `sched_ready()` skips `md_setup_initial_frame()`. `fork`,
`clone` (posix_spawn) and `rfork` (threads sharing TTBR0) are the three entry points.

---

## x86_64

[sys/arch/x86_64/kern/cpu_switch.S](../../sys/arch/x86_64/kern/cpu_switch.S),
[proc.c](../../sys/arch/x86_64/kern/proc.c), `sys/include/x86_64/proc.h`.

### `x86_64_ctx_switch(save_rsp, next_rsp)`

Pushes `rbx, rbp, r12–r15` (SysV callee-saved), stores `rsp` through `rdi`, loads `rsp`
from `rsi`, pops the set and `ret`s. Called with interrupts disabled.

### `switch_to(prev, next)`

1. If `md_cr3` differs, `mov %cr3` loads the new PML4 (kernel space is shared in
   `PML4[256..511]`, so the stack stays mapped).
2. `x86_64_set_user_kstack()` points this CPU's `TSS.rsp0` and `pcpu.kernel_rsp` at the
   incoming task's kernel-stack top, so both the interrupt path (ring 3 → 0 via TSS)
   and the `syscall` path (`%gs:40`) land on it.
3. Writes `MSR_FS_BASE` (`0xC0000100`) from `md_fsbase`. The `syscall` path does not
   `swapgs` (the kernel owns `%gs`; musl keeps TLS in `%fs`), so hardware does not save
   the base per task.
4. `fxsave`/`fxrstor` on the 16-byte-aligned region of `md_fpu[]` (`CR4.OSFXSR` is set at
   boot; `CR0.TS` is clear, so no `#NM`).
5. Calls `x86_64_ctx_switch(&prev->md.md_kstack, next->md.md_kstack)`.

### New tasks

`md_setup_initial_frame()` seeds the saved return address with `user_trampoline` (a ring-3
task: `x86_64_iret_to_user(md_entry, md_usp)`) or `kthread_trampoline` (re-enables
interrupts with `sti`, then `md_entry(md_arg)`). A forked child instead resumes at
`ret_from_fork` in `syscall_entry.S`, with `rsp` on its copied trapframe and `rax = 0`,
sharing the `SYSRETQ` return tail of the parent's syscall path.

### Per-CPU state and preemption

Per-CPU data is reached through `%gs` (`curcpu()` reads `%gs:8`; `struct pcpu` in
`include/x86_64/pcpu.h`). The tick source is the 8253/8254 PIT programmed for 100 Hz
(`pit_init(100)`); `x86_64_irq()` also advances `sysTicks` by the TSC-derived ticks due
(so ticks lost under emulation are caught up) and charges the running task via
`sched_account_tick()`. In [idt.c](../../sys/arch/x86_64/kern/idt.c) a tick whose saved
`cs & 3 == 3` runs `signal_check()` then `sched()`; ticks that interrupted ring 0 do not
switch.

### Fork

[fork.c](../../sys/arch/x86_64/kern/fork.c): `x86_64_fork_copy()` duplicates the user
page tables with COW, `x86_64_fork_common()` copies the trapframe for the child and
seeds its context frame to return through `ret_from_fork`. `x86_64_fork`, `x86_64_clone`
(posix_spawn) and `x86_64_rfork` (CLONE_THREAD) are the entry points.

---

## Differences at a glance

| | aarch64 | x86_64 |
|-|---------|--------|
| Callee-saved set switched | `x19`–`x30` | `rbx rbp r12–r15` |
| Address-space register | `TTBR0_EL1` (`md_ttbr0`) | `CR3` (`md_cr3`) |
| Kernel stack on user entry | `SP_EL1` (EL0→EL1 switches SP in hardware) | `TSS.rsp0` / `pcpu.kernel_rsp` |
| User TLS | `TPIDR_EL0` (`md_tpidr`) | `FS.base` MSR (`md_fsbase`) |
| FP/SIMD | `v0–v31`, `FPSR/FPCR` | `fxsave`/`fxrstor` (x87 + SSE) |
| Per-CPU pointer | `TPIDR_EL1` | `%gs` base |
| Timer | generic timer, 100 Hz | PIT, 100 Hz |
| First entry to user mode | `ERET` | `IRETQ` |

---

## History: i386 task switching

> Everything below describes the 32-bit i386 port, which exists only on `releng/2`.
> Paths such as `sys/arch/i386/…` do not exist on master. It is kept because the design
> history (hardware task switching → software switching) explains several of the choices
> above, e.g. why segment/TLS state is saved explicitly per task.

### Overview

uBixOS i386 implemented preemptive multitasking using **software context
switching** — the kernel explicitly saves the outgoing task's state and loads
the incoming one around a stack swap, the same approach used by Linux and
FreeBSD. (Prior to 2026 the kernel used i386 *hardware* task switching — a
single `ljmp` to a per-task TSS — but that was replaced because hardware task
switching makes the segment registers per-task TSS state, which is fundamentally
incompatible with `%gs`-based per-CPU data and therefore with SMP. See
[../design/completed/software-task-switch-plan.md](../design/completed/software-task-switch-plan.md).)

The switch lives in `sys/arch/i386/context_switch.c` (`releng/2`)
(`switch_to`, `cpu_switch`, `md_setup_initial_frame`, `ret_from_fork`) and is
driven from `sched()` in `sys/arch/i386/sched_switch.c` (`releng/2`).

---

### Data Structures

#### Task Control Block — `kTask_t` / `taskStruct`

[sys/include/ubixos/sched.h](../../sys/include/ubixos/sched.h). Fields relevant to switching:

| Field | Type | Purpose |
|-------|------|---------|
| `md.md_tss` | `struct tssStruct` | **No longer a live hardware TSS** — now a *description* of the task's initial state (entry eip/cs, segments, cr3, esp0) that `md_setup_initial_frame` reads once. Also still written by fork/exec. |
| `md.md_kstack` | `u_int32_t` | Saved kernel ESP — the heart of the software switch; points at this task's saved-context frame on its kernel stack |
| `md.md_i387` | `struct i387Struct` | x87 FPU state (lazy save/restore) |
| `kernelStack` | `u_int32_t *` | 8 KB per-task ring-0 stack (`kmalloc`, allocated in `schedNewTask`) |
| `td.frame` | `struct trapframe *` | Interrupt trapframe at syscall/IRQ entry; used by fork to snapshot user state |
| `state` | `tState` | `NEW`, `READY`, `RUNNING`, `ZOMBIE`, `DEAD`, … |

The global `kTask_t *_current` always points to the running task.

#### The kernel TSS

There is exactly **one TSS** (boot TSS at physical `0x4200`, GDT selector `0x20`,
`TR` loaded once at boot). The x86 architecture requires a TSS only to supply
`ss0`/`esp0` when the CPU takes an interrupt/trap from ring 3 → ring 0. `switch_to`
updates `kernelTSS->esp0` to the incoming task's kernel stack on every switch.
GDT[4]'s base is no longer rewritten per task; the per-task TSSes and the old
GPF/stack-fault task-gate TSSes (`0x5200`/`0x6200`) are gone.

#### Trapframe — `struct trapframe`

[sys/include/sys/trap.h](../../sys/include/sys/trap.h). On kernel entry the CPU + the
IDT stub push a trapframe onto the kernel stack at `esp0`:

```
Pushed by IDT stub:  gs, fs, es, ds, edi, esi, ebp, esp, ebx, edx, ecx, eax, trapno
Pushed by CPU:       err, eip, cs, eflags
Ring-crossing only:  user_esp, user_ss   (present only when entering from ring 3)
```

`sys_fork` snapshots `td->frame` to build the child's resume state.

---

### The Software Switch

#### `switch_to(prev, next)` and `cpu_switch`

`sys/arch/i386/context_switch.c` (`releng/2`). `sched()` calls
`switch_to(prev, _current)`. `switch_to`:

1. Sets `kernelTSS->esp0 = next->md.md_tss.esp0` (so the next ring3→ring0 entry
   lands on next's kernel stack).
2. Sets `CR0.TS` so the next task takes `#NM` (`_int7`) on its first FPU use,
   triggering the lazy FPU restore. **Hardware task switching set TS
   automatically; software switching must do it explicitly or stale x87 state
   leaks between tasks.**
3. Calls `cpu_switch(&prev->md.md_kstack, next->md.md_kstack, next->md.md_tss.cr3)`.

`cpu_switch` (pure asm) is the register-level switch. It saves the **callee-saved
GP registers and the segment registers** of the outgoing task, saves its kernel
ESP into `prev->md_kstack`, swaps CR3 if the address space differs (kernel VA is
shared, so the stack stays mapped across the change), loads `next`'s kernel ESP,
restores its segment + callee registers, and `ret`s:

```
push ebx; push esi; push edi; push ebp     ; callee-saved
push gs; push fs; push es; push ds         ; segment registers (CRITICAL — see below)
mov esp, (prev_slot)                       ; save outgoing kernel ESP
... swap CR3 if changed ...
mov (next_ksp), esp                        ; load incoming kernel ESP
pop ds; pop es; pop fs; pop gs             ; restore incoming segments
pop ebp; pop edi; pop esi; pop ebx
ret                                        ; resume prior cpu_switch caller / new task
```

> **Why `cpu_switch` saves the segment registers:** the hardware `ljmp` switch
> saved/restored *all* segment registers per task via the TSS. Software switching
> must replicate that, or `%gs` (a user task's TLS selector `0x0F`, or a kernel
> thread's `0x10`) leaks across a switch. The original bug: a user task resumed
> with another task's `%gs` read a garbage TLS thread pointer → segfault; a
> kernel thread that inherited a user `%gs=0x0F` faulted reading the LDT. This is
> the single most important correctness detail of the conversion.

#### New tasks — `md_setup_initial_frame` + `ret_from_fork`

When a task is first made runnable (`sched_ready`), `md_setup_initial_frame`
builds its initial kernel-stack frame from `md_tss` so the *first* `cpu_switch`
into it resumes correctly:

- **Ring 3 (user)** — a full `struct trapframe` is laid out at the top of the
  kernel stack with the task's user segments/eip/esp, followed by a return
  address of `ret_from_fork`. `cpu_switch` pops the (kernel-selector) saved
  segments + callee regs and returns to `ret_from_fork`, which runs the standard
  trap-return epilogue (`pop gs/fs/es/ds; popa; add $8,%esp; iret`) to `iret`
  into ring 3 with the real user segments.
- **Ring 0 (kernel thread)** — runs on its own stack (`md_tss.esp`, seeded with
  the entry argument by `execThread`); `cpu_switch` returns straight to the entry
  point, no `iret`.
- **v86 (BIOS)** — a VM86 `iret` frame; `cpu_switch` returns to `enter_vm86`
  (`popa; iret` into VM86 mode). See the v86 section below.

The cpu_switch-level saved segments are seeded to kernel selectors (`0x10`); for
a user task `ret_from_fork`'s `iret` then installs the real ring-3 segments.

#### `sched()`

`sys/arch/i386/sched_switch.c` (`releng/2`) picks the next task (O(1) via
the per-priority run-queues + `ready_mask`), captures `prev = _current`, sets
`_current = next`, then — under `cli`, after releasing the scheduler lock —
calls `switch_to(prev, _current)`. The outgoing task resumes here on its next
slot; `cpu_switch` restored its EFLAGS with IF cleared (the `cli` above), so
`sched()` re-enables interrupts (`sti`) on resume.

---

### Fork

`sys/arch/i386/fork.c` (`releng/2`). `sys_fork` fills the child's
`md_tss` from the parent's trapframe (`td->frame`): `eip = tf_eip` (resume at the
same instruction), **`eax = 0`** (child's `fork()` return), all other GPRs +
user segments copied, `cr3 = vmm_copyVirtualSpace(newpid)` (COW copy). The child
gets its **own** 8 KB kernel stack from `schedNewTask` (no longer shares the
parent's `esp0`). `sched_ready` then calls `md_setup_initial_frame`, which builds
a trapframe from `md_tss`; the child's first `cpu_switch` returns through
`ret_from_fork` and `iret`s back to user with `eax=0`.

---

### FPU State — Lazy Save/Restore

`sys/arch/i386/idt.c` (`releng/2`) `mathStateRestore` / `_int7`. The x87 state
(`md.md_i387`) is lazily switched: `switch_to` sets `CR0.TS`; the first FP
instruction in the new task raises `#NM` (INT 7); `_int7` does `clts` and, if the
FPU owner changed, `mathStateRestore` `fnsave`s the previous owner, `frstor`s (or
`fninit`s) the current task, and sets `_usedMath = _current`. Tasks that never
use FP pay nothing.

---

### v86 / BIOS calls

VESA and other BIOS services run in virtual-8086 mode. v86 tasks are software-
switched in via a VM86 `iret` frame (`enter_vm86`). BIOS privileged instructions
(`INT`, `CLI`/`STI`, `IN`/`OUT`, …) fault with `#GP`; the handler `__gpf` is now
an **ordinary interrupt gate** (formerly a hardware task gate) that syncs the
trapframe ↔ `md_tss`, virtualizes the instruction, and `iret`s back to VM86.
Results land in `md_tss` for `bioscall` to read. The timer is masked while a v86
task runs.

---

### execThread — Kernel Threads

`sys/arch/i386/i386_exec.c` (`releng/2`). Kernel threads set ring-0
selectors in `md_tss` (`cs=0x08`, `ds/es/ss=0x10`) and run on the shared kernel
page directory with no user component. `md_setup_initial_frame` builds a ring-0
entry frame (no `iret`); `cpu_switch` seeds the kernel segments so the thread
runs with `%gs=0x10`.

---

### Status of earlier "suggested improvements"

The previous version of this document listed UP-only weaknesses of the hardware
switch. Several are now resolved by the software-switch conversion:

| Item | Status |
|------|--------|
| Per-task kernel stack (was: child shared parent's `esp0`) | **Done** — `schedNewTask` allocates an 8 KB stack per task |
| Single shared GDT[4] TSS / `ljmp` race window | **Gone** — no `ljmp`; GDT[4] is static |
| Hardware task switching incompatible with SMP | **Removed** — software switch is the SMP prerequisite |
| Coarse scheduling quantum | **Improved** — per-priority quanta + run-queues (`sched_switch.c`) |
| Fork spin-wait / `FORK` state | **Improved** — run-queue based dispatch |
| LDT load on every hardware switch | **N/A** — no hardware switch; LDT still used for user TLS (`%gs=0x0F`) |
| `io_space[8192]` per task in `tssStruct` | **Still open** — `md_tss` is now mostly vestigial; candidate for slimming (see [i386-page-directory-map.md](i386-page-directory-map.md) cleanup notes) |

See [i386-page-directory-map.md](i386-page-directory-map.md) for the memory map
and the remaining cleanup / multi-arch to-do, and
[vmm.md](vmm.md) for VMM behaviour.
