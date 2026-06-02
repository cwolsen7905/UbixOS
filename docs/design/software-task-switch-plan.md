# Software Task Switching — Design & Plan

## Goal

Replace i386 **hardware task switching** (`ljmp $0x20` to a per-task TSS) with
**software context switching** (save/restore callee-saved regs + kernel ESP,
swap CR3, update a single TSS's `esp0`). This is the mainstream design
(Linux/FreeBSD), removes the per-task TSS, and — critically — frees the segment
registers from being per-task TSS state, which unblocks `%gs` per-CPU for SMP
(see [[project_smp_hw_taskswitch]]).

## Why

UbixOS schedules with `asm("ljmp $0x20,$0")` ([sched_switch.c](../../sys/arch/i386/sched_switch.c#L250)):
TR is loaded once at boot (start.S), then `sched()` re-points `ubixGDT[4]`'s base
at `_current->md.md_tss` before each `ljmp`. The CPU saves/loads ALL state
(including every segment register) to/from the per-task TSS. That makes `%gs`
per-*task* state, so it cannot hold a CPU-constant per-CPU base. Software
switching keeps segment registers under our control.

## Current mechanism (mapped)

- **TSS struct**: [sys/include/sys/tss.h](../../sys/include/sys/tss.h) — `esp0`@0x04,
  `ss0`@0x08, `cr3`@0x1C, `eip`@0x20, `eflags`@0x24, GP regs @0x28-0x47,
  `esp`@0x38, segs @0x48-0x5C.
- **`md`**: `struct md_proc { struct tssStruct md_tss; struct i387Struct md_i387; }`
  ([machine/proc.h](../../sys/include/machine/proc.h)).
- **Kernel stack**: every task gets `kmalloc(8192)` in `schedNewTask`
  ([sched_core.c:110](../../sys/kern/sched_core.c#L110)); `md_tss.esp0 = kernelStack + 8192`,
  `ss0 = 0x10`. TR loaded once at [start.S:151](../../sys/init/start.S#L151) (`ltr 0x20`).
- **Task creation** fills `md_tss` with entry eip / user esp / cr3 / segs / eflags:
  - `execThread` (kernel thread): cs=0x08, ds/es/fs/gs/ss=0x10, eip=thread fn, eflags=0x206.
  - `execFile` (user ELF): cs=0x2B, ds/es/fs/ss=0x33, gs=0x0F, eip=entry, esp=STACK_ADDR.
  - `sys_fork`: copies parent trapframe into child `md_tss`, eax=0, cr3=copied space.
- **Signals** already use the trapframe on the user/kernel stack, NOT the TSS —
  no change needed ([signal.c](../../sys/posix/signal.c)).
- **trapframe** ([sys/include/sys/trap.h](../../sys/include/sys/trap.h)): gs,fs,es,ds,
  edi,esi,ebp,isp,ebx,edx,ecx,eax,trapno,err,eip,cs,eflags,esp,ss (low→high).

## Target mechanism

One **kernel TSS per CPU** (for now: the existing boot TSS). TR stays loaded;
its descriptor base is NOT patched anymore. On each switch we only write
`tss->esp0 = next's kernel-stack top` so a later ring3→ring0 interrupt lands on
the right kernel stack.

A task's resume state lives on **its own kernel stack**, pointed to by a new
field `md.kernel_esp`. Saved there (top→down): the callee-saved regs and a
return address. switch_to pops them and `ret`s — into the task's previous
switch_to call site (existing task) or into `ret_from_fork` (new task).

### New field

`struct md_proc` gains `u_int32_t md_kstack;` — saved kernel ESP for the switch.
(Keep `md_tss` for now: it still carries the creation-time description and the
single TSS still needs the type; we just stop hardware-switching through it.)

### `switch_to(kTask_t *prev, kTask_t *next)` (new asm, sched_switch.S)

```
push %ebx; push %esi; push %edi; push %ebp      # save callee-saved of prev
mov %esp, prev->md.md_kstack                     # save prev kernel ESP
mov next->md.md_kstack, %esp                     # load next kernel ESP
mov next->md.md_tss.cr3, %eax                    # swap address space if changed
mov %cr3,%ecx; cmp %eax,%ecx; je 1f; mov %eax,%cr3; 1:
mov next->md.md_tss.esp0, %eax                    # update single TSS esp0
mov %eax, kernel_tss.esp0
pop %ebp; pop %edi; pop %esi; pop %ebx
ret
```

### `ret_from_fork` trampoline (new asm)

First-ever entry of a freshly created task. The crafted stack below the saved
callee regs holds a full trapframe; the trampoline runs the standard epilogue
(`pop gs/fs/es/ds; popa; add $8,%esp; iret`) to enter user (or kernel) mode at
the task's entry point. Same epilogue the syscall/trap stubs already use.

### Choke point correction

`schedNewTask` only allocates the blank task + kernel stack; callers fill
`md_tss` AFTER it returns. The real "task is now runnable" choke point is
**`sched_ready(task)`** ([sched_core.c](../../sys/kern/sched_core.c)) — reached by
`sched_setStatus(id, READY)` (execThread:262, execFile:660, fork) and called
directly by bioscall:105. Build the initial frame there, guarded to fire once
(task transitioning out of NEW / `md_kstack == 0`).

### v86 / BIOS complication (decision needed)

v86 tasks ([bioscall.c](../../sys/arch/i386/bioscall.c)) are coupled to hardware
switching in TWO ways:
1. **Entry**: `eflags = 2|IF|VM` — entering VM86 via `iret` needs the VM bit set
   and the extended iret frame (iret in VM86 also pops es/ds/fs/gs).
2. **Result readback**: bioscall reads BIOS output from `md_tss.eax/ebx/...`
   AFTER the task completes (bioscall.c:111) — that only works because the
   hardware switch writes live regs into the TSS. Software switching leaves
   `md_tss` stale, so results must instead be captured from the v86 task's
   trapframe when it traps back into `trap()` (trap.c:83 already special-cases
   v86) and stashed where bioscall can read them.

Two ways to handle v86: (A) **rework** — build the VM86 iret frame and capture
results from the trapframe; full software switch everywhere. (B) **hybrid** —
keep `ljmp`/TSS for v86 tasks only, software-switch everything else; simpler but
the v86 hardware switch still clobbers %gs transiently (acceptable since v86
runs only in systemtask for short BIOS calls, and %gs per-CPU is reloaded on
every kernel entry).

### Initial kernel stack (built in `sched_ready`, guarded once per task)

After `kmalloc(8192)`, lay out from the top down, from the task's `md_tss`:
```
[ top ]  trapframe { ss,esp,eflags,cs,eip,  err=0,trapno=0,  eax..edi, ds,es,fs,gs }
         return address = ret_from_fork
         saved ebp=0, edi=0, esi=0, ebx=0      <- md_kstack points here
```
`iret` consumes eip/cs/eflags(/esp/ss when cs is ring3). Kernel threads have
cs=0x08 (same-ring iret, no esp/ss pop) — so for ring0 entries the frame omits
the esp/ss words. Handle ring0 vs ring3 by cs RPL when building the frame.

### `sched()` change

Replace [sched_switch.c:238-254](../../sys/arch/i386/sched_switch.c#L238) (cli;
patch ubixGDT[4]; `ljmp`; sti) with: set `prev=outgoing`, `_current=next`,
`switch_to(prev, next)`. No GDT patch, no ljmp.

## Phasing (each step builds + multi-boot tests; flip is isolated)

1. **Add `md_kstack` + `ret_from_fork` + initial-frame builder in schedNewTask**
   — all DORMANT (hardware `ljmp` still live; the crafted frame is unused data
   on the new task's stack). Build + boot must be behavior-neutral.
2. **Add `switch_to` asm** — still dormant (not called). Build.
3. **FLIP `sched()`**: ljmp → switch_to; stop patching ubixGDT[4]; keep the boot
   TSS as the single kernel TSS and write its esp0 in switch_to. **The all-or-
   nothing step.** Heavy multi-boot test (login, fork/exec, term, signals). If
   broken, revert just this step.
4. **Cleanup**: drop dead GDT[4] re-pointing, confirm TR untouched, document.
5. **(Milestone B, separate)** Add `%gs` per-CPU current on top — now safe
   because software switch never clobbers `%gs`. Reuses the committed scaffold
   (db7ee038f) + per-entry `%gs` reload. Then per-AP and scheduler dispatch.

## Risks & mitigations

- **Initial frame layout off-by-one** → first switch into a new task triple-faults.
  Mitigate: build frame from the SAME field order as `struct trapframe`; test
  fork/exec early; the flip is one revertible step.
- **CR3 swap while on kernel stack** — kernel VA (incl. kmalloc'd kernel stacks)
  is shared across all address spaces, so the stack stays mapped across the
  `mov %cr3`. Verify kernelStack is in shared kernel VA.
- **First switch out of the boot/idle context** — the outgoing task at the first
  `switch_to` must have a valid `md_kstack` slot; it's written by switch_to's
  save half, so any current `_current` works.
- **FPU state** (`md_i387`, lazy `_int7`) — unchanged; `_current` stays global
  through Milestone A, so `mathStateRestore` is unaffected.
- **v86 tasks** (`oInfo.v86Task`) used VM86 eflags via TSS — verify the crafted
  trapframe sets eflags VM bit for v86 entries, or keep ljmp for v86 only.
