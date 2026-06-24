# lwIP Audit, Hardening, Stability & Performance Plan

**Status:** DESIGN ONLY — drafted 2026-06-24. No code yet.
**Sibling doc:** [network-driver-modernization-plan.md](network-driver-modernization-plan.md)
covers the *driver + scheduler* side (sleep/wakeup, ithreads, NAPI). This plan
covers the **lwIP core integration** itself — its configuration, its `sys_arch`
port layer, the socket/netconn ABI seam, and correctness/robustness under our
cooperative-on-aarch64 / preemptible-on-i386 scheduler.

---

## Motivation (observed symptoms)

Field reports during the Dropbear/SSH bring-up (2026-06-24):

- The box **intermittently stops replying to ICMP echo** (pings) for a while,
  then resumes on its own.
- **New TCP connections sometimes hang** mid-handshake; `Ctrl-C` + retry
  connects immediately.
- Throughput feels low / bursty under an interactive SSH session.

These are classic signatures of one or more of:

1. **`tcpip_thread` starvation.** aarch64 is *non-preemptible in-kernel*
   (timer only reschedules from EL0 — see
   [project_aarch64_sched_jobcontrol]). Any kernel thread that busy-polls
   (`poll → sched_yield → repeat`) instead of blocking can starve
   `tcpip_thread`, so timers (TCP retransmit, ARP, DHCP renew) fire late and
   RX is serviced late. A late/lost retransmit looks exactly like "hangs, works
   on retry."
2. **Timer/timeout drift.** lwIP's `sys_check_timeouts()` cadence is only as
   accurate as how often `tcpip_thread` runs. Under starvation, `TCP_TMR`
   (250 ms) slips, RTO estimation degrades, and `ARP`/`ND` entries expire →
   transient "no reply to ping."
3. **pbuf / memp pool exhaustion with no backpressure.** If a pool runs dry
   (PBUF_POOL, TCP_SEG, NETCONN), lwIP silently drops; the peer retransmits and
   it "recovers" — bursty and lossy, never a hard failure. We have no
   visibility into pool high-water marks today.
4. **`sys_arch` port races under SMP.** `-smp 2` is the default now. The
   mailbox/sem/mutex shims in `sys/net/net/sys_arch.c` must be audited for
   lost-wakeup / ABA under genuine concurrency, and for the
   `LWIP_TCPIP_CORE_LOCKING` discipline (who may touch the core, and when).

The through-line: **most of these are the same cooperative-scheduling fragility**
that the modernization plan started fixing for drivers, now seen from the lwIP
core's vantage. So Phase 0 here is *measurement* — prove which it is before
changing knobs.

---

## Principles

- **Measure first.** No tuning a single `MEM_SIZE`/pool number until we have
  instrumentation showing what actually runs dry / slips. Guessing at lwIP
  sizing to "paper over a kernel-size issue" is explicitly called out as wrong
  in CLAUDE.md; honor that here.
- **Keep both arches green.** aarch64 (primary) and x86_64 (anchor) must stay
  bootable + DHCP-up after every phase. i386 is frozen (releng/2) — not a gate.
- **FreeBSD-faithful seam.** The socket ABI stays FreeBSD-shaped
  (sockaddr with `sin_len`, errno values) per [feedback_freebsd_abi]. We patch
  musl to match FreeBSD, never the reverse.
- **Don't fork lwIP.** lwIP 2.0.3 lives in `contrib/` and is treated as
  vendored-upstream. Robustness work goes in *our* port layer
  (`sys/net/net/sys_arch.c`, `lwipopts.h`, the netif glue) and the syscall seam,
  not in lwIP `src/`. A version bump (2.0.3 → latest 2.2.x) is a Phase of its
  own with its own risk budget.

---

## Phases

Legend: ✅ done · 🟡 partial · ⬜ not started

### Phase 0 — Instrumentation & Reproduction (do first)
- ⬜ **lwIP stats on.** Enable `LWIP_STATS` + `MEMP_STATS` + `MEM_STATS` +
  `LINK_STATS` + `ETHARP_STATS` and expose them read-only via
  `/proc/net/lwip` (or a `sysctl`). Watch `*.max` / `*.err` / `*.drop`.
- ⬜ **`tcpip_thread` liveness probe.** A counter bumped each
  `tcpip_thread` loop + each `sys_check_timeouts`, surfaced in procfs, so we can
  see if it stalls during a "ping drop."
- ⬜ **Repro harness.** Scripted `ping -f` + parallel `ssh` connect/disconnect
  loop against the QEMU guest; record when echo stops and correlate with the
  liveness counter and pool `.err` deltas. This converts "feels unstable" into a
  signal we can bisect.
- ⬜ **Decide the root cause** from the data: starvation vs. pool vs. timer vs.
  sys_arch race. The later phases are conditional on this.

### Phase 1 — Stability / correctness (the actual bug-fixes)
- ⬜ **Blocking, not polling, in every lwIP-adjacent kernel thread.** Audit
  `tcpip_thread`, the RX paths, and the netif input for any
  `poll→yield` and convert to `sched_wait_event[_timeout]` /
  `sched_wakeup_chan` (the primitive from the modernization plan / the
  [project_desktop_idle] idle work). Highest-priority if Phase 0 shows
  starvation.
- ⬜ **Timer accuracy.** Ensure `sys_check_timeouts()` runs on a real timed
  sleep (callout), not "whenever the loop happens to spin." Tie to the callout
  wheel the modernization plan defers.
- ⬜ **Pool backpressure + sizing from data.** Right-size PBUF_POOL / TCP_SEG /
  NETCONN / TCP_PCB from Phase-0 high-water marks; add a low-pool log so future
  exhaustion is visible, not silent.
- ⬜ **`sys_arch` SMP audit.** Walk every mbox/sem/mutex shim for lost-wakeup
  and core-locking discipline under `-smp 2`. This is the
  [project_smp_initorder_ubistry]-class bug hiding in the network port.
- ⬜ **Connection-teardown completeness.** Re-verify the netconn-leak class
  (modernization Phase 0 fixed `close()`; re-audit accept/abort/RST and the
  Dropbear fork+dup socket-refcount path under churn).

### Phase 2 — Hardening (robustness against hostile / malformed input)
- ⬜ Bounds/fuzz the input path: malformed Ethernet/ARP/IP/TCP/UDP headers,
  truncated options, overlapping TCP segments, fragmentation edge cases.
- ⬜ Resource-exhaustion resistance: SYN flood / half-open caps, per-connection
  memory limits, ARP-table flooding, ICMP rate behavior.
- ⬜ Turn on lwIP's defensive options (`LWIP_CHECKSUM_ON_COPY`,
  random ISN, `LWIP_TCP_RTO`/sane RTO bounds, `SO_REUSE` policy review).
- ⬜ Reproducible test corpus committed under `tests/net/`.

### Phase 3 — Performance
- ⬜ Window / buffer tuning from measured BDP (TCP_WND, SND_BUF, MSS) once
  Phase 1 makes the scheduler honest.
- ⬜ Reduce pbuf copies on the syscall seam (today user data is copied to kernel
  buffers because `tcpip_thread` has no user mappings — see CLAUDE.md socket
  note). Evaluate a per-call bounce-pool vs. mapping the caller transiently.
- ⬜ Zero-copy RX from the virtio-net ring into pbufs where the driver allows.
- ⬜ Checksum offload where the emulated/real NIC supports it.

### Phase 4 — Expansion (new capability, lowest priority)
- ⬜ IPv6 end-to-end (lwIP supports it; our ABI conversions + DHCPv6/SLAAC need
  wiring — `lwip_to_posix_addr` already has an AF_INET6 branch).
- ⬜ Loopback `lo0` as a first-class netif.
- ⬜ Multiple-NIC / routing-table review (relevant to UbixFS multi-device and
  future multi-homed boxes).
- ⬜ Optional lwIP 2.0.3 → 2.2.x version bump (isolated, well-tested phase).

---

## Out of scope / explicitly deferred
- Rewriting drivers — that's [network-driver-modernization-plan.md].
- TLS — lives in [project_bearssl_port] / [project_tls_https], above lwIP.
- A from-scratch TCP/IP stack. lwIP stays; we harden the *integration*.

## Done-when
Phase 0–1 complete = no spurious ping loss or connect-hang under the Phase-0
repro harness for a sustained run on both arches, with `/proc/net/lwip` showing
zero pool `.err` and a `tcpip_thread` liveness counter that never stalls.
