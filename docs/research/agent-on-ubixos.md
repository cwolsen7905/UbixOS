# Running an OpenCode-like coding agent on uBixOS

Status: **research only** (2026-10-13). No code changes. This sizes the routes
to a terminal coding agent (chat with an LLM, run read/edit/shell tools, loop)
running *on* uBixOS, on both aarch64 and x86_64.

Everything under "What the tree has today" was checked against the source on
branch `docs/agent-on-ubixos-research` (cut from master). Claims about
third-party projects (OpenCode, Bun, Go, llama.cpp) are from general knowledge
and are marked **[unverified]**; check them before committing to a route.

## 1. What an agent actually needs

An OpenCode-class agent is a small loop:

1. A **TLS-capable HTTP client** with streaming (SSE) responses and POST.
2. **JSON** encode/decode (request bodies, tool-call arguments).
3. **Tool execution**: read file, write/edit file, list/grep, run a shell
   command and capture output, with timeouts and a kill switch (Ctrl-C).
4. A **terminal front end**: line input, scrollback, raw-mode keys, SIGWINCH.
5. Config and secrets (API key, model, endpoint) and a permission prompt.

Notably it needs **no** GUI, no JIT, no threads (a single-threaded poll loop is
enough) and very little CPU: the model runs elsewhere unless route 3b is chosen.
That shapes the whole comparison: the cost is dominated by *how much runtime
we must port to host the loop*, not by the loop itself.

## 2. What the tree has today (verified)

| Area | State | Evidence |
|------|-------|----------|
| Sockets | TCP/UDP BSD sockets over lwIP: `socket` `connect` `bind` `listen` `accept4` `sendto` `recvfrom` `sendmsg` `recvmsg` `setsockopt` `getsockopt` `getsockname` `getpeername` VALID. `socketpair`, `kqueue`/`kevent`, `ppoll`, `sendfile`, `flock`, `mkfifo` are NOTIMP | `sys/posix/syscalls_posix.c` |
| DNS | musl resolver + `/etc/resolv.conf` (8.8.8.8, 8.8.4.4); `libhttp` uses `gethostbyname` (IPv4 only) | `etc/resolv.conf`, `lib/libhttp/http.c:167` |
| TLS | BearSSL (`lib/libbearssl`), **TLS 1.2 only**, with a bundled root-CA table (`lib/libhttp/trust_anchors.c`); `httpsget` is QEMU-verified with chain, hostname and expiry validation | `lib/libhttp`, `usr.bin/httpsget` |
| HTTP client | `http_get()` only: GET, follows redirects, **buffers the whole body**. No POST, no custom headers, no chunked/SSE streaming, no timeouts, no proxy | `include/http/http.h`, `lib/libhttp/http.c` |
| Entropy, time | `getrandom` 563 + `/dev/urandom`; `clock_gettime` VALID (CNTVCT on aarch64) | `syscalls_posix.c`, `sys/kern/random.c` |
| Process model | `fork` (COW), `execve`, `wait4`, `pipe`/`pipe2`, `dup`/`dup2`, `fcntl`, `setsid`/`setpgid`, `poll`/`select`. W-037 made poll/select see pipes and signals; W-047 fixed the SMP reap race (3000-run sh loop passes). Enough for a `bash` tool via fork+exec+pipe | `docs/WORK.md` W-037, W-047 |
| mmap | anonymous + file-backed `mmap2` 477, `munmap`, `mprotect`, `madvise`, `msync`; demand-paged exec; `EXEC_MAX` is 256 MB | `sys/arch/*/kern/execfile.c:35` |
| Threads | `rfork` + futex (in `sys/posix/gen_calls.c`), `set_tid_address`, group kill on exit (W-041), shared mmap cursor (W-040). musl `pthread_create` works | `docs/WORK.md` W-040, W-041 |
| Signals / tty / pty | POSIX signals, `sigaltstack`, `sigsuspend`, `setitimer` (W-038); termios, job control; `posix_openpt` 504 VALID; the GUI terminal runs tcsh | `sys/posix/signal.c`, `tty.c` |
| Dynamic linking | musl `ld-musl-${ARCH}.so.1`; `PT_INTERP` `/lib` | CLAUDE.md |
| C++ | libc++/libc++abi in tree, built **without exceptions/RTTI** (LLVM and V8 are built `-fno-exceptions -fno-rtti` for that reason) | `tools/ports/llvm/README.md` |
| Self-hosting | Clang+lld port; `/usr/src/bin` and the kernel build in-OS on both arches (W-007, W-035, W-036); in-OS clang is ~100 MB and loads; bmake, oksh, git (M0, local only), dropbear ports exist; `tcc` staged as `/usr/bin/tcc`. Open: W-043 (musl bmake build), W-045 (crt files in image) | `docs/WORK.md`, `tools/ports/*` |
| JS today | **Duktape runs in-OS** inside NetSurf (JS works, 2026-06-03); not packaged as a standalone interpreter | `contrib/netsurf/content/handlers/javascript/duktape` |
| Gaps that matter | `kqueue` (NOTIMP), `socketpair`, `ppoll`, `sendfile`, `clock_nanosleep`, `getpriority`, `preadv/pwritev`; `sigsuspend`/`poll`/`tkill`/`statx` served at Linux slots until W-029 | `syscalls_posix.c`, W-029 |
| Prior art | `docs/design/nodejs-port-plan.md` (2026-07-05, scoping only) estimated "months"; it predates W-037..W-047 and the in-OS clang, so its "cooperative, no threads progress" gating items are partly stale | that file |

Two further facts to carry through:

- **No preemption in the kernel** (ring-3/EL0 preempted by the tick only), but
  userland threads and SMP do progress. A single-threaded event loop is safe
  today; do not design anything that needs a kernel thread to be preempted
  mid-syscall.
- **Network stack is lwIP**, not a full BSD stack: expect slow bulk throughput
  and edge-case bugs on long-lived TLS streams (see `lwip-audit-plan.md`). An
  agent holds an SSE stream open for tens of seconds, which is a new load
  pattern; `httpsget` fetches finish quickly.

## 3. Route 1: port Bun or Node so OpenCode itself runs

**What OpenCode needs [unverified]:** OpenCode is TypeScript that runs on Bun
and leans on Bun-specific APIs (`Bun.spawn`, `Bun.file`, `Bun.serve`, `bun:sqlite`,
the `$` shell) plus a native TUI layer. Node would therefore need a
compatibility shim or a fork of OpenCode, on top of porting Node. Bun is Zig +
JavaScriptCore, built with a Zig toolchain that has no uBixOS target and whose
runtime assumes Linux/macOS/Windows services (epoll/kqueue/io_uring, `/proc`,
mimalloc). A Bun port is a new-OS port of Zig's std targets *and* Bun's event
loop: out of scope for a hobby OS. Treat **Node (or a JSC/QuickJS-based
runtime) + a compat shim** as the only realistic variant.

### Missing today

- **A JS engine port.** V8 `--jitless` + single-threaded platform (the plan's
  N0 spike) is the only variant that avoids RWX memory. JavaScriptCore's
  JSCOnly port with LLInt-only is a possible alternative **[unverified]**.
- **libuv backend.** No kqueue/epoll; libuv needs a new `poll`-based backend
  (new code, no upstream precedent) plus a synchronous threadpool, or real
  threads (they exist now, so the threadpool can be real, but `getaddrinfo`
  and fs ops get threaded; this is the exact surface W-040/W-041 fixed).
- **OpenSSL.** Node hard-depends on the OpenSSL API; BearSSL cannot stand in.
  Large but mechanical C port.
- **Syscall tail.** `socketpair` (child_process stdio), `sendmsg` fd passing,
  `ppoll`, `clock_nanosleep`, `sched_getaffinity`, `/proc/self/*` readers,
  `uname`/`sysctl` shape, `statx`/`fstatat` coverage.
- **Build.** GN+ninja Chromium-style cross-build against musl-freestanding.
  The plan flags this as the top risk.
- **Binary size/RAM.** 50-90 MB executable is under `EXEC_MAX`, but
  demand-paged exec at that scale is unexercised; V8 isolates want 100s of MB.
- **Opaque startup cost.** A jitless interpreter booting Node's built-in JS
  and then OpenCode's TS bundle will cost seconds and tens of MB per start on
  aarch64 HVF, much worse on x86_64 under TCG on Apple Silicon.

### Size (MR-sized slices; each is ~1 MR unless noted)

1. N0 spike: V8 or JSC standalone, `print(1+1)` (1-2 MRs, mostly build work).
2. Close the syscall tail found by N0 (3-5 MRs).
3. libuv uBixOS backend + tests (2-3 MRs).
4. OpenSSL port (2 MRs).
5. c-ares, llhttp, nghttp2, brotli, simdutf, ada ports (3-4 MRs).
6. Node core GN cross-build (2-4 MRs, high variance).
7. Built-ins bring-up: `fs`, `net`, `tls`, `child_process`, `os` (4-6 MRs).
8. OpenCode compat shim or fork, run it (3-6 MRs, unknown without reading
   OpenCode's source against the platform).

Order of magnitude: **~25-35 MRs, many of them large; months of focused
work**, with a go/no-go after slice 1.

### Risks

- V8 build/toolchain (top risk); GN assumptions about host OS.
- Upstream drift: OpenCode changes weekly; a fork or shim rots.
- Performance: jitless V8 plus lwIP plus TCG is a poor experience on
  x86_64; acceptable on aarch64 HVF only.
- "Node runs" does not mean "OpenCode runs" (Bun API usage, pty/native deps).
- Violates the project's grain only mildly (it is a port, in `tools/ports/`),
  but it drags in a Linux-shaped runtime (epoll idioms, `/proc`, Linux errno
  assumptions). Every such gap must be answered inside the FreeBSD-like POSIX
  layer, not by cloning Linux (A-1..A-12).

### What self-hosting gives you

Clang+lld in-OS means the Node/V8 build itself stays cross-built (it is
hours-long and GN-based; do not try in-OS), so self-hosting buys little for
the port itself. It does help the *agent's tools*: the agent could `cc`, `bmake`
and `git` on-device. That benefit is shared by every route.

## 4. Route 2: native C agent (`uagent`)

A small C program (target: ~3-5k lines) in `usr.bin/agent`, linked against
`libhttp` (extended) and BearSSL. Single process, single thread, `poll()` loop.

### Design sketch

- **Providers:** one OpenAI-compatible chat-completions adapter (covers
  llama.cpp `llama-server` on another host, OpenRouter, vLLM, Ollama's
  OpenAI endpoint), then an Anthropic Messages adapter for tool-use and
  streaming. Endpoint, model and key come from env/`/etc/agent.conf`/ubistry.
  llama-server over plain `http://` on a LAN needs no TLS at all, which makes
  it the best first target.
- **Transport:** new `http_request()` in `libhttp`: method, headers, body,
  chunked decode, incremental read callback for SSE, timeouts, optional
  `Connection: keep-alive`. Keep `http_get()` as a wrapper.
- **JSON:** vendored small parser (cJSON or jsmn, both MIT) under `contrib/`
  or a ~600-line in-tree one; streaming-delta assembly for tool-call args.
- **Tools:** `read_file`, `write_file`, `edit_file` (exact-substring replace,
  fail on ambiguity), `list_dir`, `grep` (shell out to `/usr/bin/grep`),
  `bash` (fork+execve `/bin/sh -c`, pipe stdout+stderr, `setpgid`, kill the
  group on timeout/Ctrl-C), all through plain POSIX calls the tree already
  serves.
- **UI:** line editing via `libedit` (already in `lib/`), streamed token
  output, `y/n` permission prompt for write/bash; SIGWINCH optional.
- **Native-ABI angle:** keep the core (agent loop, tool dispatch, provider
  adapters) free of POSIX assumptions behind a thin `platform.h` so a later
  native uBix front end (views window, MPI) can reuse it. The POSIX layer is
  used for the tools because they *are* the POSIX world.

### Missing today

Almost nothing in the kernel. What is needed is userland:

- `libhttp` POST/headers/SSE/timeouts (above).
- IPv6 / `getaddrinfo` instead of `gethostbyname` (optional; most API hosts
  have IPv4).
- BearSSL is TLS 1.2 only. Today api.anthropic.com and openrouter.ai accept
  TLS 1.2 **[unverified, check before relying on it]**; if either moves to
  1.3-only the route needs a BearSSL replacement (or a TLS-terminating proxy on
  the LAN, which llama-server users already tolerate).
- Root CA table refresh and pinning policy (`trust_anchors.c` is a snapshot).
- Wall-clock correctness for cert expiry (needs RTC/NTP; `httpsget` already
  needed a fix here per memory).
- A long-lived lwIP stream soak (SSE for minutes, 100s of KB).

### Size

1. `libhttp`: `http_request()` with method/headers/body/chunked + tests via
   `httpsget` (1 MR).
2. SSE streaming + timeouts + keep-alive (1 MR).
3. JSON parser + chat-completions request/response + streamed tool-call
   assembly; plain-HTTP llama-server end-to-end with a canned prompt (1 MR).
4. Tool runtime: read/write/edit/list/grep (1 MR); `bash` with process-group
   timeout/kill (1 MR).
5. Agent loop, UI, permission prompts, `/etc/agent.conf` (1 MR).
6. HTTPS to OpenRouter (CA/time/TLS1.2 verification) (1 MR).
7. Anthropic Messages adapter (1 MR).
8. Packaging: `usr.bin/agent` Makefile, both arches, `docs/apps` page,
   selftest (1 MR).

Order of magnitude: **~8-10 small MRs, a few weeks of focused work**,
each independently useful and verifiable on both arches under QEMU.

### Risks

- Quality of the agent loop (prompting, context management) is a product
  problem, not a port problem; expect iteration after "it works".
- TLS 1.2-only and static CA roots age; owning the CA/TLS story is a
  recurring cost.
- lwIP under sustained streaming, plus the cooperative-ish scheduler, may
  stall a poll loop; mitigated by the loop being single-threaded and by W-037.
- No code editing UI beyond line mode; it is a CLI agent, not OpenCode's TUI.
  That is an accepted scope cut.
- Secrets: an API key on a multi-user OS. Use `0600` config, never log
  headers, and keep out of argv (K-40..K-44 style discipline applies to any
  new kernel interface; none is needed here).

### What self-hosting gives you for free

This is where the route compounds:

- **The agent can build and test uBixOS on uBixOS.** `bash` runs `bmake`,
  `cc`, `ld.lld`, `git`, and the in-OS `kernel`/`world` builds on both
  arches already work. An agent that edits `/usr/src` and rebuilds the
  kernel it runs on is the dogfooding story.
- **It builds in-OS.** Plain C99 with no exotic deps compiles with `tcc`
  (instant, tiny) and `clang`, so the agent is rebuilt on-device; no cross
  toolchain is needed once on the image.
- **No new runtime.** Nothing here adds an interpreter, GC or JIT to
  maintain; the only new code is ours.

## 5. Route 3: cheaper middle routes

### 3a. Embed a small JS engine (QuickJS or Duktape) and write the agent in JS

- **Duktape is already in-tree and verified running in-OS** (NetSurf). QuickJS
  (C, ~55k lines, ES2020, MIT) is a stronger fit **[unverified on uBixOS]**:
  plain C, no JIT, needs libc + libm, built-in `std`/`os` modules (files,
  `poll`, fork/exec, signals, timers) that map to calls we serve.
- The agent logic lives in JS (fast iteration, user-editable prompts and
  tools, easy plugin story); the C host provides `fetch`-like `http_request()`
  (same libhttp work as route 2), file and process bindings.
- **Missing:** everything route 2 needs in `libhttp`, plus: a QuickJS port
  recipe in `tools/ports/quickjs` (no GNU-make; hand-written build like
  dropbear), a `-fno-exceptions` check (QuickJS is C, so fine), and
  `setjmp`/`longjmp` correctness on both arches (it uses them for error
  handling; verify on aarch64 and x86_64).
- **Size:** route 2 slices 1-3 (shared) + QuickJS port (1 MR) + host bindings
  (1 MR) + JS agent (1-2 MRs) = **~6-8 MRs** if done *instead of* the C
  tool/loop slices, or +3-4 MRs on top of route 2.
- **Risks:** two languages to maintain; JS engine memory/GC under no-preemption
  is fine but unexercised; it does not run OpenCode (still a different
  program). Bun-API shims for OpenCode would be a new project on top.
- **Self-hosting bonus:** QuickJS builds in-OS with clang; scripts are
  editable on-device with `vi` and no rebuild.

### 3b. Run the model locally (llama.cpp on UbixOS)

- llama.cpp is C++17 with threaded matmul (ggml spin barriers), `mmap` of a
  GGUF model, NEON/AVX kernels; no network needed beyond the local loop.
  Mmap, threads and futex exist; it would run the OpenAI-compatible
  `llama-server` on-device, and route 2 would talk to `127.0.0.1`.
- **Missing/uncertain:** (a) userland FP/SIMD state is saved on context switch
  on both arches (kernel itself is FP-free by rule; confirm user NEON/AVX
  save/restore); (b) many-thread spin-wait performance with no preemption
  tuning; (c) multi-GB model files on the FAT/ubixfs write and read path;
  (d) CPU: only usable on aarch64 under HVF or real aarch64 hardware; x86_64
  under TCG on Apple Silicon is not workable; (e) RAM: even a Q4 1.5B model is
  about 1 GB resident.
- **Size:** build recipe (1-2 MRs, CMake cross-build like the LLVM port),
  fix whatever mmap/thread/SIMD gaps appear (2-4 MRs), bench and tune
  (1 MR). **~4-7 MRs**, high variance.
- **Value:** offline, private, and a legitimate showcase; **not** a quality
  coding agent: models that fit the hardware are weak at multi-step tool use.
  Do after route 2 works against a LAN llama-server, since route 2 is
  its prerequisite client.

### 3c. FreeBSD-ABI binaries (e.g. a Go agent)

- uBixOS's POSIX layer is FreeBSD-numbered, so a statically linked
  `GOOS=freebsd` Go binary is conceptually runnable without a libc **[unverified]**.
  Go's FreeBSD runtime needs `kqueue`/`kevent` (NOTIMP here), `thr_new` and
  `_umtx_op` (not present: uBixOS threads are `rfork`+futex), `sigaction`
  with FreeBSD trampolines, `sysctl` for CPU count, and a TLS register model
  that matches. That is a third "FreeBSD-compat" surface to build and keep
  honest, and the agent would still be cross-built (no on-device Go).
- **Not recommended now:** high cost (new syscalls that nothing else needs),
  and it bends the native-ABI-core principle toward becoming a FreeBSD clone.
  Revisit only if a ready-made Go agent is wanted and kqueue lands for other
  reasons.

## 6. Comparison

| | Route 1: Node/Bun + OpenCode | Route 2: native C agent | 3a: QuickJS host | 3b: local llama.cpp | 3c: Go (FreeBSD ABI) |
|---|---|---|---|---|---|
| Runs real OpenCode | needs shim/fork | no | no | no | no |
| New kernel work | tail of syscalls, libuv backend | none expected | none expected | maybe SIMD/threads | kqueue, umtx, thr_new |
| New userland runtime | V8, libuv, OpenSSL, Node | none | QuickJS | llama.cpp | Go runtime (cross) |
| MR estimate | 25-35, large | 8-10, small | 6-8 | 4-7 (+ route 2) | 10+, unclear |
| Time to first useful result | months | weeks | weeks | months | months |
| Both arches | hard (x86 TCG slow) | yes | yes | aarch64 only in practice | likely |
| Fits native-core / POSIX-for-ports | strains | fits | fits | fits | strains |
| Self-hosting leverage | tools only | tools + builds in-OS | tools + builds in-OS | builds in-OS (CMake heavy) | tools only |
| Main risk | V8 build, OpenCode drift | TLS 1.2, agent quality | two languages | model quality, speed | ABI creep |

## 7. Recommendation

**Do route 2: a native C agent.** It is the only route whose cost is bounded
and whose slices each ship something usable; it needs no new runtime and no
kernel work that the tree does not already have; it exercises the real
POSIX surface (fork/exec/pipe/poll/sockets/TLS) the OS already claims; and
it compounds with self-hosting (the agent can build the OS it runs on, and
itself, on-device, both arches).

Position the other routes as follows:

- **Route 1** stays a long-term, optional track under `nodejs-port-plan.md`;
  its N0 spike is cheap and independently useful (a JS engine on both
  arches), but do not gate the agent on it. If OpenCode compatibility ever
  matters, the shared prerequisite is the same: a solid TLS/HTTP/process
  layer, which route 2 delivers.
- **Route 3a** is a reasonable *second* step once the `libhttp` request API
  exists, if scriptable tools/plugins are wanted.
- **Route 3b** follows route 2 (as a backend it talks to), aarch64-first.
- **Route 3c** is not recommended.

### First slice

**MR 1: `libhttp` request API** (scope: `lib/libhttp/`, `include/http/`,
`usr.bin/httpsget`):

- add `http_request()` with method, extra headers, request body, and a
  chunked/Content-Length body reader that delivers bytes through a callback
  (the SSE hook for the next slice); keep `http_get()` as a thin wrapper;
- connect/read timeouts via `poll()`;
- extend `httpsget` (or a test program in `tests/`) with `-X POST -d` so it is
  verifiable: POST JSON to a LAN `llama-server` over plain HTTP and to an
  HTTPS echo endpoint;
- both arches built and booted per P-1/P-4; Doxygen on new functions; no
  kernel change.

Why this first: it is small, independent, immediately useful to every other
route (and to `git` https transport, `wget`, NetSurf), and it answers the
route's riskiest unknown early (does a minutes-long streamed TLS 1.2 response
survive lwIP on both arches?) before any agent code exists.

### Open questions for the person

1. Is TLS 1.2-only acceptable for the target providers, or should a TLS 1.3
   story (BearSSL fork, or a LAN proxy) be scoped now? Verify the providers'
   current minimum TLS version before MR 6.
2. Where do API keys live: env, `/etc/agent.conf`, or ubistry (per-user layer)?
3. Is a line-mode CLI acceptable as "OpenCode-like", or is a `views` window /
   full-screen TUI part of the goal (adds 2-4 MRs, uses `libedit`/termios or
   objGFX)?
4. Do you want a work-matrix row (`docs/WORK.md`) per route? This lane is
   scoped to `docs/research/` and did not touch it.
