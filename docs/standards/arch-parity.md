# Architecture Parity

uBixOS master is 64-bit only. **aarch64 is the default and primary forward
target; x86_64 is the abstraction anchor** — the second, deliberately
different machine that keeps the machine-independent / machine-dependent
split honest. i386 is frozen on `releng/2`.

- **P-1** *Both* architectures must build (`bmake kernel world TARGET=aarch64`
  and `TARGET=x86_64`) and boot to the login prompt before a change is done.
  Breaking one to advance the other is not allowed.
- **P-2** Put code at the right layer. Anything that does not need an
  instruction, register, MMU descriptor or device address of one CPU lives
  outside `sys/arch/`. If you find yourself writing the same logic in both
  `sys/arch/aarch64/` and `sys/arch/x86_64/`, hoist it to MI code and leave
  only the primitive behind an arch-neutral declaration.
- **P-3** Arch directories hold the minimum: boot, exceptions/interrupts,
  context switch, MMU, syscall entry glue, board/device bring-up. The
  `bringup/` directory is transitional — new code does not go there; code
  graduates out of it.
- **P-4** Every arch-specific *fix* is checked against the other arch: either
  show the same bug cannot exist there (say why in the commit message) or fix
  both. "Works on aarch64" is not done.
- **P-5** No arch conditionals in MI code: no `#ifdef __aarch64__` in
  `sys/kern`, `sys/vmm` (outside the arch pmap boundary), `sys/fs`,
  `sys/net`, `sys/posix`. Use a per-arch header macro/inline or a function
  pointer. Board differences (`BOARD_RPI3` etc.) belong in `sys/arch/*/board`.
- **P-6** Never assume x86 behaviour (strong memory ordering, coherent DMA,
  implicit I/O ordering, unaligned access cost, little-endian-only device
  data). aarch64 code must use barriers where order matters; code in MI
  must be correct under the weakest model.
- **P-7** Never assume 4 KiB pages or a fixed `PAGE_SIZE` in MI/userland
  code — read `AT_PAGESZ`/`sysconf` or the arch header (aarch64 has no fixed
  `PAGE_SIZE` macro; malloc rounds to zero if the auxv entry is missing).
- **P-8** Syscall numbers and structure layouts are identical across arches
  (see abi-and-syscalls.md).
- **P-9** Scripts and Makefiles take the arch from `TARGET=`/`OBJ_DIR`; no
  path hard-codes `build/aarch64` or `build/x86_64`.

## i386

Do not add i386 conditionals, and do not spend effort keeping it green
here. 32-bit fixes go to `releng/2`. Do not delete i386 history or the
`releng/2` branch.

## How parity is checked

- `tools/check/run.sh build` (CI matrix) builds both arches.
- `tools/check/run.sh syscall-table` compares the per-arch entry code and
  checks the shared tables for index/number consistency.
- `boot-smoke` boots both arches headless to the login prompt.
