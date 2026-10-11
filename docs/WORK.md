# Work Matrix

Committed status of uBixOS work. Format and rules: [standards/work-matrix.md](standards/work-matrix.md).
Every lane updates its row in the same PR; rows are never deleted.

## Work

| ID | Area | Item | Status | Lane | PR | Notes |
|----|------|------|--------|------|----|-------|
| W-001 | kernel | Build /usr/src/bin inside UbixOS: scheduler, pipe, readdir, UbixFS and musl fixes (aarch64 + x86_64) | done | - | #10 | aarch64 runtime-tested with full bmake -j2 build; x86_64 only sigtest/vfstest |
| W-002 | kernel | Kernel fixes for self-hosting: signals, exit cleanup, pipes, VFS, #! scripts | done | - | #5 | Merged 2026-10-10 |
| W-003 | userland | Add clear, paste, ps, test/[; fix busybox getopt32 stub | done | - | #6 | Merged 2026-10-11 |
| W-004 | standards | Engineering standards, C-world gate suite (tools/check) and CI rewrite | done | - | #7 | Merged 2026-10-11 |
| W-005 | identity | Add identity direction document (proposal) | done | - | #8 | docs/design/identity.md; the ideas themselves are separate rows |
| W-006 | docs | Bring architecture docs and DEBUG.md up to date for 64-bit master | done | - | #9 | ARCHITECTURE.md itself still needs its 64-bit rewrite (separate row) |
| W-007 | self-host | Self-hosting: rebuild world and kernel inside UbixOS with Clang/LLVM + lld | doing | - | - | Plan: docs/design/self-hosting-plan.md. /usr/src/bin builds in-OS on aarch64 (#10); kernel, other trees and x86_64 not yet |
| W-008 | self-host | x86_64 /usr/src/bin build inside UbixOS | todo | - | - | Follow-up from #10; slow under TCG |
| W-009 | kernel | errno translation at the POSIX boundary (FreeBSD errno numbers vs musl Linux values: EAGAIN, ELOOP, ENAMETOOLONG, ...) | todo | - | - | Follow-up from #10 |
| W-010 | userland | Add tee to the world | todo | - | - | Follow-up from #10 |
| W-011 | kernel | getppid (POSIX slot 39) is wired to sys_getpid; wire it to sys_getppid | done | - | #16 | Also: reparenting an orphan now updates its ppid |
| W-012 | kernel | Fix syscall table name typos: wiat4, cgdur, feek | todo | - | - | sys/posix/syscalls_posix.c slots 7, 12, 294; names only |
| W-013 | docs | Create SYSCALLS.md or remove the references to it | todo | - | - | Referenced by README.md, CONTRIBUTING.md; docs/README.md notes the file is absent. Which of create/remove is a decision |
| W-014 | vmm | Fix stale comment in vmm_layout.h and plan the aarch64 higher-half kernel | todo | - | - | sys/include/aarch64/vmm_layout.h says TTBR1 = kernel but the kernel runs in TTBR0 today; plan: docs/design/aarch64-higher-half-plan.md |
| W-015 | tooling | Update .vscode/launch.json (i386-only) for aarch64 and x86_64 | todo | - | - |  |
| W-016 | docs | Rewrite ARCHITECTURE.md for 64-bit master | todo | - | - | Not covered by #9; status unclear |
| W-017 | docs | Fix docs/design/ubistry-plan.md, which still says bin/ubistry | todo | - | - | Daemons live under usr.sbin/; verify actual path first |
| W-018 | ci | First green CI run for the build, analyze and boot-smoke jobs | todo | - | - | Jobs are continue-on-error until green; never run on a stock Linux runner (see #7) |
| W-019 | ci | Run cppcheck and shellcheck gates and baseline findings | todo | - | - | Not yet run (#7: cppcheck not installed locally) |
| W-020 | identity | Discussion: filesystem with indexed attributes and live queries (identity idea 1) | todo | - | - | discussion; docs/design/identity.md section 1; needs its own design plan |
| W-021 | identity | Discussion: message-oriented system, typed service protocols over MPI (identity idea 2) | todo | - | - | discussion; docs/design/identity.md section 2; needs its own design plan |
| W-022 | identity | Discussion: ubistry as the single structured configuration store (identity idea 3) | todo | - | - | discussion; docs/design/identity.md section 3; layering already implemented, scope of the idea is open |
| W-023 | identity | Discussion: one tree, many profiles, self-hosting, atomic and rollbackable updates (identity idea 4) | todo | - | - | discussion; docs/design/identity.md section 4 |
| W-024 | identity | Discussion: native apps get more, POSIX ports still just work (identity idea 5) | todo | - | - | discussion; docs/design/identity.md section 5 |
| W-025 | vmm | aarch64 exec leaks old user pages (no pmap_free_user_space) | todo | - | - | From docs/design/backlog-roadmap.md; status unclear, not re-verified |
| W-026 | console | fbcon on-screen text console and image profiles (convergence plan phases 3.5 and 4) | todo | - | - | docs/design/console-and-arch-convergence-plan.md, fbcon.md; status unclear |
| W-027 | kernel | Legacy i386-era backlog in TODO.md and BUGS.md (VFS, VMM, ld.so, sched, MPI) | todo | - | - | Many items name i386 paths that may no longer exist on master; triage into individual rows before working; status unclear |
| W-028 | kernel | Syscall ABI audit fixes: musl Linux numbers leaking into the FreeBSD table (pwrite was setgid, statfs wrote to 0x78, ...), sigaltstack, pwrite, posix_fadvise, copyin/copyout | done | - | #16 | 22 dangerous leaks closed; 71 Linux-only calls fail ENOSYS in musl without trapping. sigtest/vfstest pass on x86_64 and aarch64 |
| W-029 | kernel | Move the six calls the kernel serves at Linux numbers (poll 168, rt_sigsuspend 179, tkill 238, gettid 224, set_tid_address 258, statx 383) to FreeBSD or native slots | todo | - | - | Keep the old slots answering while statically linked binaries (clang/lld) still use them. From the W-028 audit |
| W-030 | kernel | Kernel size_t is typedef int (32-bit, signed) on the 64-bit kernels | todo | - | - | sys/include/sys/types.h. Shared structs using size_t have the wrong layout; lengths truncate above 2 GB. Found via sigaltstack (W-028) |
| W-031 | vmm | x86_64 MI code used the i386 vmm_layout.h (user space ended at 3 GB) | done | - | #16 | <machine/vmm_layout.h> now resolves to x86_64/vmm_layout.h |
| W-032 | self-host | /usr/src/lib builds inside UbixOS (C libraries) | doing | - | #16 | Makefile fixes in W-028 PR (_ARCH default, -L${MUSL_LIB}). aarch64: every C library links except libbearssl and libcss (blocked by W-033); C++ (objgfx) waits for clang++/libc++ in-OS |
| W-033 | kernel | execve silently truncates arguments: each argv/env string is cut at 4096 bytes and entries past 256 are dropped | todo | - | - | Both arches, copy_user_strvec in sys/arch/*/kern/execfile.c. bmake's `sh -c` link line for libbearssl/libcss (~15 KB) loses its -o, so ld writes a.out and exits 0. Should fail E2BIG past the arg region, not truncate |
| W-034 | ubixfs | New files on aarch64 show an mtime from boot time, not the current time | todo | - | - | Seen during the W-032 build (ls -l showed 20:24 for files written at 23:26); not investigated |

## Notes

Seeded 2026-10-11 from TODO.md, BUGS.md, docs/design/backlog-roadmap.md, the
design plans and merged PRs #5 to #10. The `done` rows are a starting point,
not a complete history; older done items live in CHANGELOG.md and the plans'
done lists. Rows marked `status unclear` were not re-verified against the code.
