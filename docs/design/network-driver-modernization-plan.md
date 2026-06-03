# UbixOS Network Stack & Driver Modernization Plan

## Goal

Bring the lwIP integration and the device drivers in line with how modern
kernels (FreeBSD, Linux, XNU) structure interrupt handling, blocking, timers,
and buffer management — replacing the historical **busy-wait everywhere** model
with reliable sleep/wakeup, deferred interrupt processing, and event-driven I/O.

The driving symptom was the GUI/mouse freezing during any network activity: the
whole stack blocked by spinning on `sched_yield()`, monopolizing the CPU and
starving the compositor. The first phases below have already fixed that; the
later phases remove the remaining hacks and align with current OS design.

This is a **staged** effort. Each phase is independently testable and the kernel
must stay bootable throughout; single-CPU correctness must never regress. The
work also dovetails with [smp-plan.md](smp-plan.md) (the sleep/wakeup core and
per-CPU idle thread are shared foundations).

---

## Status Matrix

Legend: ✅ done & verified · 🟡 partial · ⬜ not started

| Phase | Item | Status | Notes |
|-------|------|--------|-------|
| 0 | Real sleep/wakeup primitive (`sched_wait_event`/`sched_wakeup_chan`, `kTask_t.wait_chan`) | ✅ | ISR-safe wakeup; lost-wakeup-free via sched lock |
| 0 | Idle thread (`QOS_IDLE`, `sti;hlt`) so sleepers have somewhere to switch; CPU truly idles | ✅ | also a step toward per-CPU idle threads (smp-plan Phase 2.3) |
| 0 | Timed sleep (`kTask_t.wake_tick`, `sched_wait_event_timeout`, per-tick expiry scan) | ✅ | O(n) per-tick scan — replace with a callout wheel (Phase 3) |
| 0 | e1000 RX: IRQ-driven sleep, no poll loop | ✅ | ISR sets flag + `sched_wakeup_chan` |
| 0 | lwIP sem/cond/mutex (`ubthread`, `sys_arch` `cond_wait`) → sleep/wakeup | ✅ | PI-boost preserved |
| 0 | Bounded safety re-check on "infinite" waits (~50 ms) | ✅ | hedge; **removed in Phase 1** (commit d6e003588) |
| 0 | Fix latent `cpu_switch` self-switch (`prev==next`) | ✅ | sched skips the switch |
| 0 | Socket `close()` must tear down the lwIP netconn (`sys_close` fd_type==2) | ✅ | commit 1666f59cd — fixed netconn leak / "connect failed" |
| 1 | Infinite sem/cond waits block truly indefinitely (no safety re-check) | ✅ | commit d6e003588; a 5 s watchdog never fired under load, proving wakeups reliable |
| 1 | Proper sleepqueue / condvar API + unify `ubthread` locks (cosmetic; wait_chan already serves) | 🟡 | mechanism done & reliable; formal rename/API deferred (low value) |
| 1 | Unified lock set on the sleepqueue: mutex (PI), condvar, semaphore, rwlock | ⬜ | replaces ad-hoc `ubthread` + `sys_arch` primitives |
| 2 | NAPI-style RX: IRQ wakes thread → mask RX IRQ → poll-drain ring → re-arm | ⛔ | ATTEMPTED + REVERTED: poll-free NAPI regressed RX under QEMU's e1000 — an armed RX IRQ is dropped, RX stalls, connections never close → netconn leak → `socket: failed`. The 50 ms safety poll is load-bearing for QEMU's emulation. Revisit as **NAPI masking + keep the poll as a backstop**, not poll-free. |
| 2 | Top-half / bottom-half (ithread) for all non-trivial ISRs | 🟡 | e1000 already "ISR wakes thread"; kbd/mouse are minimal top-halves (ring enqueue) — no inline-heavy ISR left to convert |
| 3 | Callout / timer subsystem (expiry-sorted, O(1)/tick) | ✅ | commit 5ca1b070c — `sys/kern/callout.c`; timed sleep arms a callout, per-tick `wake_tick` taskList scan removed; lwIP timers ride it via tcpip_thread |
| 4 | Zero-copy pbuf path (drop the copy-into-kernel-buffer for `tcpip_thread`) | ⬜ | needs the stack to run with proper kernel mappings |
| 5 | newbus-style driver model: `probe`/`attach`/`detach` + resource manager | ✅ | `sys/sys/bus.c` (`ubx_device`/`ubx_driver`/`ubx_resource`); all live drivers (e1000, pci, IDE `hd`, lnc, ac97, mouse, atkbd, fdc, hid_kbd) attach through it; last old-model driver (`ne2k`, unbuilt) deleted along with `device.old.h` |

---

## Background: where we are today

- **Scheduler:** software context switch; `_current` is per-CPU (`%gs:8`). A
  wait-channel sleep/wakeup exists (`sched_wait_event` / `sched_wakeup_chan`)
  plus a timed variant. An idle thread halts the CPU when nothing is runnable.
- **e1000:** RX thread sleeps on `&e1000_irq_pending`; the ISR (`e1000_handle_irq`
  via `irq_dispatch`) sets the flag and wakes it. A ~50 ms bounded re-check
  guards against QEMU dropping the PIC IRQ.
- **lwIP glue:** `sys_arch.c` + `sys/kern/ubthread.c`. Semaphores/condvars/
  mutexes now sleep instead of spinning; `tcpip_thread` uses a timed mailbox
  fetch to drive lwIP's protocol timers.
- **Remaining hacks / smells:**
  - The ~50 ms safety re-check is a hedge, not a guarantee — modern designs make
    wakeups reliable so no periodic re-check is needed.
  - The timed-sleep expiry is an O(n) per-tick `taskList` scan.
  - `tcpip_thread` has no user mappings, so packets are **copied** into kernel
    buffers (see CLAUDE.md) — an address-space coupling, not zero-copy.
  - `ubthread` mutex PI-boost is bespoke; locks are ad-hoc per subsystem.
  - Most ISRs run their handler inline in hard-IRQ context.

---

## Phase 1 — Sleepqueue & unified synchronization

**Why:** today's wait-channel works but a missed wakeup can only be recovered by
the Phase-0 safety re-check. A real sleepqueue records the wakeup *against the
queue* and checks it atomically with the sleep, so wakeups cannot be lost — the
re-check (and its latency) goes away.

**What (FreeBSD `sleepqueue` / Linux `wait_queue` model):**
- A hashed set of wait queues keyed by channel address; `sleepq_add` /
  `sleepq_wait[_sig][_timeout]` / `sleepq_signal` / `sleepq_broadcast`.
- Build the lock set on top: `mutex` (with priority inheritance via a turnstile),
  `condvar`, `semaphore`, `rwlock`. Replace `ubthread_*` and the `sys_arch`
  `cond_wait` with these.
- Delete the Phase-0 bounded safety re-check once wakeups are guaranteed.

**Testable:** existing net workload (ping/DNS/httpget) with the safety re-check
removed; soak many connections; GUI stays responsive; no hangs.

## Phase 2 — Deferred interrupts (ithreads) + NAPI RX

**Why:** hard-IRQ context should do the minimum; heavy work belongs in a thread.
Under load, per-packet interrupts cause storms.

**What:**
- Generalize "ISR sets flag + wakes a thread" into an **interrupt thread**
  abstraction (`irq_register` gains an ithread option); convert kbd/mouse/etc.
- **NAPI RX** for e1000: on RX IRQ, mask RX interrupts, wake the RX thread; the
  thread poll-drains the descriptor ring, then re-enables RX interrupts when the
  ring is empty. Removes the RX safety poll and bounds interrupt rate.

**Testable:** sustained `httpget` of a large file; watch for no interrupt storm
(serial counters), RX keeps up, GUI responsive.

## Phase 3 — Callout / timer wheel

**Why:** replace the O(n) per-tick `wake_tick` scan and let drivers and lwIP
register efficient timed callbacks.

**What:** an O(1) timer wheel (FreeBSD `callout` / Linux `timer_list`):
`callout_reset(c, ticks, fn, arg)` / `callout_stop`. lwIP's `sys_check_timeouts`
becomes a periodic callout; `sched_wait_event_timeout` deadlines migrate onto it.

**Testable:** TCP retransmit / DHCP renew / DNS retry still fire on time; per-tick
scheduler cost drops (measure with many sleeping tasks).

## Phase 4 — Zero-copy buffers

**Why:** today packets are copied into kernel buffers because `tcpip_thread` has
no user mappings. Modern stacks pass buffers by reference (mbuf/sk_buff ↔ pbuf).

**What:** run the stack with proper kernel mappings (or a shared DMA buffer pool)
so the NIC's RX buffer flows up as a pbuf chain without a copy; TX likewise.

**Testable:** throughput improves; no per-packet `memcpy` in the RX/TX path.

## Phase 5 — Driver / bus model ✅

**Why:** formalize driver lifecycle and resource ownership.

**What (done):** a newbus-style model lives in `sys/sys/bus.c` /
`sys/include/sys/bus.h` — `ubx_device` / `ubx_driver` / `ubx_resource` with
`drv_probe` / `drv_attach` / `drv_detach`, resource allocators
(`ubx_alloc_memory` / `ubx_alloc_ioport` / `ubx_alloc_irq`), block/char device
registration, and `ubx_bus_probe_and_attach`. It was built out with the USB
device-model work. **All live drivers attach through it**: e1000, pci, IDE
(`hd`), lnc (PCNET), ac97, mouse, atkbd, fdc, hid_kbd.

The legacy `struct device` model (`sys/include/sys/device.old.h`) had a single
remaining consumer, the old NE2000 driver (`sys/isa/ne2k.c`), which was not even
compiled (`ne2k.o` was commented out of `sys/isa/Makefile`). It and
`device.old.h` have been **deleted**, so newbus is now the only device model in
the tree.

NE2000 support was then **rewritten from scratch** on the modern stack
(`sys/pci/ne2k.c` + `sys/net/netif/ne2knetif.c`) targeting the PCI RTL8029AS
(vendor 0x10EC / device 0x8029, QEMU `ne2k_pci`): newbus probe/attach, an
`irq_register` top-half that only flags + `sched_wakeup_chan`, and an RX thread
that sleeps on `sched_wait_event_timeout` and drains the DP8390 ring — the same
shape as the e1000. `net_init` prefers the e1000 (QEMU default) and falls back
to ne2k. Verified in QEMU (`bmake run-ne2k`): probe→attach→MAC read→TX→
IRQ-driven RX→DHCP bound (got 10.0.2.15, confirmed in the frame dump). The
DP8390 register core takes `(iobase, irq)`, so an ISA attach for real vintage
hardware is a small future addition; RX-ring overflow recovery (`ISR_OVW`) is
not yet implemented (the RX-thread safety timeout covers it for now).

**Future:** DMA-tag / busdma-style resource for zero-copy (ties into Phase 4);
hot-plug detach paths exercised end-to-end (USB already detaches on unplug).

---

## Sequencing rationale

1. **Phase 1 first** — it is small, high-leverage, builds directly on the
   wait-channel we already have, removes the safety-re-check hedge, and gives a
   single correct synchronization layer everything else uses.
2. **Phase 2** removes the RX safety poll and makes the driver model match modern
   top/bottom-half + NAPI.
3. **Phase 3** removes the last per-tick scan and gives real timers.
4. **Phases 4–5** are larger and depend on address-space and bus-model work; do
   them once the core (1–3) is solid.

Phases 1–3 are self-contained and individually shippable; none requires SMP, but
all of them make the SMP work in [smp-plan.md](smp-plan.md) cleaner (per-CPU
run queues, IPIs, and per-CPU idle threads all assume a real sleep/wakeup core).
