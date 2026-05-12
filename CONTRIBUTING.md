# Contributing to UbixOS

---

## Code Style

See **[STYLE.md](STYLE.md)** — it is the single source of truth for
formatting, naming conventions, and incremental migration rules.
Tools (`.clang-format`, `.clang-tidy`, `tools/mcr.sh`) derive from it.

Quick reference:

```sh
clang-format -i sys/vmm/paging.c   # reformat one file in place
tools/mcr.sh                        # lint changed files vs HEAD
tools/mcr.sh --fix                  # auto-apply clang-format fixes
```

---

## Build and Test

Before pushing, verify the full build passes:

```sh
bmake clean && bmake && bmake image
bmake run   # boot to login prompt in QEMU, confirm no panic in serial.log
```

For kernel-only changes: `bmake kernel && bmake kernel-to-image && bmake run`.  
For world-only changes: `bmake world && bmake install-world && bmake run`.

---

## Commit Messages

Use the imperative present tense and lead with the subsystem:

```
subsystem: short one-line summary (≤72 chars)

Optional body explaining *why*, not *what* — the diff explains what.
Reference files and line numbers for non-obvious changes.
```

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
| Page directory layout changed | `docs/architecture/i386-page-directory-map.md` |
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

- [ ] `bmake clean && bmake && bmake image` succeeds
- [ ] Boots to login prompt in QEMU with no panic in `serial.log`
- [ ] Relevant documentation updated in the same commit (see table above)
- [ ] `CHANGELOG.md` [Unreleased] section updated
- [ ] `clang-format` applied to all touched files (`tools/mcr.sh --fix`)
- [ ] No debug `kprintf` statements or debug defines left enabled
- [ ] No `-DDEBUG_*` flags left in Makefiles

---

## Adding a New Syscall

1. Pick the table: POSIX (`int 0x80`, `syscalls_posix.c`) or native (`int 0x81`, `syscalls.c`). POSIX numbers follow the FreeBSD i386 ABI.
2. Add an `args` struct to `sys/include/sys/sysproto_posix.h` or `sys/include/sys/sysproto.h`.
3. Implement the handler (usually in `sys/kernel/gen_calls.c` or a subsystem file).
4. Register it in the table — replace `SYSCALL_NOTIMP` at the chosen slot with `{ ARG_COUNT(...), "name", (sys_call_t *)handler, SYSCALL_VALID }`.
5. Add a userland wrapper in `lib/libc/sys/` using the inline-asm `int $0x80`/`int $0x81` pattern.
6. Update `SYSCALLS.md` with a new row in the correct table.

---

## Adding a New Driver

See [docs/drivers/writing-a-driver.md](docs/drivers/writing-a-driver.md) for
the full guide. After adding:

- Add the file and device to the appropriate table in `ARCHITECTURE.md`.
- Register the driver's init function in the `kmain` subsystem init sequence
  (`sys/init/main.c`) and add it to the boot sequence list in `ARCHITECTURE.md`.

---

## docs/ Ownership

Each file in `docs/` is owned by the subsystem it describes. If you change
the corresponding source, you own the doc update:

| Doc file | Owned by |
|----------|----------|
| `docs/architecture/vmm.md` | `sys/vmm/` changes |
| `docs/architecture/task-switching.md` | `sys/arch/i386/sched.c`, `fork.c`, `timer.S` changes |
| `docs/architecture/i386-page-directory-map.md` | `sys/vmm/` layout changes |
| `docs/design/fbcon.md` | framebuffer console implementation |
| `docs/drivers/writing-a-driver.md` | `sys/include/sys/device.h` API changes |
| `docs/reference/external-specs.md` | new external specifications added |
