# UbixOS USB Support and newbus-lite Device Model

## Overview

This document is the implementation plan for adding USB support to UbixOS and
evolving the PCI device model to match FreeBSD's newbus architecture (simplified
for a hobby OS).

---

## Status

| Phase | Name | Status | Notes |
|---|---|---|---|
| 1 | newbus-lite device model | Done | sys/include/sys/bus.h, sys/sys/bus.c |
| 2 | PCI bus driver refactor (e1000) | Done | pci_device_from_cfg(), e1000_ubx_probe/attach, pci_drv_table[] |
| 2b | Migrate remaining PCI drivers (lnc only) | Done | lnc_ubx_probe/attach, lnc_ubx_driver in lnc.c; hd.c is ISA-style, moves to 2c |
| 2c | PCI IDE probe + ISA bus driver + migrate ISA drivers | Done | ide_ubx_driver in hd.c; isa_bus.h/isa_bus.c; atkbd+mouse migrated; fdc/ne2k/rs232 remain commented out |
| 2d | Migrate devfs / VFS device lookup | Done | ubx_blk_ops wrappers in hd.c/fdc.c; all VFS/FS call sites updated |
| 2e | Remove old device_node / device_interface | Done | device.c removed from build; device.h stubbed to redirect to sys/bus.h |
| 3 | DMA-safe allocator | Done | sys/include/sys/dma_mem.h + sys/sys/dma_mem.c; dma_alloc/dma_free with PAGE_CACHE_DISABLED |
| 4 | UHCI host controller | Not started | |
| 5 | USB core + enumeration | Not started | |
| 6 | HID keyboard | Not started | |
| 7 | USB mass storage | Not started | |

---

## Standing Rules

These apply to every phase and every driver without exception.

**Driver ownership:** Each driver owns its own `probe`, `attach`, `detach`, and
`struct ubx_driver` in its own `.c` file. Bus files (`pci.c`, `isa_bus.c`) contain
zero driver logic — only the scan loop and driver table. This is required for future
loadable modules; a module registers its `ubx_driver` at load time without touching
the bus file.

**Major/minor preservation:** Major and minor numbers are never renumbered during
migration. Only the lookup mechanism changes.

---

## FreeBSD newbus Concepts → UbixOS Simplifications

| FreeBSD concept | UbixOS equivalent |
|---|---|
| `device_t` (opaque) | `struct ubx_device *` (concrete, no accessor needed) |
| `DRIVER_MODULE` / linker sets | Plain `struct ubx_driver * const []` array per bus, NULL-terminated |
| `bus_space_*` accessors | Direct `inportWord/outportDWord` + volatile pointer — i386-only, no abstraction needed |
| `resource_list` / `bus_alloc_resource` | Fixed `dev_res[6]` array in `ubx_device`, `ubx_alloc_irq()` / `ubx_alloc_memory()` helpers |
| `device_probe / attach / detach` | Same three-method function pointer table in `struct ubx_driver` |

---

## Dependency Graph

```
Phase 1 (newbus-lite)
  └── Phase 2 (PCI refactor — e1000)
        ├── Phase 2b (PCI — lnc only; hd is ISA, goes to 2c)
        │     └── Phase 2c (ISA bus + ISA drivers)
        │           └── Phase 2d (devfs / VFS device lookup)
        │                 └── Phase 2e (delete old device_node / device_interface)
        └── Phase 3 (DMA allocator)  ← can start in parallel once Phase 1 structs are stable
              └── Phase 4 (UHCI HCD)
                    └── Phase 5 (USB core + enumeration)
                          ├── Phase 6 (HID keyboard)
                          └── Phase 7 (USB mass storage)
```

---

## Summary Table

| Phase | Name | Effort | Risk |
|---|---|---|---|
| 1 | newbus-lite device model | 1–2 weeks | Low — purely additive |
| 2 | PCI bus driver refactor (e1000) | 1 week | Medium — must not break e1000/hd boot path |
| 2b | Migrate remaining PCI drivers (hd, lnc) | 3–5 days | Low — same pattern as e1000 |
| 2c | ISA bus driver + migrate ISA drivers | 1–2 weeks | Medium — static resource declarations, no config space |
| 2d | Migrate devfs / VFS device lookup | 3–5 days | Medium — `device_find()` is called from mount and devfs |
| 2e | Remove old device_node / device_interface | 1–2 days | Low — mechanical delete once 2d is done |
| 3 | DMA-safe allocator | 3–5 days | Low — main risk is forgetting `PAGE_CACHE_DISABLED` |
| 4 | UHCI host controller | 3–4 weeks | **High** — hardware timing, ISR storm risk, critical path |
| 5 | USB core + enumeration | 2 weeks | Medium — well-specified, descriptor parsing edge cases |
| 6 | HID keyboard | 1 week | Low — boot protocol is fixed format |
| 7 | USB mass storage | 2–3 weeks | Medium — toggle state management, BOT error recovery |

**Total: ~14–19 weeks** part-time. Phase 4 is the critical path; Phases 2b–2e are the full
migration prerequisite for deprecating the old device system.

---

## Key Files

| File | Role in plan |
|---|---|
| `sys/pci/pci.c` | Starting point for Phase 2 refactor |
| `sys/include/sys/device.h` | Existing names to avoid colliding with |
| `sys/vmm/paging.c` | `vmm_remapPage()`, `vmm_findFreePage()`, `PAGE_CACHE_DISABLED` |
| `sys/isa/atkbd.c` | `kbd_ring_push()` to un-staticize for Phase 6 |
| `sys/include/ubixos/init.h` | Add `usb_init` after `pci_init` in `init_tasks[]` |
| `sys/sys/bus.c` | New file — Phase 1 framework implementation |
| `sys/sys/dma_mem.c` | New file — Phase 3 DMA allocator |
| `sys/usb/uhci.c` | New file — Phase 4 UHCI HCD |
| `sys/usb/usb.c` | New file — Phase 5 USB core |

---

## Phase 1 — newbus-lite Device Model

**Goal:** Introduce `struct ubx_device` / `struct ubx_driver` / `struct ubx_resource`
as the new bus-agnostic device framework. Additive only — the existing
`device_node` / `device_interface` ISA path in `sys/sys/device.c` is untouched.

### New Files

- `sys/include/sys/bus.h` — types and prototypes
- `sys/sys/bus.c` — `ubx_device_alloc`, `ubx_alloc_irq`, `ubx_alloc_memory`, `ubx_bus_probe_and_attach`
- `sys/include/sys/bus_resource.h` — `struct ubx_resource`, `UBX_RES_IRQ / _MEMORY / _IOPORT`

### Key Data Structures

```c
#define UBX_RES_IRQ    1
#define UBX_RES_MEMORY 2
#define UBX_RES_IOPORT 3

struct ubx_resource {
        int              r_type;    /* UBX_RES_IRQ, _MEMORY, _IOPORT */
        uint32_t         r_start;   /* phys base or IRQ number */
        uint32_t         r_size;
        volatile void   *r_vaddr;
};

#define UBX_MAX_RESOURCES 6

struct ubx_device {
        struct ubx_device  *dev_parent;
        struct ubx_driver  *dev_driver;
        void               *dev_softc;
        char                dev_nameunit[32];
        struct ubx_resource dev_res[UBX_MAX_RESOURCES];
        int                 dev_nres;
        uint16_t            dev_vendor;
        uint16_t            dev_device_id;
        uint8_t             dev_class, dev_subclass, dev_progif;
        uint8_t             dev_bus, dev_slot, dev_func;
        struct ubx_device  *dev_children, *dev_sibling;
};

struct ubx_driver {
        const char *drv_name;
        int (*drv_probe)(struct ubx_device *);   /* 0=match, -ENODEV=no match */
        int (*drv_attach)(struct ubx_device *);
        int (*drv_detach)(struct ubx_device *);  /* NULL allowed */
};
```

### Key Functions

```c
struct ubx_device *ubx_device_alloc(struct ubx_device *parent,
    const char *nameunit);
void ubx_device_free(struct ubx_device *dev);

volatile void *ubx_alloc_memory(struct ubx_device *dev,
    uint32_t phys_base, uint32_t size);
int ubx_alloc_irq(struct ubx_device *dev, uint8_t irq,
    void (*isr)(void));

int ubx_bus_probe_and_attach(struct ubx_device *dev,
    struct ubx_driver * const *drivers);
```

### Gotchas

- Existing names `device_interface` and `device_node` are taken in `sys/include/sys/device.h`.
  Use `ubx_device` / `ubx_driver` throughout to avoid collision.
- The stub `sys/include/sys/driver.h` has a missing semicolon — fix before including it anywhere.
- `ubx_alloc_memory()` calls `vmm_remapIOPage()` which identity-maps physical == virtual.
  For MMIO BARs above `numPages * PAGE_SIZE` (~256 MB) no page tracking entry exists — this is correct.

---

## Phase 2 — PCI Bus Driver Refactoring

**Goal:** Rewire `pci_init()` to build an `ubx_device` tree instead of calling driver
`init()` functions directly. The e1000 driver keeps working unchanged internally —
only its registration point moves.

### Driver ownership rule

**Each driver owns its own `probe`/`attach`/`detach` and exports a `ubx_driver` struct
from its own `.c` file.** `pci.c` contains zero driver logic — it only owns the scan
loop, `pci_device_from_cfg()`, and `pci_drv_table[]`.

This is required for future loadable modules: a module registers its `ubx_driver` at
load time without touching `pci.c` at all. If probe/attach live inside `pci.c` they
cannot exist in a module.

**Pattern for every driver:**

```c
/* In e1000.c — driver owns its own probe/attach */
static int e1000_ubx_probe(struct ubx_device *dev) { ... }
static int e1000_ubx_attach(struct ubx_device *dev) { ... }

struct ubx_driver e1000_ubx_driver = {
        .drv_name   = "e1000",
        .drv_probe  = e1000_ubx_probe,
        .drv_attach = e1000_ubx_attach,
};

/* In e1000.h — exported symbol */
extern struct ubx_driver e1000_ubx_driver;

/* In pci.c — zero driver logic; only the table */
static struct ubx_driver * const pci_drv_table[] = {
        &e1000_ubx_driver,
        &uhci_ubx_driver,   /* added in Phase 4 */
        NULL,
};
```

### Key Changes to `sys/pci/pci.c`

- Replace `pci_drivers[]` with `pci_drv_table[]` (NULL-terminated pointer array to `ubx_driver`).
- Add `pci_device_from_cfg()` helper — creates `ubx_device` from `pciConfig`, records
  BARs and IRQ as typed resources, frees `pciConfig`.
- `pci_init()` calls `pci_device_from_cfg()` + `ubx_bus_probe_and_attach()` per device.
- No probe/attach functions live in `pci.c`.

### Gotchas

- The TEMPORARY AMD vendor hack (`if (cfg->vendorID == 0x1022)`) in `pciProbe()` must survive.
- Multi-function devices: skip funcs 1–7 if bit 7 of `headerType` is clear.
- UHCI PCI function needs I/O Space Enable (bit 0) and Bus Master Enable (bit 2) in PCI CMD.
- `pci_init()` entry point in `init_tasks[]` is unchanged — only the internals move.

---

## Phase 2b — Migrate Remaining PCI Drivers (lnc)

**Goal:** Bring `sys/pci/lnc.c` (AMD PCnet NIC, vendor `0x1022`) into the new
`ubx_driver` table. Each driver owns its own probe/attach — nothing goes in `pci.c`.

**Note on `hd.c`:** Despite living in `sys/pci/`, the IDE driver probes fixed ISA I/O
ports (`0x1F0`, `0x170`) and never reads PCI config space BARs. It is not a PCI driver.
It is handled in Phase 2c alongside the other ISA-style drivers.

### Changes

- Add `lnc_ubx_probe()` + `lnc_ubx_attach()` to `sys/pci/lnc.c`.
  `probe()` matches vendor `0x1022` / device `0x2000` (PCnet-PCI II).
  `attach()` extracts the I/O BAR from `dev_res[]` and replaces the hardcoded
  `lnc->ioAddr = 0xD020` with the real PCI-assigned port.
  Export `struct ubx_driver lnc_ubx_driver` and declare it `extern` in `lnc.h`.
- Add `&lnc_ubx_driver` to `pci_drv_table[]` in `pci.c`. That is the only change to `pci.c`.

### Gotchas

- `lnc.c` is currently dead code — `initLNC()` is not in `init_tasks[]` and e1000
  replaced it at runtime. The migration wires it correctly to PCI but runtime testing
  requires `-device pcnet` in QEMU. Compile correctness is sufficient for this phase.
- The TEMPORARY AMD vendor hack in `pciProbe()` (clears BAR lower word, sets CMD to 0x5)
  was a workaround for lnc's I/O BAR. Verify this is still present after migration, or
  remove it if `lnc_ubx_attach()` handles CMD setup itself.

---

## Phase 2c — ISA Bus Driver + Migrate ISA Drivers

**Goal:** Introduce an ISA pseudo-bus so that ISA drivers (`atkbd`, `fdc`, `ne2k`,
`rs232`, `mouse`, `pit`, `8259`) register through `ubx_driver` with statically declared
resources instead of being called directly from `init_tasks[]`.

The same driver ownership rule from Phase 2 applies: each driver exports its own
`ubx_driver` struct. `isa_bus.c` only owns the probe loop and `isa_drv_table[]` —
it contains zero driver logic.

### New Files

- `sys/include/sys/isa_bus.h` — `struct isa_res_decl` for static I/O port / IRQ / DMA
  resource declarations
- `sys/sys/isa_bus.c` — ISA bus init, probe loop, `ubx_device` allocation per driver

### Key Concept

Unlike PCI, ISA has no config space. Resources are declared statically in each driver's
`ubx_driver` entry via a companion `struct isa_res_decl[]`:

```c
/* Companion to ubx_driver for ISA — declares fixed resources */
struct isa_res_decl {
        int      ir_type;    /* UBX_RES_IOPORT, UBX_RES_IRQ, or 0 to terminate */
        uint32_t ir_start;   /* I/O base or IRQ number */
        uint32_t ir_size;    /* I/O range size; 0 for IRQ */
};

/* Example: AT keyboard controller */
static const struct isa_res_decl atkbd_isa_res[] = {
        { UBX_RES_IOPORT, 0x60, 4 },
        { UBX_RES_IRQ,    1,    0 },
        { 0 }
};
```

`isa_bus_init()` iterates a static `isa_drv_table[]`, allocates an `ubx_device` per
entry, fills `dev_res[]` from the `isa_res_decl[]`, then calls `ubx_bus_probe_and_attach()`.

### Drivers to Migrate

| Driver | File | I/O base | IRQ |
|---|---|---|---|
| 8259 PIC | `sys/isa/8259.c` | 0x20, 0xA0 | — |
| PIT | `sys/isa/pit.c` | 0x40 | 0 |
| AT keyboard | `sys/isa/atkbd.c` | 0x60 | 1 |
| RS-232 COM1 | `sys/isa/rs232.c` | 0x3F8 | 4 |
| Floppy | `sys/isa/fdc.c` | 0x3F0 | 6 |
| NE2000 | `sys/isa/ne2k.c` | 0x300 | 10 |
| Mouse (PS/2) | `sys/isa/mouse.c` | 0x60 | 12 |

`8259.c` and `pit.c` are infrastructure drivers that must initialize before all others.
Keep them at the top of `isa_drv_table[]` and document the ordering constraint.

### Integration Points

- `isa_bus_init()` is added to `init_tasks[]` in `sys/include/ubixos/init.h`, replacing
  the individual ISA init entries currently listed there.
- The `fdc.c` call to `device_add()` is removed here; it is replaced by the new
  `ubx_driver` attach path.

### Gotchas

- The 8259 (`pic_init`) and PIT (`pit_init`) **must** run first — before any driver that
  enables interrupts. The `isa_drv_table[]` order is the initialization order; document it.
- `ne2k.c` currently probes by reading a register at 0x300 to confirm the card exists.
  That probe logic moves into `ne2k_ubx_probe()` which returns `-ENODEV` if the card
  is absent — this is how ISA devices "not present" is handled without crashing.
- Mouse and keyboard share port 0x60. That is hardware reality; the `isa_res_decl`
  entries can both list it. `ubx_alloc_memory()` does not enforce exclusivity for
  I/O ports — it is just a record, not a reservation system.

---

## Phase 2d — Migrate devfs and VFS Device Lookup

**Goal:** Replace all calls to `device_find()` and `device_add()` (the old
`device_node` / `device_interface` API) in the filesystem layer with a new
`ubx_device_find(major, minor)` that walks the `ubx_device` tree instead.

### Callers to Update

| File | What changes |
|---|---|
| `sys/fs/vfs/mount.c` | `device_find(major, minor)` → `ubx_device_find(major, minor)` |
| `sys/fs/devfs/devfs.c` | Three `device_find()` call sites → `ubx_device_find()` |
| `sys/fs/common/gpt.c` | `struct device_interface *` parameters → `struct ubx_device *` |
| `sys/pci/hd.c` | `device_add()` calls → `ubx_device_register_block()` helper |
| `sys/isa/fdc.c` | `device_add()` call → removed (handled by ISA bus in Phase 2c) |

### New Function

```c
/*
 * ubx_device_find: look up a registered block device by major/minor.
 * Replaces device_find().  Returns NULL if not found.
 */
struct ubx_device *ubx_device_find(int major, int minor);

/*
 * ubx_device_register_block: register a block device major/minor mapping
 * so ubx_device_find() can locate it.  Called from driver attach().
 * Replaces device_add() for block devices.
 */
int ubx_device_register_block(struct ubx_device *dev, int major, int minor,
    struct ubx_blk_ops *ops);
```

`struct ubx_blk_ops` replaces the function pointer fields currently scattered
across `struct device_interface`:

```c
struct ubx_blk_ops {
        int (*read)(struct ubx_device *, uint32_t lba, uint32_t count, void *buf);
        int (*write)(struct ubx_device *, uint32_t lba, uint32_t count, const void *buf);
        int (*ioctl)(struct ubx_device *, int cmd, void *arg);
};
```

### Major/Minor Numbering

Major/minor numbers are **preserved exactly** — no renumbering. The existing values
assigned by `hd.c` and `fdc.c` are carried through unchanged. Only the lookup
mechanism changes (old linked list → new `ubx_device` tree search).

The VFS and devfs callers receive an `ubx_device *` from `ubx_device_find()`.
They access the block ops via `dev->dev_blk_ops` (a new field added to `ubx_device`
in Phase 1 as a `void *` placeholder, promoted to `struct ubx_blk_ops *` here).

### Gotchas

- `gpt.c` passes `struct device_interface *` through several layers of static functions.
  This is the most invasive change in Phase 2d — each function signature must be updated
  to `struct ubx_device *`. There are six functions in `gpt.c` that need updating.
- `devfs.c` currently calls `device_find()` in three places: open, read, and write paths.
  All three must be updated atomically (one commit) to avoid a half-migrated state where
  some paths use the old list and some use the new tree.
- The `device_interface.initialized` flag (checked in `device_add()` before calling
  `init()`) has no equivalent in the new model — `ubx_driver.drv_attach()` is always
  called exactly once by `ubx_bus_probe_and_attach()`. Verify no driver relies on
  `initialized` being 0 to skip re-init.

---

## Phase 2e — Remove Old device_node / device_interface

**Goal:** Delete `sys/sys/device.c`, remove `device_node` and `device_interface` from
`sys/include/sys/device.h`, and confirm nothing links against the old API.

### Steps

1. `grep -r "device_add\|device_find\|device_remove\|device_interface\|device_node" sys/`
   — must return zero results (outside of `device.h` itself and any historical comments).
2. Delete `sys/sys/device.c`.
3. Gut `sys/include/sys/device.h` — remove the struct definitions and function prototypes.
   Keep the file as a stub with a comment pointing to `sys/bus.h` as the replacement,
   or remove it entirely if no header includes it transitively.
4. Update `sys/sys/Makefile` to remove `device.o`.
5. Full `bmake kernel` to confirm no linker errors.

### Gotchas

- `sys/fs/common/gpt.c` includes `<sys/device.h>` indirectly through other headers.
  Audit all include chains before deleting.
- `sys/include/ubixos/init.h` likely references old ISA init functions that were
  removed in Phase 2c. Confirm `init_tasks[]` is clean before Phase 2e.

---

## Phase 3 — DMA-Safe Memory Allocator

**Goal:** `dma_alloc(size, align, &buf)` / `dma_free(&buf)` for physically contiguous,
cache-disabled, identity-mapped pages usable as UHCI frame lists and TD/QH slab pools.

### New Files

- `sys/include/sys/dma_mem.h`
- `sys/sys/dma_mem.c`

### Key Data Structures and API

```c
struct dma_buf {
        void    *db_vaddr;   /* kernel virtual address */
        uint32_t db_paddr;   /* physical address (== db_vaddr on UbixOS) */
        uint32_t db_size;
};

int  dma_alloc(uint32_t size, uint32_t align, struct dma_buf *buf);
void dma_free(struct dma_buf *buf);

static inline uint32_t
dma_paddr(void *vaddr) { return ((uint32_t)vaddr); }
```

### Implementation Strategy

On UbixOS, kernel pages are identity-mapped (vaddr == paddr).

1. Round `size` up to `align` (must be power of two, max 4096).
2. Call `vmm_findFreePage(sysID)` to get a physical page.
3. Map with `vmm_remapPage()` using `KERNEL_PAGE_DEFAULT | PAGE_CACHE_DISABLED`.
4. Return virtual == physical address.

**Limit:** Phase 3 supports allocations ≤ 4096 bytes (one page). UHCI frame list is exactly
4096 bytes. QH/TD structures (16–32 bytes) are packed via a slab within a single page.

### Gotchas

- **`PAGE_CACHE_DISABLED` (bit 4 of PTE, value `0x10`) is mandatory.** Without it, the CPU's
  write-back cache makes UHCI TD status updates invisible to the CPU (and vice versa).
  This is the most common first-time USB driver bug.
- Never use `kmalloc()` for hardware-visible structures — alignment is not guaranteed.
- The new file is `dma_mem.h` to avoid collision with the ISA `dma.h`.
- `dma_free()` must call `freePage(paddr)` then `vmm_unmapPage(vaddr)` in that order.

---

## Phase 4 — UHCI Host Controller Driver

**Goal:** Working UHCI driver with frame list, QH/TD skeleton, interrupt handling, and three
transfer primitives consumed by the USB core.

### New Files

- `sys/usb/uhci.c`
- `sys/include/usb/uhci.h`

### UHCI Register Map (I/O BAR — not MMIO)

```c
#define UHCI_USBCMD    0x00  /* 16-bit: RS, HCRESET, GRESET, CF, MAXP */
#define UHCI_USBSTS    0x02  /* 16-bit: write-1-to-clear */
#define UHCI_USBINTR   0x04  /* 16-bit: interrupt enable */
#define UHCI_FRNUM     0x06  /* 16-bit: current frame number */
#define UHCI_FRBASEADD 0x08  /* 32-bit: frame list physical address */
#define UHCI_SOFMOD    0x0C  /*  8-bit: start-of-frame modify */
#define UHCI_PORTSC1   0x10  /* 16-bit: root port 1 status/control */
#define UHCI_PORTSC2   0x12  /* 16-bit: root port 2 status/control */
```

### Key Data Structures

```c
/* Queue Head — must be 16-byte aligned */
struct uhci_qh {
        uint32_t        qh_link;     /* next QH/TD phys addr | flags */
        uint32_t        qh_elt;      /* first TD phys addr | flags */
        struct uhci_qh *qh_next_sw;  /* software list linkage */
        void           *qh_softc;    /* owning transfer context */
} __attribute__((packed, aligned(16)));

/* Transfer Descriptor — must be 16-byte aligned */
struct uhci_td {
        uint32_t        td_link;     /* next TD phys addr | flags */
        uint32_t        td_status;   /* Active, SPD, error bits */
        uint32_t        td_token;    /* endpoint/addr/MaxLen/PID */
        uint32_t        td_buffer;   /* data buffer physical address */
        struct uhci_td *td_next_sw;
        void           *td_buf_vaddr;
} __attribute__((packed, aligned(16)));

#define UHCI_PTR_T  0x00000001u  /* Terminate */
#define UHCI_PTR_QH 0x00000002u  /* Points to QH (else TD) */

struct uhci_softc {
        struct ubx_device *sc_dev;
        uint16_t           sc_iobase;   /* I/O BAR base (bar[4] & ~3u) */
        uint8_t            sc_irq;
        struct dma_buf     sc_fl_buf;   /* frame list DMA buffer */
        uint32_t          *sc_fl;       /* virtual frame list pointer */
        struct uhci_qh    *sc_int_qh;   /* interrupt skeleton QH */
        struct uhci_qh    *sc_ctl_qh;   /* control skeleton QH */
        struct uhci_qh    *sc_bulk_qh;  /* bulk skeleton QH */
        struct uhci_td    *sc_td_pool;  /* TD slab */
        struct uhci_qh    *sc_qh_pool;  /* QH slab */
        int                sc_running;
};
```

### Initialization Sequence

1. Extract I/O BAR: `sc_iobase = bar[4] & ~3u` (bit 0 is the I/O indicator, not address).
2. GRESET for 10 ms; then HCRESET — poll `UHCI_USBCMD` bit 1 until clear (max 50 ms).
3. `dma_alloc(4096, 4096, &sc_fl_buf)` — initialize all 1024 entries to `UHCI_PTR_T`.
4. Allocate skeleton QHs; link them: frame list → intr QH → ctl QH → bulk QH → TERMINATE.
5. Write frame list physical addr to `UHCI_FRBASEADD`; write 0 to `UHCI_FRNUM`.
6. Enable interrupts in `UHCI_USBINTR` (IOC, resume detect, short packet).
7. `ubx_alloc_irq(dev, irq, uhci_isr_trampoline)`.
8. Set `UHCI_CMD_RS | UHCI_CMD_MAXP | UHCI_CMD_CF` in `UHCI_USBCMD`.
9. Call `uhci_root_port_init()` → triggers USB enumeration (Phase 5).

### Transfer Primitives

```c
int uhci_control_transfer(struct uhci_softc *sc, uint8_t addr, uint8_t ep,
    uint8_t *setup_pkt, void *data, uint16_t datalen, int direction);

int uhci_bulk_transfer(struct uhci_softc *sc, uint8_t addr, uint8_t ep,
    void *data, uint16_t datalen, int direction, uint8_t *toggle);

struct uhci_qh *uhci_schedule_intr(struct uhci_softc *sc, uint8_t addr,
    uint8_t ep, uint16_t maxpkt, uint32_t interval_ms,
    void (*callback)(void *arg, uint8_t *data, int len), void *arg);
```

### Gotchas

- The ISR **must** read and write-1-to-clear `UHCI_USBSTS` before any other work.
  Forgetting this causes an immediate spurious re-fire (interrupt storm).
- TD Active bit (bit 23 of `td_status`) must be set before handing a TD to hardware.
  Polling loops must have a timeout — errors leave Active=0 but set error bits.
- The I/O BAR (PIIX4 BAR[4]) has bit 0 set (I/O space indicator). Always strip with `& ~3u`.
- PCI CMD register for the UHCI function must have bit 0 (I/O Space) and bit 2 (Bus Master) set.
  `pci_bus_probe()` in Phase 2 should set these for all class 0x0C devices.
- QEMU places UHCI on IRQ 11 (8259 slave). `irqEnable(11)` already unmasks the cascade
  correctly in `sys/isa/8259.c`.

---

## Phase 5 — USB Core and Device Enumeration

**Goal:** `usb_new_device()` performs the full USB enumeration sequence: port reset →
address assignment → descriptor reading → configuration selection → class driver binding.

### New Files

- `sys/usb/usb.c`
- `sys/include/usb/usb.h` — all USB standard descriptor structs (`__attribute__((packed))`)
- `sys/include/usb/usb_driver.h` — `struct usb_driver` keyed on class/subclass/protocol

### Key Data Structures

```c
struct usb_device {
        struct uhci_softc      *ud_hc;
        uint8_t                 ud_addr;
        uint8_t                 ud_speed;         /* USB_SPEED_LOW / FULL */
        struct usb_device_desc  ud_dev_desc;
        struct usb_iface_desc   ud_iface_desc;
        struct usb_ep_desc      ud_ep[16];
        int                     ud_nep;
        struct usb_driver      *ud_driver;
        void                   *ud_drv_softc;
};

struct usb_driver {
        uint8_t  drv_class;
        uint8_t  drv_subclass;
        uint8_t  drv_protocol;
        int    (*drv_probe)(struct usb_device *);
        int    (*drv_attach)(struct usb_device *);
        int    (*drv_detach)(struct usb_device *);
};
```

### Enumeration Sequence

1. Assert PORTSC reset bit for ≥ 10 ms; read speed from PORTSC bit 8 (0=full, 1=low).
2. `GET_DESCRIPTOR(Device, 8 bytes)` at address 0 — learn `bMaxPacketSize0`.
3. `SET_ADDRESS(next_free_addr)`.
4. `GET_DESCRIPTOR(Device, 18 bytes)` at new address.
5. `GET_DESCRIPTOR(Configuration, 9 bytes)`, then full `wTotalLength` bytes.
6. Parse interface and endpoint descriptors out of the config blob.
7. `SET_CONFIGURATION(bConfigurationValue)`.
8. Walk `usb_driver_table[]` calling `drv_probe()` / `drv_attach()`.

### Gotchas

- USB address 0 is shared by all unaddressed devices — serialize enumeration (one device
  at a time). For single-port QEMU this is trivially satisfied.
- Short Packet Detection (SPD bit in the TD) must be set for the initial 8-byte
  `GET_DESCRIPTOR` so a short reply isn't treated as an error.
- All 16-bit descriptor fields are little-endian — native on i386, no byte-swapping needed.
- `wTotalLength` in the config descriptor controls how many bytes to read in the second
  `GET_DESCRIPTOR(Configuration)` call. Clamp to a reasonable max (e.g. 512) to avoid
  stack or heap overflows from malformed descriptors.

---

## Phase 6 — HID Boot-Protocol Keyboard

**Goal:** USB HID keyboard driver using the fixed 8-byte boot report format.
Feeds the existing `kbd_ring[]` in `sys/isa/atkbd.c`.

### New Files

- `sys/usb/hid_kbd.c`
- `sys/include/usb/hid.h`

### Key Data Structures

```c
struct hid_kbd_report {
        uint8_t modifier;    /* LCtrl=0, LShift=1, LAlt=2, LMeta=3, and R versions */
        uint8_t reserved;
        uint8_t keycode[6];  /* up to 6 simultaneous keys; 0 = no key */
} __attribute__((packed));

struct hid_kbd_softc {
        struct usb_device     *kd_dev;
        struct uhci_qh        *kd_intr_qh;
        struct dma_buf         kd_buf;
        struct hid_kbd_report  kd_last;
};

struct usb_driver hid_kbd_driver = {
        .drv_class    = 0x03,   /* HID */
        .drv_subclass = 0x01,   /* Boot Interface */
        .drv_protocol = 0x01,   /* Keyboard */
        .drv_probe    = hid_kbd_probe,
        .drv_attach   = hid_kbd_attach,
        .drv_detach   = hid_kbd_detach,
};
```

### Attach Sequence

1. `SET_PROTOCOL(Boot)` control transfer — selects 8-byte fixed format; no HID report
   descriptor parsing needed.
2. `SET_IDLE(0)` — device only reports on state change.
3. `dma_alloc(8, 8, &sc->kd_buf)`.
4. `uhci_schedule_intr(hc, addr, ep, maxpkt, bInterval, hid_kbd_callback, sc)`.

### Integration with `atkbd.c`

`hid_kbd_decode()` maps USB HID keycodes (USB HID Usage Tables §10) to the existing
`keyboardMap[]` scan codes via a 256-entry `usb_hid_to_atkbd_scancode[]` table, then
calls `kbd_ring_push()`.

**Required change:** `kbd_ring_push()` in `sys/isa/atkbd.c` must be un-staticized and
declared in `sys/include/isa/kbd.h`. The PS/2 and USB keyboards can coexist feeding
the same ring.

### Gotchas

- `hid_kbd_callback()` runs in interrupt context — no `kmalloc()`, no blocking calls.
- USB HID keycode 0 = no key. All six `keycode[]` slots may be 0; ignore them.
- Key-repeat is hardware-driven via `SET_IDLE` rate; no software repeat needed.

---

## Phase 7 — USB Mass Storage (Bulk-Only Transport)

**Goal:** BOT class driver exposing the USB flash drive as a block device mountable
by the existing VFS/FAT layer.

### New Files

- `sys/usb/ums_bot.c`
- `sys/include/usb/ums.h` — CBW/CSW structs, SCSI CDB definitions

### Key Data Structures

```c
/* Command Block Wrapper — 31 bytes, bulk OUT */
struct ums_cbw {
        uint32_t dCBWSignature;          /* 0x43425355 "USBC" */
        uint32_t dCBWTag;
        uint32_t dCBWDataTransferLength;
        uint8_t  bmCBWFlags;             /* bit 7: 1=IN, 0=OUT */
        uint8_t  bCBWLUN;
        uint8_t  bCBWCBLength;           /* 1–16 */
        uint8_t  CBWCB[16];              /* SCSI CDB */
} __attribute__((packed));

/* Command Status Wrapper — 13 bytes, bulk IN */
struct ums_csw {
        uint32_t dCSWSignature;          /* 0x53425355 "USBS" */
        uint32_t dCSWTag;
        uint32_t dCSWDataResidue;
        uint8_t  bCSWStatus;             /* 0=pass, 1=fail, 2=phase error */
} __attribute__((packed));

struct ums_softc {
        struct usb_device *um_dev;
        uint8_t            um_bulk_in_ep;
        uint8_t            um_bulk_out_ep;
        uint16_t           um_max_pkt_in;
        uint16_t           um_max_pkt_out;
        uint8_t            um_toggle_in;
        uint8_t            um_toggle_out;
        uint32_t           um_tag;
        uint32_t           um_blocks;
        uint32_t           um_blk_size;
};
```

### Minimum SCSI Commands

| Command | Opcode | Purpose |
|---|---|---|
| `INQUIRY` | 0x12 | Identify device type |
| `TEST UNIT READY` | 0x00 | Check media present |
| `READ CAPACITY (10)` | 0x25 | Get block count and size |
| `READ (10)` | 0x28 | Read sectors |
| `WRITE (10)` | 0x2A | Write sectors (optional for milestone 1) |

### VFS Integration

Register with `device_add()` using a new major number (e.g. 5). The `read()`/`write()`
device callbacks translate sector requests to `READ(10)` / `WRITE(10)` BOT transactions.
Mount via `vfs_mount(major=5, minor=0, ...)` — no VFS layer changes required.

### Gotchas

- CBW must be exactly 31 bytes sent as a single bulk OUT transaction.
- DATA0/DATA1 toggle (`um_toggle_in` / `um_toggle_out`) must be maintained across all
  bulk calls. Reset to DATA0 after a Bulk-Only Mass Storage Reset (class control request
  `bmRequestType=0x21, bRequest=0xFF`).
- `bCSWStatus=2` (phase error) requires `ums_reset()` before any retry.
- QEMU: use `-drive file=disk.img,format=raw,if=none,id=usbdisk` — the `if=none` prevents
  the disk from also appearing as an IDE device.

---

## QEMU Test Invocation

```sh
qemu-system-i386 \
  -m 256 \
  -machine pc \
  -device piix3-usb-uhci,id=uhci-bus \
  -device usb-kbd,bus=uhci-bus.0 \
  -drive file=usb.img,format=raw,if=none,id=usbdisk \
  -device usb-storage,bus=uhci-bus.0,drive=usbdisk \
  -drive file=ubixos.img,format=raw,index=0,media=disk \
  -net nic,model=e1000 -net user \
  -serial stdio
```

Notes:
- `-m 256` matches the kernel's `numPages` assumption; UHCI I/O BARs are in port space
  (not MMIO), so the MMIO threshold issue does not apply to UHCI itself.
- `-serial stdio` routes COM1 to the terminal for early USB debug output.
- Both the PS/2 keyboard and USB keyboard can be active simultaneously during development.

---

## Build System Integration

New directory `sys/usb/` needs a `Makefile`:

```makefile
include ../../Makefile.incl
include ../Makefile.incl

OBJS = uhci.o usb.o hid_kbd.o ums_bot.o

.include "${UBIX_MK}/ubix.kern.mk"
```

Add `sys/usb` to the subsystem list in `sys/Makefile` (after `pci`).

Add `bus.o dma_mem.o` to `sys/sys/Makefile`.

In `sys/include/ubixos/init.h`, add `usb_init` to `init_tasks[]` after `pci_init`.
`usb_init()` is a thin wrapper in `usb.c`; actual enumeration is driven by
`pci_init()` → UHCI attach → root port scan.
