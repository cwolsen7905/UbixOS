# Review Checklist

Use before asking for review (author) and during review (reviewer). A "no"
without an explanation blocks the change. AI agents: answer these explicitly
in your report, with the evidence (command + result), not "should be fine".

## Scope and intent
- [ ] The diff does one thing; unrelated cleanups are separate commits.
- [ ] No reformat/rename of lines you did not otherwise change.
- [ ] The commit message says *why* (subsystem: summary, imperative, ≤72 cols).

## Correctness
- [ ] Every user pointer/length validated and copied (K-40..K-44).
- [ ] Every allocation checked, freed on every path, single owner (C-21/22).
- [ ] Integer overflow considered for every size/index/length (C-23).
- [ ] Lock held/taken/ordered correctly; context (IRQ/thread/SMP) stated (K-20..25).
- [ ] Error paths release what was acquired; `-errno` convention kept.
- [ ] MMIO/device frames handled wherever frames are freed/refcounted (K-31).
- [ ] No kernel FP/SIMD, no VLA/recursion/large stack buffers (K-1, K-10).
- [ ] Padding/uninitialised bytes cannot leak to user/disk (C-24).

## Architecture and ABI
- [ ] Builds for aarch64 **and** x86_64; boots on both (P-1).
- [ ] The bug/feature considered on the *other* arch (P-4).
- [ ] No arch `#ifdef` in MI code (P-5); no 32-bit/4 KiB assumptions (C-12, P-7).
- [ ] Syscall changes follow abi-and-syscalls.md; FreeBSD slots untouched (A-1..A-5).

## Style and docs
- [ ] `tools/check/run.sh --changed` clean (format, banned API, Doxygen, statics, headers).
- [ ] New functions have Doxygen blocks (C-40); statics at top of file (C-15).
- [ ] Copyright end year updated on touched files.
- [ ] Docs updated per the CONTRIBUTING.md sync table; `CHANGELOG.md [Unreleased]`.
- [ ] No debug prints / `-DDEBUG_*`, no commented-out code, no stray files.

## Tests
- [ ] A test or reproduction exists in `tests/` or the commit message for the bug/feature.
- [ ] The failure path was exercised, not only the happy path.

## Slop check (agent-written code)
- [ ] Every function/API/flag used actually exists in this tree (grep it).
- [ ] No invented helper names, no "TODO: implement" bodies, no placeholder returns.
- [ ] No narrating comments; no defensive checks for impossible states that hide bugs.
- [ ] No new abstraction with a single caller; no re-implementation of an existing helper.
