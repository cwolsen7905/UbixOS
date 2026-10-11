# Contributing to UbixOS

---

## Code Style

The engineering rules live in **[docs/standards/](docs/standards/README.md)**:
language rules (`coding.md`), kernel rules (`kernel-rules.md`), the ABI and
syscall rules (`abi-and-syscalls.md`), architecture parity (`arch-parity.md`),
the review checklist and the definition of done. **[STYLE.md](STYLE.md)** is
the source of truth for naming, header guards and formatting. Tools
(`.clang-format`, `.clang-tidy`, `tools/check/`) derive from them.

Quick reference:

```sh
clang-format -i sys/vmm/paging.c   # reformat one file in place
tools/check/run.sh                 # run the gates on files changed vs master
tools/mcr.sh --fix                 # apply clang-format to changed files, then gate
tools/check/run.sh --list          # every gate and its tier
```

---

## Build and Test

Before pushing, verify **both** 64-bit architectures build and boot, and the
gates pass (see [definition-of-done.md](docs/standards/definition-of-done.md)):

```sh
tools/check/run.sh --tier ci          # fast gates + header/warnings/tidy/cppcheck
tools/buildlock.sh with "<who>" bmake kernel world image TARGET=aarch64
tools/buildlock.sh with "<who>" bmake kernel world TARGET=x86_64
bmake run-debug-aarch64               # boot to login, confirm no panic on serial
bmake run-debug-x86_64 TARGET=x86_64
```

Kernel-only changes: `bmake kernel TARGET=<arch>` then `bmake kernel-to-image`.
`bmake image` is preferred over `install-world` (the latter rsyncs the source
tree and is slow). i386 is frozen on `releng/2`; do not build or fix it here.

---

## Commit Messages

Use the imperative present tense and lead with the subsystem:

```
subsystem: short one-line summary (≤72 chars)

Optional body explaining *why*, not *what* — the diff explains what.
Reference files and line numbers for non-obvious changes.
```

No `Co-authored-by:` or tool-attribution trailers. For a bug fix, say how it was
reproduced and on which architectures it was verified.

Examples:
```
vmm: fix off-by-one in vmm_findFreePage loop bound
sys/fs/vfs: restore TTY ownership to parent on task exit
bin/shell: add Ctrl-C kill and TTY handoff
```

---

## Documentation Sync Rules

Every change to source must update the relevant documentation in the **same
commit**. The table below maps change types to the documents that must stay
in sync.

| Change type | Documents to update |
|------------|---------------------|
| New syscall (either table) | `SYSCALLS.md` — add row to the correct table |
| Bug fixed | `BUGS.md` — mark row `~~FIXED~~`; add to `CHANGELOG.md` [Unreleased] |
| New known bug | `BUGS.md` — add a new row with ID, file, and description |
| New TODO / improvement noted | `TODO.md` — add a row; reference the file and line |
| New userland binary in `bin/` | `ARCHITECTURE.md` — add to the Executables table |
| New driver in `sys/isa/` or `sys/pci/` | `ARCHITECTURE.md` — add to the appropriate driver table |
| New third-party component in `contrib/` | `ARCHITECTURE.md` — add to Third-Party Components |
| VMM internals changed | `docs/architecture/vmm.md` |
| Scheduler or task-switch changed | `docs/architecture/task-switching.md` |
| Page-table / address-space layout changed | `docs/architecture/vmm.md` (and the arch's section; `i386-page-directory-map.md` is legacy i386) |
| Engineering rule added or changed | `docs/standards/*` and, if automatable, a gate in `tools/check/` + its self-test |
| Version bump | `sys/include/ubixos/version.h` + `CHANGELOG.md` (see checklist below) |
| Build system change | `BUILDING.md` |
| New debug define added | `DEBUG.md` — add to the defines table |
| New external reference spec | `docs/reference/external-specs.md` |
| New driver API or `deviceAdd()` signature changes | `docs/drivers/writing-a-driver.md` |

### What does NOT need a doc update

- Refactors that don't change behavior or API surface
- Bug fixes fully described by the `BUGS.md` entry
- Internal rename of a local variable or helper function
- Adding or removing a `kprintf` debug statement

When in doubt: if a future developer reading only the docs would be surprised
by the code, update the doc.

---

## Version Bump Checklist

1. Edit `UBIXOS_VERSION_MAJOR`, `MINOR`, `PATCH`, and `TAG` in
   `sys/include/ubixos/version.h`.
2. Rename `[Unreleased]` → `[X.Y.Z-TAG] - YYYY-MM-DD` in `CHANGELOG.md`
   and add a fresh empty `[Unreleased]` above it.
3. Update the version badge in `README.md`.
4. Rebuild: `bmake kernel world image`
5. Commit: `git add sys/include/ubixos/version.h CHANGELOG.md README.md`
6. Tag: `git tag -a vX.Y.Z-TAG -m "Release X.Y.Z-TAG" && git push --tags`

---

## Pull Request Checklist

- [ ] `bmake kernel world` succeeds for **both** `TARGET=aarch64` and `TARGET=x86_64`
- [ ] Both boot to the login prompt in QEMU with no panic on the serial console
- [ ] `tools/check/run.sh` passes (no new findings; baselines not raised)
- [ ] `docs/standards/review-checklist.md` walked, deviations explained in the PR
- [ ] Relevant documentation updated in the same commit (see table above)
- [ ] `CHANGELOG.md` [Unreleased] section updated
- [ ] `clang-format` applied to all touched files (`tools/mcr.sh --fix`)
- [ ] No debug `kprintf` statements or debug defines left enabled
- [ ] No `-DDEBUG_*` flags left in Makefiles

---

## Adding a New Syscall

The full rules are in [docs/standards/abi-and-syscalls.md](docs/standards/abi-and-syscalls.md).
In short:

1. Pick the table: the FreeBSD-numbered POSIX table (`sys/posix/syscalls_posix.c`)
   only for calls FreeBSD defines, at FreeBSD's number; everything else goes in
   the native table (`sys/kern/syscalls.c`), append-only.
2. Add an `args` struct to `sys/include/sys/sysproto_posix.h` or `sys/include/sys/sysproto.h`.
3. Implement a machine-independent handler in a subsystem file (e.g.
   `sys/posix/`), validating and copying every user pointer; return negative errno.
4. Register it — replace the `SYSCALL_NOTIMP`/invalid slot with
   `{ ARG_COUNT(...), "name", (sys_call_t *)handler, SYSCALL_VALID }`.
5. Add a userland wrapper (`lib/libc/sys/` or `lib/ubix_api/`) and confirm the
   number matches. Do not add per-arch pre-dispatch `case`s.
6. Test on aarch64 **and** x86_64 (success, error and bad-pointer paths);
   `tools/check/run.sh syscall-table` must stay clean.
7. Update `CHANGELOG.md` `[Unreleased]` (and the syscall reference).

---

## Adding a New Driver

See [docs/drivers/writing-a-driver.md](docs/drivers/writing-a-driver.md) for
the full guide. After adding:

- Add the file and device to the appropriate table in `ARCHITECTURE.md`.
- Register the driver through the device model as described in the guide
  (do not hard-code addresses; discover via PCI/DTB/ACPI), and make sure it is
  built for **every arch that has the bus** — aarch64 and x86_64 differ here.

---

## docs/ Ownership

Each file in `docs/` is owned by the subsystem it describes. If you change
the corresponding source, you own the doc update:

| Doc file | Owned by |
|----------|----------|
| `docs/architecture/vmm.md` | `sys/vmm/` changes |
| `docs/architecture/task-switching.md` | `sys/arch/*/kern` context-switch, `fork`, scheduler changes |
| `docs/architecture/i386-page-directory-map.md` | legacy i386 only (frozen on `releng/2`) |
| `docs/standards/*` | engineering rules; changes need the person's approval |
| `docs/design/fbcon.md` | framebuffer console implementation |
| `docs/drivers/writing-a-driver.md` | `sys/include/sys/device.h` API changes |
| `docs/reference/external-specs.md` | new external specifications added |
