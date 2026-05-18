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

## Source Files

- `sys/vmm/` — VMM implementation
- `sys/include/ubixos/vmm.h` — VMM constants (`VMM_USER_LDT`, etc.)
- See also: [vmm.md](vmm.md) for function-level documentation
