# An OpenCode-like coding agent on uBixOS: routes, gaps and sizes

Status: **research only** (2026-10-14). No code changes. This **supersedes and
extends** [docs/research/agent-on-ubixos.md](research/agent-on-ubixos.md)
(PR #32), which stays as the detailed route walk-through. This document
re-checks the tree against current master (through W-048), adds person-day
sizes and per-arch notes, and adds a WASM route the earlier doc lacked.

Claims about third-party projects (OpenCode, Bun, Node, Go, llama.cpp, WASM
runtimes) are from general knowledge and marked **[unverified]**. Everything
under "State of the tree" was checked against source.

Sizes: **S** ~1-3 person-days, **M** ~4-8, **L** ~9-20, **XL** 20+. A
person-day is focused work including both-arch build/boot verification
(P-1/P-4), Doxygen, and docs.

## 1. What an agent needs

1. TLS HTTP client: POST, custom headers, streaming (SSE) responses.
2. JSON encode/decode.
3. Tools: read/write/edit/list/grep, run a shell command with timeout and kill.
4. Terminal front end: line input, streamed output, permission prompts.
5. Config and secrets (endpoint, model, key).

No GUI, JIT or threads are required. The model runs elsewhere unless local
inference is chosen. Cost is therefore dominated by *how much runtime must be
ported to host the loop*.

## 2. State of the tree (re-verified 2026-10-14)

| Area | State |
|------|-------|
| Sockets | BSD TCP/UDP over lwIP. `socketpair` (135), `kqueue` (362), `kqueuex` (583), `ppoll` (545), `sendfile` (393), `clock_nanosleep` (244) are still `SYSCALL_NOTIMP` in `sys/posix/syscalls_posix.c` |
| HTTP | `lib/libhttp` exposes only `http_get()` (`include/http/http.h`): GET, redirects, whole body buffered. No POST, headers, streaming, timeouts. `usr.bin/wget` exists |
| TLS | BearSSL (`lib/libbearssl`), TLS 1.2 only, static CA table in `lib/libhttp/trust_anchors.c` |
| Process model | fork/execve/wait4/pipe/dup2/poll/select/setpgid; SMP reap race fixed (W-047, both arches). Enough for a `bash` tool |
| Threads, mmap, signals, ptys | rfork+futex threads (W-040/W-041), file/anon mmap, POSIX signals and sigaltstack (W-038), termios, `posix_openpt` |
| Line editing | `lib/libedit` in tree |
| Toolchain | In-OS clang+lld, bmake (musl now has a bmake build, W-043), tcc, git (local), dropbear. Open: W-045 (crt files missing from image; the `cc` wrapper works around it) |
| JS | Duktape runs in-OS inside NetSurf; no standalone interpreter; no QuickJS port |
| W^X | Executable mappings are the real constraint for any JIT; none is exercised in-tree |
| Open perf items | W-044 (idle aarch64 VM burns ~270% host CPU), W-048 (x86_64 serial RX polled) |

Per-arch note: aarch64 runs under HVF on Apple Silicon and is the practical
dev target. x86_64 runs under TCG there: correct but several times slower, so
anything CPU-heavy (JS engines, local inference) is judged on aarch64 first
and only *correctness-checked* on x86_64.

## 3. Route 1: Bun or Node, then OpenCode itself

OpenCode is TypeScript that, as far as I know, targets Bun APIs
(`Bun.spawn`, `Bun.file`, `Bun.serve`, `bun:sqlite`) **[unverified]**. Bun is
Zig + JavaScriptCore with Linux/macOS/Windows event-loop assumptions; a Zig
std target plus runtime port for uBixOS is not realistic. Node is the only
workable base, plus a Bun-API shim or an OpenCode fork.

| Gap | Size | Notes |
|-----|------|-------|
| JS engine, `--jitless` V8 or JSC LLInt-only | L-XL | GN/Chromium-style build against musl; the top risk. A JIT would need a W^X policy (RW then `mprotect` RX) in the VMM, plus icache maintenance on aarch64: M extra |
| libuv backend | L | no kqueue/epoll: write a poll-based backend; real threadpool is possible now |
| OpenSSL | L | Node needs the OpenSSL API; BearSSL cannot stand in |
| Syscall tail | M | `socketpair`, `ppoll`, `clock_nanosleep`, `/proc/self`, `statx`-shaped calls; fill in FreeBSD slots only (A-1..A-12) |
| c-ares, llhttp, nghttp2, brotli, ada, simdutf | M | mechanical ports |
| Node core cross-build | L | high variance |
| Built-ins bring-up (`fs`, `net`, `tls`, `child_process`) | L | |
| OpenCode shim/fork + native TUI deps | L-XL | unknown until its source is read against the platform |
| Dynamic linker / libc++ | S-M | musl `ld-musl` works; libc++ is built without exceptions/RTTI, and V8/Node need to be checked against that (Node uses exceptions in places **[unverified]**) |

**Total: ~100-180 person-days** (~25-35 large MRs), go/no-go after the engine
spike. **Risks:** V8 build; upstream drift (OpenCode moves weekly; a shim rots);
50-90 MB binaries and 100s of MB isolates on a small-RAM target; jitless
startup of seconds on aarch64 and worse on x86_64 TCG; drags in a
Linux-shaped runtime that must be answered inside the FreeBSD-like layer.
**First milestone:** standalone engine prints `1+1` on both arches (L, 10-15
days). **Verdict:** long-term optional; do not gate the agent on it.

## 4. Route 2: native C agent (`uagent`) against llama.cpp / OpenAI-compatible servers

Single-threaded `poll()` loop in `usr.bin/agent`, ~3-5k lines of C, linked
against an extended `libhttp` and BearSSL. A thin `platform.h` keeps the
core (loop, tool dispatch, provider adapters) free of POSIX assumptions so a
native uBix front end (views window, MPI) can reuse it later; the tools
themselves are POSIX because they *are* the POSIX world.

Backend options, in order:

1. **LAN `llama-server`** (llama.cpp's OpenAI-compatible endpoint) over plain
   `http://`: no TLS, so the first end-to-end test avoids the TLS risk.
2. Remote OpenAI-compatible API over HTTPS (OpenRouter etc.).
3. Anthropic Messages adapter (tool use + SSE).
4. On-device inference: route 3b below.

| Prerequisite | Exists? | Size |
|--------------|---------|------|
| `http_request()`: method, headers, body, chunked, streaming callback, timeouts, keep-alive | no | M |
| JSON (vendored cJSON/jsmn, or in-tree) + streamed tool-call assembly | no | S |
| Chat-completions adapter, plain-HTTP end-to-end | no | S |
| Tools: read/write/edit/list/grep | syscalls yes | S |
| `bash` tool: fork+exec, pipe, process group, timeout kill | syscalls yes | S |
| UI: libedit line input, streaming output, y/n prompts | libedit yes | S |
| HTTPS path: CA refresh, wall-clock, TLS 1.2 vs provider minimum | partial | S-M |
| Anthropic adapter | no | S |
| Packaging, both arches, `docs/apps` page, selftest | no | S |
| Long SSE soak on lwIP (minutes, 100s of KB) | untested | S, risk |

**Total: ~20-30 person-days (8-10 small MRs).** No kernel work expected.
Optional niceties (`socketpair`, `ppoll`) are not required.

**Risks:** TLS 1.2-only BearSSL if a provider goes 1.3-only (fallback: LAN
proxy, or a TLS 1.3 library, M-L); lwIP under long streams; agent-loop
quality is a product problem; API-key handling (0600 config, never in argv).
**First milestone:** `http_request()` POSTs JSON to a LAN llama-server and
streams the reply, verified on both arches. **Self-hosting leverage:** the
agent's `bash` tool runs bmake/cc/git on-device, so it can edit `/usr/src` and
rebuild the kernel it runs on; it is plain C99 and rebuilds in-OS with tcc or
clang.

## 5. Route 3: middle routes

### 3a. QuickJS (or Duktape) host, agent logic in JS: M-L (~15-25 days)

Shares the `libhttp` work with route 2. Adds a QuickJS port recipe in
`tools/ports` (hand-written build, as dropbear; `setjmp`/`longjmp` checked on
both arches), host bindings (http, fs, spawn), and the JS agent. Duktape is
already proven in-OS but is ES5-class and slow; QuickJS (ES2020) is the better
engine **[unverified on uBixOS]**. No JIT, so no W^X issue. Value: editable
prompts/tools without rebuilds. Does not run OpenCode. Do after route 2's
transport exists.

### 3b. Local inference: llama.cpp on uBixOS: L (~15-30 days, high variance)

C++17, threaded matmul with spin barriers, mmap'd GGUF, NEON/AVX kernels
**[unverified]**. mmap, threads and futex exist. To confirm: userland SIMD
state saved/restored per thread on both arches; spin-wait behaviour without
preemptive kernel threads; multi-GB reads through the FS; RAM (a Q4 1.5B model
is ~1 GB resident). Practical on aarch64 HVF/real aarch64 only; x86_64 TCG is
not workable. Models that fit are weak at multi-step tool use. It is a
backend for route 2 (`127.0.0.1`), so route 2 comes first.

### 3c. FreeBSD-ABI Go agent: L-XL, not recommended

A static `GOOS=freebsd` binary would need `kqueue`, `thr_new`/`_umtx_op` and
FreeBSD signal trampolines, none of which uBixOS serves (threads are
rfork+futex). That is a new compat surface nothing else needs, it pulls
toward a FreeBSD clone, and the agent would be cross-built only.

### 3d. WASM runtime hosting the agent: M-L (~15-25 days) **[unverified]**

Embed a small interpreter (wasm3, WAMR, or wasmtime/wasmer-class if
Rust-built) and run the agent as a `.wasm` module with WASI. Interpreters
(wasm3, WAMR interpreter mode) are plain C, no JIT, no W^X. Needs: the runtime
port (S-M), a WASI host layer mapped onto POSIX calls (M: `fd_*`, `path_*`,
`poll_oneoff`, sockets are the weak spot since WASI preview1 has no client
sockets and preview2 sockets are heavy), and the agent written in a
wasm-targeting language. Gains: sandboxed, portable, arch-independent agent
binary, and the same artifact on both arches. Costs: two toolchains, sockets
and TLS still have to be host functions, and the interpreter is slower than
QuickJS for string-heavy work. It does not run OpenCode and is not
cheaper than route 2 for the same capability; interesting only if a plugin
sandbox becomes a goal. A Rust wasm runtime additionally needs a Rust
toolchain, which uBixOS does not have.

## 6. Comparison

| | R1 Node + OpenCode | R2 native C | 3a QuickJS | 3b llama.cpp | 3c Go | 3d WASM |
|---|---|---|---|---|---|---|
| Runs OpenCode | with shim | no | no | no | no | no |
| Size (person-days) | 100-180 | 20-30 | 15-25 (+R2 transport) | 15-30 (+R2) | 25+ | 15-25 |
| New kernel work | syscall tail | none | none | maybe SIMD/threads | kqueue, umtx | none |
| New runtime | V8, libuv, OpenSSL | none | QuickJS | llama.cpp | Go (cross) | wasm VM |
| aarch64 / x86_64 | aarch64 usable, x86 slow | both | both | aarch64 only | both | both |
| Fits native core + POSIX-for-ports | strains | fits | fits | fits | strains | fits |
| Main risk | V8 build, drift | TLS 1.2, agent quality | two languages | model quality | ABI creep | sockets/WASI |

## 7. Recommendation

**Route 2 first, then 3a or 3b as optional layers; keep route 1 as a
separate, ungated track; skip 3c; treat 3d as a curiosity.**

Reasons: bounded cost (~4-6 weeks), no new runtime, no kernel work, every
slice shippable and verifiable on both arches, and it exercises exactly the
POSIX surface the OS already claims while compounding with self-hosting.

### First slice (MR 1)

`http_request()` in `lib/libhttp` + `include/http/http.h` (method, headers,
body, chunked/Content-Length reader through a callback, `poll()` timeouts;
`http_get()` becomes a wrapper), and `-X POST -d` in `usr.bin/wget` or a
`tests/` program. Verify by POSTing JSON to a LAN `llama-server` over HTTP
and to an HTTPS echo endpoint, on both arches. It is S-M (~4-6 days), useful
to wget/git/NetSurf regardless, and answers the biggest unknown early: does a
minutes-long streamed response survive lwIP?

### Open questions for the person

1. Is TLS 1.2-only acceptable for target providers, or scope TLS 1.3 now?
2. Where do keys live: env, `/etc/agent.conf` (0600), or ubistry per-user?
3. Is a line-mode CLI "OpenCode-like" enough, or is a full-screen TUI or
   `views` window in scope (+3-6 days)?
4. Add `docs/WORK.md` rows per route? This lane's scope excludes it.
