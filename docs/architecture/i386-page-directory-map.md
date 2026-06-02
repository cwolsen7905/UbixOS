# i386 Page Directory Map

The i386 MMU uses a two-level page table. The top-level **page directory**
has 1024 entries (PDEs), each covering 4 MB of virtual address space. This
document maps every PDE index to its virtual range and purpose.

PDEs 0–767 cover the lower 3 GB (user + low kernel space). PDEs 768–1023
cover the upper 1 GB (kernel-only).

---

## Named / Significant PDEs

| PDE | Virtual start | Virtual end | Notes |
|-----|--------------|-------------|-------|
| 0 | `0x00000000` | `0x003FFFFF` | Identity-mapped 4 MB (all 1024 PT entries). First 1 MB: ISA/VGA/BIOS. `0x101000–0x201FFF`: page bitmap staging. `0x300000–0x392000`: kernel image. |
| 1 | `0x00400000` | `0x007FFFFF` | Allocated but initially empty; `0x007FF000–0x007FFFFF` = `USER_LDT` |
| 2–767 | `0x00800000` | `0xBFFFFFFF` | Per-process user space (private page tables per process) |
| 768 | `0xC0000000` | `0xC03FFFFF` | Mapped page tables — unique to each address space |
| 769 | `0xC0400000` | `0xC07FFFFF` | Mapped page directory |
| 770 | `0xC0800000` | `0xC0BFFFFF` | Kernel memory start |
| 771–1015 | `0xC0C00000` | `0xFDFFFFFF` | Kernel memory (unassigned; available for expansion) |
| 1016 | `0xFE000000` | `0xFE3FFFFF` | Kernel stack start |
| 1017–1023 | `0xFE400000` | `0xFFFFFFFF` | Kernel stack continued |

---

## Full Per-Process User Space (PDEs 2–767)

Each of these PDEs covers a 4 MB window of the per-process virtual address
space. The kernel does not assign fixed sub-regions here; the ELF loader and
stack allocator place segments wherever the address space is free.

| PDE range | Virtual range | Notes |
|-----------|--------------|-------|
| 2–767 | `0x00800000 – 0xBFFFFFFF` | Process code, data, heap, stack (768 × 4 MB = 3 GB − 8 MB) |

---

## Kernel Space Detail (PDEs 768–1023)

| PDE | Virtual start | Virtual end | Purpose |
|-----|--------------|-------------|---------|
| 768 | `0xC0000000` | `0xC03FFFFF` | Self-referencing page table map (all page tables visible here) |
| 769 | `0xC0400000` | `0xC07FFFFF` | Page directory itself mapped here |
| 770 | `0xC0800000` | `0xC0BFFFFF` | Start of kernel static allocations (`kernelPageDirectory`, `systemVitals`, etc.) |
| 771–1015 | `0xC0C00000` | `0xFDFFFFFF` | Unassigned kernel space |
| 1016 | `0xFE000000` | `0xFE3FFFFF` | Kernel stack |
| 1017–1022 | `0xFE400000` | `0xFFBFFFFF` | Kernel stack (continued) |
| 1023 | `0xFFC00000` | `0xFFFFFFFF` | Kernel stack top |

---

## Notes on the Self-Referencing Entry

PDE 768 (`0xC0000000`) points back to the page directory itself. This means
that from kernel mode:

- The page directory is readable at `0xC0400000` (PDE 769 × 4 MB).
- Any page table is readable by indexing into `0xC0000000 + (PDE × 4096)`.

This is the standard recursive page-table trick used to avoid needing a
separate virtual mapping for the page tables.

---

## Physical Low Memory (< 1 MB, identity-mapped via PDE 0)

| Phys | Contents |
|------|----------|
| `0x00000`–`0x003FF` | Real-mode IVT (needed by v86 BIOS calls; saved/restored around AP startup, which reuses `0x0`) |
| `0x00000` | AP boot trampoline destination (`AP_TRAMPOLINE_PHYS`, copied at SMP bring-up) |
| `0x00600` | v86 BIOS-call stub (`cs=0x60`) |
| `0x01000` | v86 stack (`ss=0x1000`) |
| `0x04200` | **Kernel TSS** (GDT selector `0x20`, TR) — the *only* live TSS; used solely for `ss0`/`esp0` on ring3→ring0 entry |
| `0x05200`, `0x06200` | **dead** — former GPF / stack-fault hardware-task-gate TSSes (removed after the software-switch conversion; GDT descriptors 7/8 still present) |
| `0x20000` | AP boot GDT (built by `GDT_fixer`) |
| `0xA0000`–`0xFFFFF` | VGA / BIOS ROM |
| `0x300000` | **Kernel image load address** (`sys/compile/ldscript.i386`) |
| `page_align(_end)` | Physical-page bitmap (`vmm_memMapInit`; RAM-size-independent placement) |

## GDT Selectors

[sys/init/main.c](../../sys/init/main.c) `ubixGDT`:
`0x08` kcode · `0x10` kdata · `0x18` LDT · `0x20` kernel TSS · `0x28/0x2B` ucode ·
`0x30/0x33` udata · `0x38`/`0x40` (now-dead TSS descriptors) · `0x48` SMP private ·
`0x50` user-gs/stack · `0x58` **`SEL_PCPU`** (per-CPU `%gs` base = `&g_pcpu[cpuid]`,
[gdt.h](../../sys/include/sys/gdt.h)). User TLS uses LDT[1] via `%gs = 0x0F`.

> Since the move to **software task switching**, no per-task / per-handler TSS is
> loaded by hardware; segment registers are saved/restored by `cpu_switch`, and
> GDT[4] stays pointed at the single kernel TSS (`0x4200`). See
> [task-switching.md](task-switching.md).

---

## Cleanup / Multi-Architecture Notes

Known layout issues and the portability split (x86-64 / ARM). Each region's
*intent* is machine-independent; the *addresses* are not — the port should hoist
them into a per-arch `machine/vmm_layout.h`
([../design/cross-arch-plan.md](../design/cross-arch-plan.md) Phase 9).

1. **`//TMP` / "Find Out What This Was For" comments** in `vmm.h`/`paging.h`
   (`VMM_KERN_START //TMP ADDED 1000`, `PD_BASE_ADDR2`) — resolve and document.
2. **LDT placement** (`VMM_USER_LDT = 0x7FF000`, PDE 1) sits in the gap *below*
   `VMM_USER_START`; its per-process reachability has caused real bugs. Consider
   relocating, or eliminating the LDT entirely (x86-64 uses an `%fs`-base MSR for
   TLS, no LDT).
3. **`VMM_CHILD_PD_WINDOW = 0x5A00000`** — a transient kernel mapping window
   placed *inside* the user VA range (PDE 22). Move into kernel VA.
4. **LAPIC (`0xFEE00000`, PDE 1019) and `VMM_KERN_STACK` (PDE 1016+)** are
   *above* `VMM_KERN_END` (PDE 1015) — outside the globally-synced kernel PD
   range (770–1015). Extend the sync or relocate so per-CPU LAPIC access works
   from any address space (needed for the SMP LAPIC timer / IPIs).
5. **Dead TSS GDT descriptors** (entries 7/8, bases `0x5200`/`0x6200`) — reclaim.
6. **i386-only mechanisms to retire for portability**: hardware task switching
   (done), LDT-based TLS, and VM86/BIOS (no real mode in long mode — VESA must
   move to a different mechanism on 64-bit).
7. **Per-CPU area**: give `g_pcpu[]` / per-CPU data a dedicated, documented
   kernel region so the `%gs`-per-CPU work has a clean home.

---

## Source Files

- `sys/vmm/` — VMM implementation
- `sys/include/vmm/vmm.h`, `sys/include/vmm/paging.h` — layout constants
- See also: [vmm.md](vmm.md) (VMM behaviour), [task-switching.md](task-switching.md),
  and [../audit/vmm-audit.md](../audit/vmm-audit.md) (dated technical audit)
