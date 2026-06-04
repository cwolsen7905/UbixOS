# UbixOS Task Switching Internals

## Overview

UbixOS implements preemptive multitasking on i386 using **software context
switching** — the kernel explicitly saves the outgoing task's state and loads
the incoming one around a stack swap, the same approach used by Linux and
FreeBSD. (Prior to 2026 the kernel used i386 *hardware* task switching — a
single `ljmp` to a per-task TSS — but that was replaced because hardware task
switching makes the segment registers per-task TSS state, which is fundamentally
incompatible with `%gs`-based per-CPU data and therefore with SMP. See
[../design/completed/software-task-switch-plan.md](../design/completed/software-task-switch-plan.md).)

The switch lives in [sys/arch/i386/context_switch.c](../../sys/arch/i386/context_switch.c)
(`switch_to`, `cpu_switch`, `md_setup_initial_frame`, `ret_from_fork`) and is
driven from `sched()` in [sys/arch/i386/sched_switch.c](../../sys/arch/i386/sched_switch.c).

---

## Data Structures

### Task Control Block — `kTask_t` / `taskStruct`

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

### The kernel TSS

There is exactly **one TSS** (boot TSS at physical `0x4200`, GDT selector `0x20`,
`TR` loaded once at boot). The x86 architecture requires a TSS only to supply
`ss0`/`esp0` when the CPU takes an interrupt/trap from ring 3 → ring 0. `switch_to`
updates `kernelTSS->esp0` to the incoming task's kernel stack on every switch.
GDT[4]'s base is no longer rewritten per task; the per-task TSSes and the old
GPF/stack-fault task-gate TSSes (`0x5200`/`0x6200`) are gone.

### Trapframe — `struct trapframe`

[sys/include/sys/trap.h](../../sys/include/sys/trap.h). On kernel entry the CPU + the
IDT stub push a trapframe onto the kernel stack at `esp0`:

```
Pushed by IDT stub:  gs, fs, es, ds, edi, esi, ebp, esp, ebx, edx, ecx, eax, trapno
Pushed by CPU:       err, eip, cs, eflags
Ring-crossing only:  user_esp, user_ss   (present only when entering from ring 3)
```

`sys_fork` snapshots `td->frame` to build the child's resume state.

---

## The Software Switch

### `switch_to(prev, next)` and `cpu_switch`

[context_switch.c](../../sys/arch/i386/context_switch.c). `sched()` calls
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

### New tasks — `md_setup_initial_frame` + `ret_from_fork`

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

### `sched()`

[sched_switch.c](../../sys/arch/i386/sched_switch.c) picks the next task (O(1) via
the per-priority run-queues + `ready_mask`), captures `prev = _current`, sets
`_current = next`, then — under `cli`, after releasing the scheduler lock —
calls `switch_to(prev, _current)`. The outgoing task resumes here on its next
slot; `cpu_switch` restored its EFLAGS with IF cleared (the `cli` above), so
`sched()` re-enables interrupts (`sti`) on resume.

---

## Fork

[sys/arch/i386/fork.c](../../sys/arch/i386/fork.c). `sys_fork` fills the child's
`md_tss` from the parent's trapframe (`td->frame`): `eip = tf_eip` (resume at the
same instruction), **`eax = 0`** (child's `fork()` return), all other GPRs +
user segments copied, `cr3 = vmm_copyVirtualSpace(newpid)` (COW copy). The child
gets its **own** 8 KB kernel stack from `schedNewTask` (no longer shares the
parent's `esp0`). `sched_ready` then calls `md_setup_initial_frame`, which builds
a trapframe from `md_tss`; the child's first `cpu_switch` returns through
`ret_from_fork` and `iret`s back to user with `eax=0`.

---

## FPU State — Lazy Save/Restore

[idt.c](../../sys/arch/i386/idt.c) `mathStateRestore` / `_int7`. The x87 state
(`md.md_i387`) is lazily switched: `switch_to` sets `CR0.TS`; the first FP
instruction in the new task raises `#NM` (INT 7); `_int7` does `clts` and, if the
FPU owner changed, `mathStateRestore` `fnsave`s the previous owner, `frstor`s (or
`fninit`s) the current task, and sets `_usedMath = _current`. Tasks that never
use FP pay nothing.

---

## v86 / BIOS calls

VESA and other BIOS services run in virtual-8086 mode. v86 tasks are software-
switched in via a VM86 `iret` frame (`enter_vm86`). BIOS privileged instructions
(`INT`, `CLI`/`STI`, `IN`/`OUT`, …) fault with `#GP`; the handler `__gpf` is now
an **ordinary interrupt gate** (formerly a hardware task gate) that syncs the
trapframe ↔ `md_tss`, virtualizes the instruction, and `iret`s back to VM86.
Results land in `md_tss` for `bioscall` to read. The timer is masked while a v86
task runs.

---

## execThread — Kernel Threads

[i386_exec.c](../../sys/arch/i386/i386_exec.c). Kernel threads set ring-0
selectors in `md_tss` (`cs=0x08`, `ds/es/ss=0x10`) and run on the shared kernel
page directory with no user component. `md_setup_initial_frame` builds a ring-0
entry frame (no `iret`); `cpu_switch` seeds the kernel segments so the thread
runs with `%gs=0x10`.

---

## Status of earlier "suggested improvements"

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
