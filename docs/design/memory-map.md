# UbixOS Memory Map (canonical reference)

This is the single source of truth for the UbixOS address-space layout. It
documents the **current i386 map**, separates **machine-independent intent**
(MI) from **machine-dependent addresses** (MD) for the eventual x86-64 / ARM
ports, and flags **cleanup opportunities** discovered along the way.

> Status: descriptive (matches the code as of the software-task-switch work).
> The "Cleanup / multi-arch notes" section is the to-do list for reorganizing.

All address constants live in [sys/include/vmm/vmm.h](../../sys/include/vmm/vmm.h)
and [sys/include/vmm/paging.h](../../sys/include/vmm/paging.h) unless noted.

---

## i386 virtual address space (per process)

Each process has its own page directory (CR3). The **low 4 MB is identity
mapped** in every space; the kernel half (≥ `VMM_KERN_START`) is shared and
re-synced across spaces on fork / `vmm_share_region`.

| Range | Size | Constant | Contents | Shared? |
|-------|------|----------|----------|---------|
| `0x00000000`–`0x003FFFFF` | 4 MB | (identity map) | Low physical RAM identity-mapped: real-mode IVT, BIOS/VGA, GDT/IDT, kernel TSS (`0x4200`), AP GDT (`0x20000`), v86 stub (`0x600`)/stack (`0x1000`) | yes (identity) |
| `0x00000000`–`0x007FEFFF` | ~8 MB | `VMM_KERN_CODE_START`..`_END` | "kernel code" window (overlaps identity map; legacy naming) | — |
| `0x007FF000`–`0x007FFFFF` | 4 KB | `VMM_USER_LDT` | **Per-process LDT** (LDT[1] = user TLS for musl `%gs`) | per-process |
| `0x00800000`–`0xBFFFFFFF` | ~3 GB | `VMM_USER_START`..`_END` | **User space**: ELF image, heap, mmap, shared regions | per-process |
| `0xBFFFFFFF` | — | `STACK_ADDR` | Default user stack top (grows down) | per-process |
| `0x05A00000` | 4 KB | `VMM_CHILD_PD_WINDOW` | Temp window to map a child/peer PD during fork / share_region (**note: sits inside the user range!**) | transient |
| `0xC0000000`–`0xC03FFFFF` | 4 MB | `PT_BASE_ADDR` / `VMM_PAGE_DIRS` | Recursive page-**table** self-map (all PTs visible here) | per-process |
| `0xC0400000`–`0xC07FFFFF` | 4 MB | `PD_BASE_ADDR` / `VMM_PAGE_DIR` | Recursive page-**directory** self-map (PD visible here) | per-process |
| `0xC0800000`–`0xFDFFFFFF` | ~864 MB | `VMM_KERN_START`..`_END` | **Kernel space**: kmalloc heap (`vmm_get_free_malloc_page`), kernel pages. PD indices 770–1015, **synced across all address spaces**. | shared (synced) |
| `0xE6667000` | — | `vmmMemoryMapAddr` | Physical-page bitmap mapping (legacy const; see note) | shared |
| `0xFE000000`–`0xFFFFFFFF` | 32 MB | `VMM_KERN_STACK_START`..`_END` | Reserved "kernel stack" range — **PD ≥ 1016, OUTSIDE the synced range** | ⚠ not synced |
| `0xFEE00000` | 4 KB | `LAPIC_PHYS` (smp.h) | Local APIC MMIO (identity-mapped) — **PD 1019, OUTSIDE the synced range** | ⚠ not synced |

## i386 physical low memory (< 1 MB, identity-mapped)

| Phys | Contents |
|------|----------|
| `0x00000`–`0x003FF` | Real-mode IVT (needed by v86 BIOS calls; saved/restored around AP startup, which reuses `0x0`) |
| `0x00600` | v86 BIOS call stub (`cs=0x60`), entry for `biosCall` |
| `0x01000` | v86 stack (`ss=0x1000`) |
| `0x04200` | **Kernel TSS** (GDT selector `0x20`, TR) — only `ss0`/`esp0` used (ring3→ring0) |
| `0x05200`, `0x06200` | ⚠ **dead** former GPF/stack-fault task-gate TSSes (removed from setup; GDT entries 7/8 still present) |
| `0x20000` | AP boot GDT (built by `GDT_fixer`) |
| `0xA0000`–`0xFFFFF` | VGA / BIOS ROM |
| `0x300000` | **Kernel image load address** (`ldscript.i386`; text+data+bss) |
| `page_align(_end)` | Physical page bitmap (`vmm_memMapInit`; RAM-size-independent placement) |

## Key GDT / segment layout

[sys/init/main.c](../../sys/init/main.c) `ubixGDT` (selectors):
`0x08` kcode · `0x10` kdata · `0x18` LDT · `0x20` kernel TSS · `0x28/0x2B` ucode ·
`0x30/0x33` udata · `0x38/0x40` (dead TSS descrs) · `0x48` SMP private · `0x50` user-gs/stack ·
`0x58` **`SEL_PCPU`** (per-CPU `%gs` base = `&g_pcpu[cpuid]`; [gdt.h](../../sys/include/sys/gdt.h)).
User TLS uses LDT[1] via `%gs = 0x0F`.

---

## Machine-independent intent vs machine-dependent addresses

For the x86-64 / ARM ports, the **intent** is portable; the **addresses** are not.

| MI concept | i386 address | x86-64 equivalent (future) |
|------------|--------------|----------------------------|
| Identity / low map | low 4 MB | early boot only; kernel uses high-half direct map |
| User space | `0x00800000`–`0xBFFFFFFF` | `0x0`–`0x00007FFF_FFFFFFFF` (canonical low) |
| Kernel space | `0xC0800000`+ | `0xFFFF8000_00000000`+ (canonical high) |
| Per-process TLS | LDT[1] via `%gs` | `%fs` base (MSR `FSBASE`); no LDT |
| Per-CPU area | `%gs` = `SEL_PCPU` GDT desc | `%gs` base (MSR `GSBASE` + `swapgs`) |
| Recursive PT/PD self-map | `0xC0000000`/`0xC0400000` | recursive slot or direct map |
| Page-directory base reg | CR3 | CR3 (4-level) |

The portable split should be a per-arch `machine/vmparam.h` (FreeBSD model)
defining these regions; MI code refers to the names, never the literals.

---

## Cleanup / multi-arch notes (the reorganization to-do)

1. **`//TMP` and "Find Out What This Was For" comments** in `vmm.h`/`paging.h`
   (`VMM_KERN_START //TMP ADDED 1000`, `PD_BASE_ADDR2`) — resolve and document.
2. **LDT placement** (`VMM_USER_LDT = 0x7FF000`) sits *below* `VMM_USER_START`
   in a gap between "kernel code" and user space. It is per-process and its
   reachability caused real bugs. Consider moving it into a clearly-owned
   per-process region (or eliminating the LDT entirely — see #6).
3. **`VMM_CHILD_PD_WINDOW = 0x5A00000`** is a transient kernel window placed
   *inside* the user VA range (90 MB). Move it into kernel VA.
4. **LAPIC (`0xFEE00000`) and `VMM_KERN_STACK` (`0xFE000000+`)** are above
   `VMM_KERN_END` (PD 1015), i.e. **outside the globally-synced kernel PD
   range (770–1015)**. Either extend the synced range to cover them or relocate
   them into it, so per-CPU LAPIC access works from any address space (needed
   for the SMP LAPIC timer / IPIs).
5. **Dead TSS GDT descriptors** (entries 7/8, bases `0x5200`/`0x6200`) — now
   unused after the move to software task switching; can be reclaimed.
6. **i386-only mechanisms to retire for portability**: hardware task switching
   (done — software switch landed), the **LDT-based TLS** (x86-64 uses `%fs`
   base MSR, no LDT), and **VM86/BIOS** (no real mode in long mode — VESA must
   move to a different mechanism on 64-bit).
7. **Consolidate magic numbers**: low-mem addresses (`0x600`, `0x1000`,
   `0x4200`, `0x20000`, `0x0` AP trampoline) are hardcoded across `bioscall.c`,
   `idt.c`, `start.S`, `smp.c`, `context_switch.c`. Name them in one header.
8. **Per-CPU area in the map**: define a dedicated kernel region for the
   `g_pcpu[]` array / per-CPU data so the `%gs`-per-CPU work (Milestone B) has a
   clean, documented home rather than ad-hoc placement.
