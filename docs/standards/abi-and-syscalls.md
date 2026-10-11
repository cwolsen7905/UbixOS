# ABI and Syscall Rules

## Design principle

uBixOS has **one native ABI** and a **compatibility layer**:

1. The **native uBix ABI** is the primary, first-class interface. It is what
   uBix-specific facilities (MPI mailboxes, klog, display claim, ubistry,
   `ubix_getcwd`, thread primitives) are defined in. It is table-driven
   (`systemCalls[]` in `sys/kern/syscalls.c`). Userland reaches it through
   `lib/ubix_api/` and, from musl, through the `NATIVE_FLAG` bit on the
   syscall number.
2. The **POSIX layer** is a *FreeBSD-ABI-compatible* table
   (`systemCalls_posix[]` in `sys/posix/syscalls_posix.c`) so that
   FreeBSD-derived userland and a FreeBSD-numbered musl work unmodified.
   Slot numbers are FreeBSD's and are **fixed**.

Consequences — each is a rule:

- **A-1** Do not squat on a FreeBSD slot for something FreeBSD does not
  define. Linux-isms and uBix-isms (futex, set_thread_area, exit_group,
  MPI, ...) go in the native table.
- **A-2** Patch the *client* (musl constants, libc wrappers) to match the
  FreeBSD numbers, never the other way round. The FreeBSD slot table is the
  contract.
- **A-3** The native table is append-only. A published native number is never
  reused or renumbered; retire it with `SYSCALL_INVALID`.
- **A-4** Do not add arch-specific syscall behaviour. The tables and handlers
  are machine-independent. The per-arch entry code (`sys/arch/aarch64/kern/syscall.c`,
  `sys/arch/x86_64/kern/syscall_md.c`) only decodes registers and dispatches.
  Pre-dispatch `case`s there exist for bring-up reasons and are being retired
  into the shared table (see `docs/design/console-and-arch-convergence-plan.md`);
  **do not add new ones**. If the generic dispatcher cannot handle an
  argument shape (e.g. two 32-bit fields packed in a 64-bit slot), fix the
  argument struct or the dispatcher, don't fork the call path per arch.
- **A-5** Argument structs are fixed-width and padded identically on every
  arch (`PADL_`/`PADR_` conventions from `sysproto*.h`); `_Static_assert`
  their size. 64-bit values are not passed in "two registers" as on i386.

## Error convention

- **A-10** A handler signals failure with a **negative errno** in
  `td->td_retval[0]`. The dispatcher returns `td_retval[0]` to the arch glue,
  which places it in the user's return register. musl treats a return in
  `[-4095, -1]` as an error; there is no carry-flag channel.
- **A-11** Unknown/unimplemented numbers return `-ENOSYS` (the dispatcher
  does this); a handler never `kpanic`s on bad arguments.
- **A-12** Interrupted waits return `-EINTR`. The *sign matters*: a positive
  `EINTR` is read as a valid return value.

## User memory

Every user pointer/length is validated and copied per `kernel-rules.md`
K-40..K-44. Syscall handlers are the trust boundary: nothing past the
handler's argument decode may assume a user value is sane.

## Adding a syscall (checklist)

1. **Decide the table.** FreeBSD defines it → POSIX table at FreeBSD's
   number. Otherwise → native table, next free number, append-only.
2. Add the `args` struct (`sys/include/sys/sysproto_posix.h` or
   `sysproto.h`), fixed-width members, size-asserted.
3. Implement the handler in a subsystem file (not in an `sys/arch/` file),
   machine-independent, with the validation rules above and a Doxygen block
   stating locking context and `@return`.
4. Register it: replace the `SYSCALL_NOTIMP`/invalid slot with
   `{ ARG_COUNT(...), "name", (sys_call_t *)handler, SYSCALL_VALID }` and
   keep the trailing `/* N */` index comment equal to the slot (the
   `syscall-table` gate checks it).
5. Add the userland wrapper (`lib/libc/sys/` or `lib/ubix_api/`) and verify
   the number matches the kernel table.
6. **Verify on both arches** (aarch64 and x86_64): a test in `tests/`
   exercises the call, including the error path and a bad user pointer.
7. Document it in the syscall reference and `CHANGELOG.md [Unreleased]`.

## Stability

Anything reachable from a shipped binary (`/bin`, `/usr/bin`, musl) is a
public contract: changing a struct layout, a slot, or an errno result is an
ABI break and needs a `CHANGELOG.md` entry under "Changed" naming the
break.
