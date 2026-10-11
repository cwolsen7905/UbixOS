# uBixOS Engineering Standards

These documents are the rules for anyone — human or AI agent — writing
kernel or userland C, C++ or assembly for uBixOS. They are enforced where a
machine can enforce them (`tools/check/`, `.github/workflows/ci.yml`) and by
review where it cannot (`review-checklist.md`).

| Document | Covers |
|----------|--------|
| [coding.md](coding.md) | Language rules: style(9), CERT-C subset, banned APIs, integers, errors, docs, comments |
| [kernel-rules.md](kernel-rules.md) | Kernel-only rules: locking, allocation, user pointers, stacks, MMIO, no FP/SIMD |
| [abi-and-syscalls.md](abi-and-syscalls.md) | Native uBix ABI, FreeBSD-compatible POSIX layer, adding a syscall |
| [arch-parity.md](arch-parity.md) | aarch64 + x86_64 must both stay green; MI/MD split |
| [review-checklist.md](review-checklist.md) | What a reviewer (or the author, before asking) checks |
| [definition-of-done.md](definition-of-done.md) | When a change may be called finished |
| [../../STYLE.md](../../STYLE.md) | Naming and formatting detail (supplements `coding.md`) |

## Precedence

1. `CLAUDE.md` and the documents in `docs/architecture/` describe the system as
   it is; where they state a fact about the code, they win.
2. These standards win over habits visible in legacy code. Existing code that
   violates a rule is not precedent; fix it when you touch it (see
   "Legacy and ratchets" below).
3. Industry references for anything not covered: FreeBSD `style(9)` and the
   committer's guide, SEI CERT C, and (as a concept, not a licence to copy
   code) the Linux kernel's `submitting-patches` / `checkpatch` discipline.

## Legacy and ratchets

The tree is older than these rules. The gates therefore work on **changed
lines/files**, and where a whole-tree count is meaningful they use a
**baseline file** in `tools/check/baselines/`: the count may go down, never up.
Do not edit a baseline upward to make a gate pass. Lowering one is welcome and
should be its own commit.

## Known conflicts in older documents

- `STYLE.md` says 80 columns; `.clang-format` (the enforced source) and
  `CLAUDE.md` say **120**. 120 is correct.
- `STYLE.md` says "default to no comments / header comments only for public
  API"; `CLAUDE.md` makes a Doxygen block **mandatory on every new function**.
  The Doxygen rule wins; the "don't narrate the code" advice still applies to
  comments inside function bodies.
- `STYLE.md` examples use 32-bit constants (`0xC0000000`) — illustrative only.
