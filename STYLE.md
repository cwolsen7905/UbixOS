# UbixOS Coding Style Guide

This is the single source of truth for **naming, headers and formatting** in
uBixOS. Tools derive from it: `.clang-format` enforces formatting,
`.clang-tidy` enforces naming. Language, safety and kernel rules are in
[`docs/standards/`](docs/standards/README.md) (start with `coding.md`);
`CONTRIBUTING.md` references both rather than duplicating them.

Where this file and `.clang-format` disagree, **`.clang-format` wins** — it is
what the `format` gate runs.

Enforcement is incremental — new code must follow these rules; existing code
is renamed as files are touched, or in dedicated per-subsystem cleanup commits.

---

## Formatting

Enforced by `.clang-format`. Run `clang-format -i <file>` or
`tools/mcr.sh --fix` before committing.

- **Indentation**: 8-space hard tabs, never spaces.
- **Braces**: Allman style — opening brace on its own line for functions;
  same line for control flow (`if`, `for`, `while`, `do`).
- **Line length**: 120 columns (`ColumnLimit` in `.clang-format`).
- **Pointers**: aligned to the variable name — `int *foo`, not `int* foo`.
- **Spaces**: one space after keywords (`if (`, `for (`), no space for
  function calls (`foo(`).

---

## Naming Conventions

### Functions

**Kernel** (`sys/`): `subsystem_verb_noun`

```c
vmm_find_free_page()
vmm_copy_virtual_space()
sched_new_task()
sched_yield()
vfs_open_file()
sys_open()
kern_execve()
```

Common subsystem prefixes:

| Prefix   | Subsystem                        |
|----------|----------------------------------|
| `vmm_`   | Virtual memory manager           |
| `sched_` | Scheduler                        |
| `sys_`   | Syscall handlers                 |
| `kern_`  | General kernel helpers           |
| `vfs_`   | Virtual filesystem layer         |
| `mpi_`   | Message-passing IPC              |
| `pci_`   | PCI bus                          |
| `isa_`   | ISA bus / legacy drivers         |
| `net_`   | Network stack                    |
| `dev_`   | Device layer                     |

**Userland** (`bin/`, `lib/libc/` custom code): `verb_noun`, no prefix.

```c
parse_input()
exec_program()
free_args()
print_prompt()
```

Standard C library names (`fwrite`, `fread`, `malloc`, etc.) are left as-is —
they follow POSIX, not this guide.

**UbixOS API** (`lib/ubix_api/`): `ubix_verb_noun`

```c
ubix_getcwd()
ubix_spawn()
```

### Structs

Snake_case tag, no suffix:

```c
struct task_struct { ... };
struct vmm_page    { ... };
struct file_desc   { ... };
struct spin_lock   { ... };
```

### Typedefs

Snake_case with `_t` suffix. Struct typedefs use the struct tag as the base:

```c
typedef struct task_struct   task_t;
typedef struct vmm_page      vmm_page_t;
typedef struct file_desc     file_desc_t;
typedef struct spin_lock     spin_lock_t;
```

Primitive / integer typedefs also use `_t`:

```c
typedef uint32_t   pid_t;
typedef uint32_t   addr_t;
```

**No pointer typedefs.** Do not hide pointers behind a typedef. Write
`task_t *foo` explicitly. Existing pointer typedefs (`spinLock_t`,
`dirList_t`) are cleaned up as files are touched.

### Enums

Type name uses snake_case with `_t` suffix. Values use `ALL_CAPS` with a
subsystem prefix matching the type name:

```c
typedef enum task_state {
        TASK_READY,
        TASK_RUNNING,
        TASK_DEAD,
        TASK_WAIT,
        TASK_FORK,
} task_state_t;

typedef enum syscall_type {
        SYSCALL_INVALID,
        SYSCALL_VALID,
        SYSCALL_NOTIMP,
} syscall_type_t;
```

### Global Variables

`g_` prefix + snake_case. Every file-scope or extern global must carry `g_`:

```c
task_t          *g_current;
vmm_page_t      *g_vmm_memory_map;
uint32_t        *g_kernel_page_dir;
uint32_t         g_free_pages;
sys_vitals_t    *g_system_vitals;
```

Static file-scope variables (not exported) also use `g_` — they are still
global within their translation unit and the same reasoning applies.

### Local Variables and Parameters

Snake_case. Short well-known abbreviations are acceptable (`fd`, `err`, `buf`,
`pid`, `td`) but prefer the full word when the scope is more than a few lines:

```c
int             error_code = 0;
struct file    *file_desc;
uint32_t        page_table;
pid_t           target_pid;

/* short forms acceptable in tight loops / syscall stubs */
int             err;
struct file    *fd;
uint8_t        *buf;
```

### Macros and `#define` Constants

`ALL_CAPS_SNAKE_CASE` always, for both constants and function-like macros
(the addresses below are illustrative i386 values, not current layouts):

```c
#define PAGE_SIZE               4096
#define PAGE_MASK               (PAGE_SIZE - 1)
#define VMM_KERN_START          0xC0000000
#define VMM_USER_START          0x00800000

#define K_PANIC(msg)            do { ... } while (0)
#define PD_INDEX(addr)          ((addr) >> 22)
#define PT_INDEX(addr)          (((addr) >> 12) & 0x3FF)
```

### File Names

Snake_case. If the file implements a specific function or subsystem operation,
name it after that:

```c
vmm_memory.c
vmm_copy_virtual_space.c
vmm_find_free_page.c
sched_new_task.c
vfs_file.c
fork.c
paging.c
```

Header files follow the same rule. One header per logical unit.

### Header Guards

`UBIXOS_<SUBSYSTEM>_<FILENAME>_H` — project-namespaced, all caps, no leading
or trailing underscores (leading underscores are reserved by the C standard):

```c
#ifndef UBIXOS_VMM_PAGING_H
#define UBIXOS_VMM_PAGING_H

/* ... */

#endif /* UBIXOS_VMM_PAGING_H */
```

The closing `#endif` must carry a comment repeating the guard name.

---

## Comments

- Inside function bodies, default to **no comments**. Well-named identifiers
  are self-documenting.
- Add a comment only when the **why** is non-obvious: a hardware quirk, a
  subtle invariant, a workaround for a specific bug, or behavior that would
  surprise a reader.
- Never explain what the code does — the code does that.
- Never reference the task, PR, or caller in a comment — that belongs in the
  commit message.
- Delete commented-out code.

**Every new or substantially changed function — `static` helpers included —
has a Doxygen block** immediately above its definition (format in `CLAUDE.md`
and `docs/standards/coding.md` C-40). This replaces the older guidance that
only public API functions in headers were documented; the `doxygen-docs` gate
enforces it on functions added in a diff.

```c
/**
 * Return the first free physical page frame.
 *
 * @return the frame number, or 0 on exhaustion.
 */
uint64_t vmm_find_free_page(void);
```

---

## Types

Prefer fixed-width types from `<sys/types.h>`:

```c
uint8_t, uint16_t, uint32_t, uint64_t   /* unsigned */
int8_t,  int16_t,  int32_t,  int64_t    /* signed   */
```

Pointers and sizes use `uintptr_t`, `size_t`, `ptrdiff_t` — the tree is LP64;
never assume `long` or a pointer is 32 bits (coding.md C-12).

Avoid `uInt8`, `uInt16`, `uInt32` (legacy UbixOS aliases) in new code.
Replace them as files are touched.

---

## Incremental Migration

The codebase is being migrated to this style subsystem-by-subsystem. The rules:

1. **New code** must follow this guide from day one.
2. **Modified files** — apply naming fixes to any identifier you touch in that
   file. Do not leave a file half-migrated.
3. **Subsystem cleanup** — dedicated commits that rename a full subsystem
   (e.g., all of `sys/vmm/`) are encouraged. Keep them separate from
   functional changes so `git bisect` stays useful.
4. **Do not** reformat or rename identifiers in a file you are not otherwise
   changing — it pollutes `git blame`.

Migration priority order (highest impact / most-read code first):

1. `sys/include/` — headers propagate naming everywhere
2. `sys/vmm/`
3. `sys/kernel/`
4. `sys/arch/aarch64/`, `sys/arch/x86_64/`
5. `sys/fs/vfs/`
6. `lib/libc/`
7. Remaining `sys/` subsystems
8. `bin/`

---

## Tooling Quick Reference

```sh
# Format a single file in place
clang-format -i sys/vmm/paging.c

# Run the gate suite (format, banned APIs, Doxygen, statics, tidy) on changed files
tools/mcr.sh
tools/check/run.sh            # the same gates, directly

# Auto-apply clang-format fixes
tools/mcr.sh --fix

# Check staged files only (pre-commit)
tools/mcr.sh --staged

# Check a specific file
tools/mcr.sh sys/vmm/paging.c
```
