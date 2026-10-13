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
| W-007 | self-host | Self-hosting: rebuild world and kernel inside UbixOS with Clang/LLVM + lld | doing | - | - | Plan: docs/design/self-hosting-plan.md. /usr/src/bin builds in-OS on aarch64 (#10). aarch64: the kernel builds in-OS (bmake -j4, 4 vCPUs) and the self-built kernel boots and passes sigtest/vfstest (with W-033, W-035). x86_64 too: the kernel builds in-OS (clang/lld, #18; thread fix W-040) and the self-built kernel boots and passes sigtest/vfstest |
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
| W-029 | kernel | Move the six calls the kernel serves at Linux numbers (poll 168, rt_sigsuspend 179, tkill 238, gettid 224, set_tid_address 258, statx 383) to FreeBSD or native slots | todo | - | - | Keep the old slots answering while statically linked binaries (clang/lld) still use them. From the W-028 audit. The in-OS clang (static, old musl) also calls 242, logged as ffclock_setestimate (Linux i386 sched_getaffinity) |
| W-030 | kernel | Kernel size_t is typedef int (32-bit, signed) on the 64-bit kernels | done | - | #25 | sys/include/sys/types.h. Shared structs using size_t have the wrong layout; lengths truncate above 2 GB. Found via sigaltstack (W-028) |
| W-031 | vmm | x86_64 MI code used the i386 vmm_layout.h (user space ended at 3 GB) | done | - | #16 | <machine/vmm_layout.h> now resolves to x86_64/vmm_layout.h |
| W-032 | self-host | /usr/src/lib builds inside UbixOS (C libraries) | doing | - | #16 | Makefile fixes in W-028 PR (_ARCH default, -L${MUSL_LIB}). aarch64: every C library links except libbearssl and libcss (blocked by W-033); C++ (objgfx) waits for clang++/libc++ in-OS |
| W-033 | kernel | execve silently truncated arguments: each argv/env string cut at 4096 bytes, entries past 256 dropped | done | - | #17 | Both arches now copy argv/envp in whole (exec_args in sys/kern/kern_exec.c) up to EXEC_ARG_MAX (128 KB, = ARG_MAX) and fail E2BIG/EFAULT beyond it. bmake's 15 KB `sh -c` link lines for libbearssl/libcss lost their -o, so ld wrote a.out and exited 0 |
| W-034 | ubixfs | New files on aarch64 show an mtime from boot time, not the current time | todo | - | - | Seen during the W-032 build (ls -l showed 20:24 for files written at 23:26); not investigated |
| W-035 | self-host | Parallel kernel builds: one bmake target per kernel object so -j works | done | - | #18 | x86_64 kernel recipe also moved off recipe-time find and the obj/sys/*.o link glob (both break under the in-OS shell) |
| W-036 | self-host | Block I/O: 4 MB hashed buffer cache, multi-sector UbixFS block I/O, batched virtio-blk on both arches | done | - | #19 | aarch64 in-OS clean kernel build -j4: ~32 min -> 9.6 min |
| W-037 | kernel | poll/select ignored pipes and signals, so bmake -j waited its 5 s poll timeout about twice per job | done | - | #19 | 20 trivial jobs: -j1 204 s -> 2 s, -j4 52 s -> 3 s |
| W-038 | kernel | setitimer/getitimer were stubs (no SIGALRM ever fired) | done | - | #23 | ITIMER_REAL fires SIGALRM via a per-task callout (tick resolution, interval reload, cancelled at exit); VIRTUAL/PROF stored but do not fire (no CPU-time accounting) |
| W-039 | vfs | Global VFS lock + uncached path lookups serialise parallel compiles (open/readlink/close dominate) | doing | - | #24 | Plan: docs/design/vfs-lookup-perf-plan.md. Step 1 (verified-block cache in UbixFS) done: in-OS clean kernel build -j4 9.1 min -> 74 s. Left: steps 2-6 (block-wise directory scan, fclose off fdTable_lock, one lookup per stat/access/readlink, name cache, reader/writer VFS lock); low priority now |
| W-040 | kernel | x86_64 threads each kept a private mmap/brk cursor, so threads mapped over each other (ld.lld crashed in parallelFor) | done | - | #20 | Shared cursor on the thread-group leader under a lock, as aarch64 |
| W-041 | kernel | Fatal signals and exit() end only the calling thread; x86_64 faults never deliver SIGSEGV; threads had tid 0 and no CLONE_CHILD_CLEARTID | done | - | #22 | Group kill on exit_group / fatal signal (both arches), SIGKILL unblockable, faults via signal_fault_deliver, rfork sets ptid + ctid, set_tid_address records its pointer |
| W-042 | kernel | Kernel size_t/ssize_t were 32-bit on the 64-bit kernels (i386 leftover); clang-tidy findings from #17-#20 failed CI analyze | done | - | #25 | size_t = unsigned long, ssize_t = long (LP64); type-table headers marked clang-format off; poll builds revents in an int; explicit casts for multilevel pointers and widening |
| W-043 | self-host | musl's Makefile needs GNU make: give musl a bmake build so /usr/src/lib builds in-OS with the BSD toolchain | todo | - | - | Blocks W-032 and a full in-OS world (W-007); both arches |
| W-044 | kernel | An idle aarch64 VM uses ~270% host CPU | todo | - | - | Idle loop likely spins instead of WFI on each vCPU; check x86_64 HLT path too |
| W-045 | self-host | Bare in-OS clang cannot link user programs: crtbeginS.o/crtendS.o missing from the image | todo | - | - | Image staging (mkimage); the cc wrapper in share/bin works around it |
| W-046 | cleanup | Old BeFS sys/fs/ubixfs/dir_cache.c is in the tree but not built | todo | - | - | Delete or document; UbixFS v2 lives in lib/ubixfs_core |
| W-047 | kernel | SMP: wait4 unlinked the reaped child from taskList (and pushed delList) without the scheduler lock; a concurrent fork/reap lost list updates, a forked child fell off taskList, its exit was never reaped and the parent shell waited forever (in-OS builds hung after a few hundred commands, both arches) | doing | - | - | wait_find_child / find_and_reap_child now run under schedulerSpinLock; x86_64 wait4 re-checks ECHILD every pass; sched_stop no longer enqueues a RUNNING parent; the reaper no longer reaps the exiting task from its own final sched() (it posted SIGCHLD while the child was still on-CPU, wait4 refused it, and the shell slept on a consumed SIGCHLD) |
| W-048 | kernel | x86_64 serial console input is polled (2-tick nap, no RX interrupt): a burst longer than the UART FIFO drops bytes | todo | - | - | Seen when driving the VM over the serial socket; aarch64 PL011 path to check too |

## Notes

Seeded 2026-10-11 from TODO.md, BUGS.md, docs/design/backlog-roadmap.md, the
design plans and merged PRs #5 to #10. The `done` rows are a starting point,
not a complete history; older done items live in CHANGELOG.md and the plans'
done lists. Rows marked `status unclear` were not re-verified against the code.
