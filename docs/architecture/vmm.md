# UbixOS Virtual Memory Manager

**Source:** `sys/vmm/`

---

## Memory Layout

Each process has a private 4 GB virtual address space divided into three regions:

| Range | Description |
|-------|-------------|
| `0x00000000 – 0x000FFFFF` | Shared read-only (1:1 identity-mapped). Contains BIOS data, VGA frame buffer, and kernel boot code. |
| `0x00100000 – 0xBFFFFFFF` | Per-process region. Private page tables; available for code, data, heap, and stack. |
| `0xC0000000 – 0xFFFFFFFF` | Kernel-only region. Shared across all processes but only accessible at ring 0. User-space access triggers a GPF. |

### Page Directory Layout (PDE indices)

| PDE index | Virtual range | Mapped to |
|-----------|--------------|-----------|
| 0 | `0x00000000 – 0x003FFFFF` | Kernel code |
| 1 | `0x00400000 – 0x007FEFFF` | Kernel code (continued); `0x007FF000–0x007FFFFF` = `USER_LDT` |
| 2–767 | `0x00800000 – 0xBFFFFFFF` | Per-process user space |
| 768 | `0xC0000000 – 0xC03FFFFF` | Page directories |
| 769 | `0xC0400000 – 0xC07FFFFF` | Page tables |
| 770 | `0xC0800000 – 0xC0BFFFFF` | Kernel space start |
| 1015 | `0xFDC00000 – 0xFDFFFFFF` | Kernel space end |
| 1016 | `0xFE000000 – 0xFE3FFFFF` | Kernel stack start |
| 1023 | `0xFFC00000 – 0xFFFFFFFF` | Kernel stack end |

PDE entry 0x300 (page 0x768) is the self-referencing slot that maps the top 1 GB kernel region.

---

## Key Functions

### `vmmInit()`

Top-level initialization entry point. Calls `vmmMemMapInit()` then `vmmPagingInit()`. Halts on failure.

### `vmmMemMapInit()`

Builds the physical page-frame map — a linked list of all available physical pages. Each entry tracks:

- Physical frame address
- Owning PID
- Reference count (for COW sharing)
- Status flags (`free`, `allocated`, `cow-pending`)

### `vmmPagingInit()`

Enables hardware paging. Sets up the kernel's initial page directory, identity-maps the lower 1 MB, and maps the physical frame list into the top 1 GB so the kernel can reach it from any process context.

### `vmmCreateVirtualSpace(pid)`

Allocates and initializes a fresh page directory for `pid`. Returns the physical base address. The shared lower 1 MB and top 1 GB kernel mappings are pre-installed; everything between 1 MB and 3 GB starts unmapped.

### `vmmCopyVirtualSpace(pid)`

Forks the address space of `pid` using copy-on-write (COW):

1. The entire 2 MB – 3 GB range is duplicated at the page-table level.
2. All pages in that range are marked read-only and COW-pending in both parent and child; no physical memory is copied.
3. On the first write to any shared page, a page fault fires.
4. The page-fault handler allocates a new physical frame, copies the content, clears the COW flag, and rewrites the faulting PTE to the new frame.

---

## Page-Fault Handling

**Source:** `sys/vmm/page_fault.S`, `sys/vmm/pagefault.c`

x86 exception 14 (page fault) is routed to the VMM handler, which distinguishes three cases:

| Case | Condition | Action |
|------|-----------|--------|
| COW fault | Write to a shared COW page | Allocate new frame, copy, update PTE, resume |
| Demand-zero | First access to an allocated but unmapped page | Allocate zeroed frame, map, resume |
| Invalid access | Unmapped or protected region | Deliver `SIGSEGV` to faulting process |
