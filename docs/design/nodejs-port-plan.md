# Porting Node.js to uBixOS — Scoping Plan

Status: **scoping only** (2026-07-05). No code. The goal is a clear-eyed
engineering plan: what Node actually requires, what uBixOS already provides, the
gating prerequisites, a milestone roadmap, and a go/no-go first step.

**Motivating goal:** run Node ≥18 in uBixOS, ultimately enough of it to host the
**Claude Code** CLI. (This challenges the aside in
[`vellum-editor-plan.md`](vellum-editor-plan.md) that the OS "has no path to
host Node/V8" — there *is* a path, it's just long.)

---

## 1. What "Node.js" actually is

Node is not one program; it's a stack:

| Layer | What it is | Port difficulty |
|-------|-----------|-----------------|
| **V8** | Google's JS engine (~a few M LOC C++), JIT + Ignition bytecode interpreter, generational GC, its own `v8::Platform` threading abstraction | **Hardest** |
| **libuv** | async I/O event loop + threadpool; per-OS backend (epoll/kqueue/…) | Hard (new backend) |
| **OpenSSL** | Node's `crypto`/`tls` — hard API dependency | Large but mechanical (C library) |
| c-ares, nghttp2, llhttp, ngtcp2, zlib, brotli, simdutf, ada | smaller bundled libs | Mostly mechanical |
| **Node core** | the C++/JS runtime tying it together; GN+ninja build | Hard (build system + built-ins) |

Two of these collide with uBixOS's current shape (cooperative, non-preemptive
kernel; no epoll/kqueue). The rest are "big but ordinary" C/C++ ports.

---

## 2. The two viability unlocks

Without these, the port is effectively blocked on multi-year kernel work. With
them, it becomes long but tractable.

### 2a. V8 `--jitless` (interpreter-only)

V8 supports a **jitless** mode: it runs the **Ignition bytecode interpreter**
with **no runtime code generation** — no JIT, no RWX/`PROT_EXEC` mappings, none
of the JIT-specific signal/GC machinery. Node accepts `--jitless`. This deletes
the single hardest kernel requirement (executable, writable-then-executable JIT
memory). Cost: **much slower** JS execution (interpreter only). Correctness
first; revisit partial JIT in N7 once preemption/exec-memory mature.

Related size levers: pointer compression, and disabling the snapshot/embedded
builtins where they complicate the cross build.

### 2b. Single-threaded `v8::Platform`

V8 abstracts threading behind `v8::Platform`. Provide an implementation that
runs posted tasks on the **main thread** (synchronous background compilation and
GC), so V8 doesn't depend on real preemptive multithreading. Cost: longer GC
pauses, no parallel compile. Works on a cooperative kernel. This is a supported
embedding pattern (`SingleThreadedDefaultPlatform`).

> Net effect: V8 becomes "a very large single-threaded C++ interpreter that
> needs `mmap`, a good libc, signals and a high-res clock" — all of which we
> have or can finish.

---

## 3. What uBixOS already provides (surveyed 2026-07-05)

Better than the "no path" assumption suggested:

- **Virtual memory:** `mmap2` (477) and **`mprotect` (74)** are VALID; demand
  paging exists ([[demand-paged-exec]]). `--jitless` means we do **not** need
  RWX for JIT.
- **Threads:** musl `pthread_create` → `__clone` with `CLONE_THREAD` → `RFORK`
  (aarch64 `syscall.c:678`, x86_64 `usermode.c:645`), backed by
  `ubthread_create`. **futex is implemented** (`sys/posix/gen_calls.c`:
  `FUTEX_WAIT/WAKE/REQUEUE/CMP_REQUEUE/WAIT_BITSET`). So thread *creation* and
  *synchronization primitives exist.
- **I/O readiness:** `select` (93) and `poll` (168) are VALID (implemented on a
  scheduler yield-loop in `sys/kern/descrip.c`).
- **Entropy/time:** `getrandom(2)` + `/dev/urandom` ([[kernel-csprng]]);
  wall-clock via `gettimeofday` (note the CLOCK_MONOTONIC gotcha,
  [[world-clock-gettime-gotcha]]).
- **Networking:** lwIP + DHCP + BSD sockets; TLS via BearSSL (but Node needs
  OpenSSL's *API* — BearSSL cannot substitute; see N3).

## 4. The gating gaps (prerequisites)

1. **Preemption / thread progress.** The kernel is **cooperative and
   non-preemptive** ([[aarch64-sched-jobcontrol]]): threads only advance at
   explicit yield points. A single-threaded V8 platform sidesteps V8's own
   threads, but **libuv's threadpool** (fs, `getaddrinfo`) and any Node worker
   still need threads that make progress without the holder yielding. Cross-ref
   [`in-kernel-preemption-plan.md`](in-kernel-preemption-plan.md) and
   [`threads-refactor.md`](threads-refactor.md). Interim: a **synchronous
   libuv threadpool** (run the "work" inline on the loop thread) avoids the
   dependency at a latency cost.
2. **Async-I/O backend.** No `epoll`/`kqueue` (kqueue = NOTIMP). libuv needs a
   **uBixOS backend built on `select`/`poll`**. libuv has no generic poll
   backend upstream, so this is new `src/unix/` code (model it on the
   historical `select`-based paths). This is the single biggest *bounded* piece
   of new code.
3. **Syscall-surface completion.** V8 + libuv + OpenSSL exercise a wide POSIX
   surface: `writev`/`readv`, `pread`/`pwrite`, `dup`/`dup2`, `pipe2`,
   `socketpair`, `sendmsg`/`recvmsg`, `fcntl` flags, `sigaction`/`sigaltstack`,
   TLS (thread-local storage), `sched_getaffinity`, `uname`, `sysconf`,
   `clock_gettime(CLOCK_MONOTONIC)`. Enumerate against a real build; expect a
   long tail of stubs → implementations.
4. **Huge-binary loading.** The `node` binary is ~50–90 MB (V8 + snapshot). Our
   loader had a ~4 MB `EXEC_MAX` cap ([[netsurf-port]] hit it at ~4 MB). Node
   requires (a) lifting that cap dramatically and (b) the **demand-paged
   execve** path actually exercised at scale ([[demand-paged-exec]] — currently
   implemented but path-unexercised). FAT/UbixFS must serve a huge executable.
5. **C++ runtime.** V8/Node are C++17. We have libc++/libc++abi in-tree; verify
   exceptions/RTTI posture matches what V8 needs (V8 builds `-fno-exceptions
   -fno-rtti`, which helps).

---

## 5. Dependency port order

Build bottom-up, each independently verifiable:

1. **zlib** — ✅ already in-tree (`lib/zlib`).
2. **OpenSSL** (or BoringSSL). Node hard-depends on the OpenSSL API. Big, but a
   normal C library cross-build. *Early milestones may use Node's
   `--without-ssl` to defer this, but Claude Code needs TLS, so OpenSSL is
   required for the real goal.* Decision point: OpenSSL vs BoringSSL (Node
   historically tracks OpenSSL; BoringSSL diverges).
3. **libuv** — with the new uBixOS `select`/`poll` backend + synchronous
   threadpool.
4. **c-ares, llhttp, nghttp2, brotli, simdutf, ada** — mechanical.
5. **V8** — `--jitless`, single-threaded platform, cross-compiled musl.
6. **Node core** — GN+ninja wiring the above; the last mile.

---

## 6. Milestone roadmap

- **N0 — Feasibility spike (go/no-go).** Cross-build **V8 standalone**
  (`d8` or a ~50-line embedder) `--jitless` + single-threaded platform against
  musl, and run `print(1+1)`. This de-risks the single biggest unknown: *does
  V8 build and run at all on our libc/kernel?* If N0 fails or balloons, stop and
  reconsider (QuickJS is the fallback for JS-the-language).
- **N1 — Kernel/libc prerequisites.** Close the §4 gaps surfaced by N0 (TLS,
  signals, clock, mprotect semantics, huge-binary loading). Lift `EXEC_MAX`,
  exercise demand paging on a big binary.
- **N2 — libuv uBixOS backend.** `select`/`poll` event loop + synchronous
  threadpool; pass libuv's own tests where feasible.
- **N3 — OpenSSL port.** `libcrypto`/`libssl` cross-built; a TLS smoke test.
- **N4 — Node builds & runs.** GN+ninja cross-build; `node -e "console.log(2+2)"`.
- **N5 — Built-ins bring-up.** `fs`, `net`, `tls`, `stream`, `child_process`,
  `os`, `process`, `crypto` — the modules Claude Code touches — iterated against
  the OS until a real npm package runs.
- **N6 — Claude Code.** Get `npm` working (or bundle the package), then work
  through Claude Code's own needs: it is **not pure JS** — it spawns
  subprocesses (e.g. ripgrep), uses a pty layer, and expects a POSIX-ish
  environment. Each is its own sub-task.
- **N7 — Make it usable.** Revisit real preemptive threads (smp-plan) and
  partial JIT once exec-memory + preemption mature, to move off
  interpreter-only speed.

---

## 7. Risks & honest estimate

- **V8 build/toolchain** is the top risk — GN+ninja + Chromium-style deps are
  hostile to cross-compilation; N0 exists to hit this first.
- **Cooperative scheduler vs the event loop:** even single-threaded jitless
  Node needs its loop + timers + I/O to interleave without starving the rest of
  the system; expect scheduler tuning (cf. [[desktop-idle]] blocking primitives,
  [[lwip-audit-plan]] tcpip-thread starvation).
- **OpenSSL** is large but low-novelty.
- **Claude Code's native/subprocess deps** mean "Node runs" ≠ "Claude Code
  runs"; N6 is its own project.
- **Effort:** realistically **months of focused work**, and several milestones
  (N1) are shared kernel investments that benefit the whole OS, not just Node.

**Recommended first action:** execute **N0** as a time-boxed spike. It's the
cheapest way to convert "months of unknowns" into a concrete go/no-go, and it
commits nothing else until we know V8 runs on uBixOS at all.
