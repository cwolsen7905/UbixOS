# PCI / e1000 / IDE Audit Findings

## Summary

18 findings: 2 critical, 6 high, 6 medium, 4 low

---

## Findings

### PCI-001 — e1000 TX/RX ring has no locking between ISR and transmit path
**Severity:** 🔴 Critical
**File:** `sys/pci/e1000.c:218` (`e1000_send_packet`) and `sys/pci/e1000.c:290` (`e1000_handle_irq`)
**Description:** `tx_tail` is a plain `uint32_t` modified non-atomically in `e1000_send_packet`. `e1000_rx_process` can be entered both from the IRQ path (via `e1000_irq_pending` flag) and from the fallback polling check at line 315 in `e1000_thread` — in the same thread body, back-to-back, with no serialisation. Any path that calls `e1000_send_packet` from an lwIP callback while the scheduler also runs the e1000 thread is a concurrent-access race.
**Impact:** Duplicate packet delivery or descriptor corruption under load.
**Suggested fix:** Protect `tx_tail` with a spinlock or disable-IRQ section. Remove the redundant in-thread `rx_descs[rx_tail].status` poll at line 315 (the `e1000_irq_pending` path already handles it) so `e1000_rx_process` is never called twice concurrently.

---

### PCI-002 — `lnc_INT` reads uninitialized `csr0` (always zero), masking all PCnet interrupts
**Severity:** 🔴 Critical
**File:** `sys/pci/lnc.c:292`
**Description:** `lnc_INT()` declares `uint16_t csr0 = 0x0;` then immediately tests bits on it without reading the hardware register. Every conditional (`ERR`, `RINT`, `TINT`) always evaluates false. The function then writes `0x7940` to CSR0 unconditionally, clearing all pending interrupt flags on the PCnet.
**Impact:** All PCnet RX and TX events are silently dropped. The NIC is permanently dead after the first IRQ.
**Suggested fix:** Add `csr0 = (uint16_t)lnc_readCSR32(lnc, CSR0);` as the first statement in `lnc_INT`. Re-enable the `lnc_rxINT()` and `lnc_txINT()` calls for the corresponding flag bits.

---

### PCI-003 — RX descriptor ring: tail index semantics ⚠️ RETRACTED
**Severity:** ~~🟠 High~~ (retracted — original behavior correct for this driver)
**File:** `sys/pci/e1000.c:278–280`
**Note:** The "write RDT after advancing rx_tail" change was applied but broke DHCP reception in QEMU. The original write-back-then-advance convention (`RDT = rx_tail` before `rx_tail++`) is what QEMU's e1000 model expects — it returns the just-consumed slot index as the new hardware tail boundary. The SDM description of "RDT = last returned descriptor" maps to the *consumed* index, not the *next* index. Reverted. Do not re-apply.

---

### PCI-004 — BAR0 size not validated before mapping 32 pages (128 KB)
**Severity:** 🟠 High
**File:** `sys/pci/e1000.c:108`
**Description:** `e1000_map_mmio` hardcodes 32 pages regardless of the BAR size stored in `dev->dev_res[i].r_size`. A device with a smaller BAR would have pages beyond the BAR mapped to whatever physical address follows it.
**Impact:** Kernel reads/writes beyond the real BAR boundary corrupt adjacent MMIO space or RAM.
**Suggested fix:** Pass `barSize` into `initE1000`/`e1000_map_mmio` and assert `barSize >= 0x20000`. Use the actual size for the mapping loop.

---

### PCI-005 — `e1000_mmio` set to raw physical address with no NULL-check on `vmm_remapIOPage`
**Severity:** 🟠 High
**File:** `sys/pci/e1000.c:116` and `e1000_init_rx`/`e1000_init_tx`
**Description:** After calling `vmm_remapIOPage` for each page, `e1000_mmio` is set to the physical address directly with no check that any mapping succeeded. Same pattern in `e1000_init_rx` and `e1000_init_tx` for descriptor ring and buffer pages. A silent mapping failure leads directly to a triple-fault on the first MMIO access.
**Impact:** Kernel triple-fault on first MMIO register access if any page mapping fails.
**Suggested fix:** Check return values from `vmm_remapIOPage`. At minimum, verify the first MMIO DWORD reads back a non-0xFFFFFFFF value after mapping.

---

### PCI-006 — `hdRead` double-adds `parOffset + lba_start`; `hdWrite` adds only one
**Severity:** 🟠 High
**File:** `sys/pci/hd.c:513–514` vs `sys/pci/hd.c:435–437`
**Description:** `hdRead` unconditionally adds *both* `hdd->parOffset` and `hdd->lba_start` to `startSector`. `hdWrite` conditionally adds only one (if `lba_start == 0` use `parOffset`, else use `lba_start`). For BSD disklabel sub-partitions both fields are non-zero, so `hdRead` over-offsets every sector read.
**Impact:** Silent data corruption / read errors on BSD disklabel sub-partitions.
**Suggested fix:** Apply the same conditional in `hdRead`:
```c
if (hdd->lba_start == 0)
    startSector += hdd->parOffset;
else
    startSector += hdd->lba_start;
```

---

### PCI-007 — pciProbe writes AMD command register before validating device exists
**Severity:** 🟠 High
**File:** `sys/pci/pci.c:272–279`
**Description:** The AMD-specific command-register fixup fires at `i == 1` during the initial DWORD loop — before the `vendorID == 0xffff` absent-device check, before header-type validation, and before any other device identification. A phantom device accidentally returning `0x1022` in bits 15:0 of DWORD 1 would have its command register modified.
**Impact:** Possible accidental disabling of I/O and memory decode on a real device that happens to have `0x1022` in that field position.
**Suggested fix:** Remove the temporary fixup (it is tagged `/* This is TEMPORARY */` with no date) or move it to the lnc attach function where the device is positively identified by vendor *and* device ID.

---

### PCI-008 — TX busy-wait spins 100,000 iterations with no delay or yield
**Severity:** 🟠 High
**File:** `sys/pci/e1000.c:229`
**Description:** `e1000_send_packet` busy-waits up to 100,000 iterations reading `tx_descs[tail].status` with no `sched_yield`, `pause` instruction, or I/O delay. On a loaded system this holds the CPU for milliseconds, starving the RX thread and scheduler.
**Impact:** CPU starvation under network load; potential missed RX deadlines.
**Suggested fix:** Add `__asm__ volatile("pause")` in the inner loop and call `sched_yield()` every ~1000 retries.

---

### PCI-009 — Oversized RX frames dropped silently with no statistics
**Severity:** 🟡 Medium
**File:** `sys/pci/e1000.c:268`
**Description:** If `rx_descs[rx_tail].length > E1000_BUF_SIZE`, the frame is consumed and discarded with no log message and no `LINK_STATS_INC(link.drop)`.
**Impact:** Silent packet loss; difficult to diagnose.
**Suggested fix:** Add `LINK_STATS_INC(link.drop)` and a `klog(KLOG_WARNING, ...)` on oversized frames. Add a compile-time assertion `_Static_assert(E1000_BUF_SIZE >= 1536, ...)`.

---

### PCI-010 — `hdInit` issues `SET MULTIPLE` without checking the result
**Severity:** 🟡 Medium
**File:** `sys/pci/hd.c:413–415`
**Description:** After ATA IDENTIFY, `SET MULTIPLE` (0xC6) is issued unconditionally using the maximum supported count from word 47. The status register is never polled after the command. If the drive sets ERR, `hdShift`/`hdMulti` remain set to a value the drive rejected.
**Impact:** Silent misconfiguration of multi-sector reads; potential read corruption on drives that reject the count.
**Suggested fix:** Poll status after `SET MULTIPLE`; on ERR, fall back to `hdShift=0`, `hdMulti=1`.

---

### PCI-011 — `lncInt` ISR contains infinite loop before EOI — system would hang if called
**Severity:** 🟡 Medium
**File:** `sys/pci/lnc.c:421`
**Description:** `lncInt()` begins with `while (1) { kprintf("Finished!!!\n"); }`. The `outportByte(0x20, 0x20)` EOI is unreachable. If this function were ever reached via a stray function pointer, the system would hang with serial flooded.
**Impact:** System hang if called; misleading dead code below the infinite loop.
**Suggested fix:** Delete `lncInt` entirely.

---

### PCI-012 — PCI enumeration scans only buses 0–1
**Severity:** 🟡 Medium
**File:** `sys/pci/pci.c:465`
**Description:** `bus < 0x2` limits scanning to buses 0 and 1. QEMU's Q35 machine places USB controllers and other devices on secondary buses (≥ 2) behind PCI-to-PCI bridges.
**Impact:** Devices on bus ≥ 2 are never discovered.
**Suggested fix:** Implement recursive bus scanning: when a PCI-to-PCI bridge (header type 0x01) is found, read its secondary bus number (config offset 0x19) and scan it.

---

### PCI-013 — `low_level_output` uses a `static` scratch buffer — not re-entrant
**Severity:** 🟡 Medium
**File:** `sys/net/netif/e1000netif.c:78`
**Description:** `static uint8_t tx_scratch[1518]` is shared across all calls. Concurrent or reentrant calls overwrite each other's in-flight frame data.
**Impact:** Latent data corruption if a second output context is introduced.
**Suggested fix:** Change `tx_scratch` to a local (stack) variable.

---

### PCI-014 — `e1000_rx_len` stale between packets; no consume-once semantics
**Severity:** 🟡 Medium
**File:** `sys/pci/e1000.c:270` and `sys/pci/e1000.c:395`
**Description:** `e1000_rx_len` is a global set once per RX event and never cleared. A second call to `e1000_get_rx_packet` before the next packet arrives returns the length of the previous frame.
**Impact:** Stale length and buffer delivered on spurious second reads.
**Suggested fix:** Set `e1000_rx_len = 0` inside `e1000_get_rx_packet` after copying out the length (consume-once semantics), or document the single-use contract clearly.

---

### PCI-015 — lnc driver always linked even when no PCnet NIC present
**Severity:** 🔵 Low
**File:** `sys/pci/pci.c:388`, `sys/pci/lnc.c`
**Description:** `lnc_ubx_driver` is always in `pci_drv_table[]`. The dead code in `lncInt`, `lnc_rxINT`, and `lnc_txINT` increases kernel image size and is a latent hazard (see PCI-011).
**Suggested fix:** Guard lnc behind `#ifdef UBIXOS_LNC` or remove the dead code paths.

---

### PCI-016 — `_initHardDisk` leaks `data2` on all exit paths
**Severity:** 🔵 Low
**File:** `sys/pci/hd.c:119, 250`
**Description:** `data2` (512 bytes, BSD disklabel buffer) is allocated unconditionally but never freed. `kfree(data)` at line 250 frees only the MBR buffer.
**Suggested fix:** Add `kfree(data2)` before `hdC++; return (0x0)`.

---

### PCI-017 — `pciProbe` forwards bridge/unknown-type devices with zeroed BAR records
**Severity:** 🔵 Low
**File:** `sys/pci/pci.c:329–337`
**Description:** For header types 0x1 and 0x2, `pciProbe` skips the BAR-read and BAR-size-discovery loops but still returns the `cfg` pointer. The caller creates a `ubx_device` with all BARs at zero and forwards it to driver probe functions. A driver matching only by vendor/device ID could attach to a bridge with bogus resources.
**Suggested fix:** Free `cfg` and return `NULL` for non-type-0 headers. Handle bridges in a separate recursive-scan path.

---

### PCI-018 — `lnc_sendPacket` calls `kpanic` on TX ring full instead of dropping the packet
**Severity:** 🔵 Low
**File:** `sys/pci/lnc.c:622`
**Description:** `kpanic("NO TX BUFFERS")` is called when all 8 TX descriptors are in use. Any burst of 8+ packets kills the kernel.
**Suggested fix:** Replace with `LINK_STATS_INC(link.drop); return 0;` and a rate-limited `klog(KLOG_WARNING, ...)`.
