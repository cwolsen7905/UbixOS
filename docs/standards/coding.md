# Coding Standard

Applies to every `.c`, `.cc`, `.cpp`, `.h`, `.S` file under `sys/`, `bin/`,
`sbin/`, `usr.*`, `lib/`, `tests/`. Third-party code in `contrib/` is not
modified and not checked.

Naming, header guards and file naming are in [STYLE.md](../../STYLE.md).
This document adds the rules STYLE.md does not have.

Rule IDs (`C-n`) are stable so reviews and gate output can cite them.

## Formatting

- **C-1** `clang-format` with the repo `.clang-format` (FreeBSD `style(9)`:
  8-column hard tabs, Allman function braces, 120 columns, `int *foo`).
  Enforced on every touched file. Never reformat lines you didn't change
  (it wrecks `git blame`) — format touched files, review the diff.
- **C-2** A new file starts with the project licence header and a copyright
  range ending in the current year; touching a file bumps its end year.

## Language

- **C-10** C as the build compiles it (kernel `-std=c99` plus the GNU extensions in
  `sys/include/sys/cdefs.h`); don't rely on newer-standard features the Makefiles
  don't enable. No VLAs, no `alloca`, no
  recursion in kernel code (stacks are 8 KiB — see kernel-rules).
- **C-11** Every function has a prototype visible at its definition and at
  every call site. A missing prototype silently truncates a 64-bit pointer to
  `int` on LP64; the build promotes `-Werror=implicit-function-declaration`
  for exactly this reason. Never add `-Wno-*` to make a warning go away —
  fix the code.
- **C-12** No casts between pointer and `int`/`long`. Use `uintptr_t`,
  `intptr_t`, `size_t`, `ptrdiff_t`, `uint64_t`. Never assume
  `sizeof(long) == 4` or `sizeof(void *) == 4`; i386 is gone.
- **C-13** Fixed-width types for anything that crosses a boundary: disk
  formats, wire protocols, register images, syscall argument structs, shared
  memory. Never `int`/`long` there. Mark on-disk/wire structs `__packed` and
  `_Static_assert(sizeof(...) == N)` them.
- **C-14** Every `switch` has a `default`, or covers an enum fully and says so.
  Intentional fall-through is annotated `/* FALLTHROUGH */`.
- **C-15** `static` for everything not in a header. File-scope variables use
  the `g_` prefix and sit at the top of the file after `#include`s (see
  CLAUDE.md "File-scope variable rules"). No dead file-scope variables.
- **C-16** Headers are self-contained (compile on their own), have
  `UBIXOS_<SUBSYS>_<FILE>_H` guards, declare no non-`static inline` function
  bodies and no variable definitions.
- **C-17** `const` where data is not modified; `volatile` only for MMIO and
  state shared with interrupt context — and then you also need a barrier or a
  lock, `volatile` alone is not synchronisation.
- **C-18** Assembly (`.S`): one function per symbol with `.type`/`.size`,
  documented register/stack contract in a comment block, and an `ENTRY`/`END`
  style consistent with its neighbours. Stack-alignment and callee-saved
  register rules of the target ABI (AAPCS64 / SysV x86_64) are met at every
  call out of assembly.

## Memory and bounds (CERT ARR/MEM/STR)

- **C-20** Every buffer write carries an explicit capacity. Use `snprintf`, or `memcpy`
  with a checked length and an explicit terminator (`strlcpy`/`strlcat` only
  where the library you link provides them; the kernel lib may not). Banned in new
  code: `strcpy`, `strcat`, `sprintf`, `vsprintf`, `gets`, `strncpy` used as
  a "safe strcpy" (it does not terminate), `strtok` (not re-entrant).
- **C-21** Check an allocation for failure before use. `kmalloc` can return
  `NULL`; so can `malloc`. The failure path must free what was already
  acquired (single `goto out;` cleanup label is the preferred shape).
- **C-22** Every allocation has exactly one owner and one `free` on every
  path. Pointers are set to `NULL` after free when they outlive the scope.
- **C-23** Integer arithmetic that feeds an allocation size, an index, a
  loop bound or a length must be checked for overflow *before* it happens
  (`a > SIZE_MAX / b`), not after. Signed overflow is undefined; sizes and
  counts are `size_t`/unsigned.
- **C-24** No use of uninitialised memory. Zero (`memset`/`= {0}`) any
  structure copied to userland or onto disk — padding leaks kernel data.
- **C-25** No `memcpy` between overlapping regions (`memmove`), no `sizeof`
  on a pointer where the pointee was meant, no `sizeof(array_param)`.

## Error handling

- **C-30** Every call that can fail is checked; a deliberately ignored result
  is cast `(void)` with a one-line reason.
- **C-31** A function's failure convention is stated in its Doxygen
  `@return`: `0` / `-errno` (the syscall-layer convention, see
  [abi-and-syscalls.md](abi-and-syscalls.md)), or a documented
  pointer/`NULL`. Within one subsystem use one convention. Never return a bare
  `-1` from a function whose callers need to know *why* it failed.
- **C-32** Errors are propagated, not swallowed. `K_PANIC`/`kpanic` is for
  *kernel invariant violations* only — never for bad user input, a missing
  device, or an allocation failure.
- **C-33** No `assert()`-only validation of external input. Anything that
  comes from userland, a disk, a network packet or a device is hostile until
  checked.

## Documentation and comments

- **C-40** Every new or substantially changed function has a Doxygen block
  (`/** brief ... @param (only when non-obvious) ... @return ... */`) —
  exact format in CLAUDE.md. The `doxygen-docs` gate enforces it on added
  function definitions.
- **C-41** Comments inside bodies explain *why* (hardware quirk, invariant,
  workaround), never *what*. No task/PR/caller names in comments. Delete
  commented-out code; git has it.
- **C-42** `TODO`/`FIXME` carry an owner or a `BUGS.md`/`TODO.md` id.
- **C-43** Do not leave debug `kprintf`s or `-DDEBUG_*` flags enabled.
  Diagnostic output goes through the klog/`kprintf` levels that exist; never
  `printf` in kernel code.

## Userland specifics

- **C-50** Userland links the musl libc (FreeBSD syscall numbers, see ABI
  doc). Do not call kernel-internal headers (`sys/include`) from userland.
- **C-51** Only `views` maps the framebuffer (`sys_mapfb()`); all other
  processes draw into a shared-memory buffer with objGFX. MPI carries
  signals only, never pixels.
- **C-52** Programs that are exec'd by absolute path must be updated
  everywhere that path appears when they move (`etc/init.d`, `tools/makereg.c`,
  `bin/init/main.c`).

## Tooling mapping

| Rule | Gate (`tools/check/`) |
|------|----------------------|
| C-1 | `format` |
| C-11, C-12, C-14 | `warnings`, `clang-tidy` (bugprone, cert) |
| C-15 | `file-scope-static`, `clang-tidy` (misc-use-internal-linkage) |
| C-16 | `header-selfcontained` |
| C-20 | `banned-api` |
| C-40 | `doxygen-docs` |
| C-21, C-22, C-23, C-24 | `clang-tidy`, `scan-build`, `cppcheck`, review |
| everything else | `review-checklist.md` |
