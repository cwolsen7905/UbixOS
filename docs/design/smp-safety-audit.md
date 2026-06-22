# SMP-safety audit — views, daemons, drivers (2026-06-22)

Context: uBixOS was a uniprocessor OS (2002). With two-core SMP now working on
aarch64/x86_64 (per-CPU run queues + the FS/scheduler fixes), the remaining bugs are
all the same shape: code that assumed **one of everything** — one reader, one mailbox,
one device buffer, one CPU — and races or collides once two cores/processes run at
once. This audit enumerates the remaining single-CPU / single-instance assumptions so
we fix them deliberately instead of reactively.

Kernel core services are already SMP-safe (done in prior work): scheduler (per-CPU
queues + release-before-switch), VFS read/write/open + metadata (vfs_io_lock), the
demand-pager/mmap positional reads, MPI (mpiSpinLock), kmalloc, VMM page alloc,
virtio-blk, aarch64 input. The gaps below are what's left.

## A. Single-instance MPI mailbox names (the vdoom class) — userland

Every GUI client hardcodes its reply mailbox name, so `mpi_createMbox()` fails for a
second instance and the app silently can't start (exactly the vdoom bug, now fixed).
Launch any of these twice → the second one fails:

| App | mailbox | file |
|-----|---------|------|
| files | `"files"` | bin/files/main.cc:129 |
| diskutil | `"diskutil"` | bin/diskutil/main.cc:68 |
| activity | `"activity"` | bin/activity/main.cc:125 |
| hello (demo) | `"hello"` | bin/hello/main.cc:62 |
| doom (raw fb) | `"doom"` | bin/doom/doomgeneric_ubix.c:80 |
| vdoom | FIXED → `"vdoom.<pid>"` | (done) |

Singleton SERVERS keep their well-known names (correct): init, views, taskbar, authd,
automountd, ubistry. Fix for the clients: per-instance `"<name>.<pid>"` via snprintf +
getpid(), same one-liner as vdoom. Mechanical, low-risk. (Not strictly an SMP bug — it
fails on UP too — but it's the same single-instance assumption.)

## B. Device drivers with shared state and NO lock (real SMP races)

Pattern to copy: virtio_blk.c wraps submit→poll-complete in g_blk_lock; aarch64
input.c wraps getkbd/getmouse in g_input_lock. Drivers missing equivalent protection
(static-state-decls / lock-uses):

| Driver | state / locks | risk | notes |
|--------|--------------|------|-------|
| aarch64 virtio_sound | 23 / 0 | MED-HIGH | audio; two apps with sound (two DOOMs) hit it concurrently unless the sound server is the sole serial owner — verify ownership, else lock |
| x86_64 ac97 | 19 / 0 | MED-HIGH | same, x86_64 audio |
| aarch64 virtio_gpu | 10 / 0 | LOW | only the compositor (views, single process) touches it → serial by single-owner; confirm no other caller |
| virtio_net (both) | 8 / 0 | MED | TX from a syscall/app context can race lwIP's tcpip_thread; needs a driver TX lock |
| aarch64 uart / fbcon / console | 1-5 / 0 | MED | kprintf from two CPUs interleaves/garbles serial + console; needs an output lock around the kprintf sink |
| x86_64 input | 6 / 0 | MED | mirror the aarch64 g_input_lock fix |

Fix: a driver-wide spinlock around each driver's critical section (submit/complete, or
the register/ring sequence). For console output, one lock around the kprintf sink.

## C. Taskbar renders gray (wrong accent)

bin/views/taskbar/taskbar.cc:108 `apply_theme()`:
```
if (ubistry_get_for_int(user, "views/theme/accent", &accent) != 0)
    <keep slate-gray default 0x333C4C>
```
The bar is gray because that get **fails** and it falls back to the default. The
wallpaper (also per-user) loads fine, so ubistry works generally — so either the
per-user `views/theme/accent` key isn't set, or the query races at startup (taskbar
asks before ubistry has the value, or an MPI timing issue under SMP). Diagnose: does
the accent key exist for the session user? If yes, make the taskbar's startup query
robust (retry/block until ubistry is ready, and re-apply on DISPLAY_THEME).

## Suggested order

1. **Driver locks** (B) — correctness/crash class; copy the virtio_blk pattern. Start
   with audio (two-DOOM scenario) + the console/kprintf output lock (garbled serial
   hampers all further debugging) + x86_64 input.
2. **Single-instance mailboxes** (A) — mechanical pid-unique names; unlocks multi-
   instance for every app.
3. **Taskbar accent** (C) — diagnose the failing get; make startup theme load robust.

Principle going forward: any new driver gets a driver-wide lock from day one; any new
client gets a per-instance mailbox name; any shared kernel structure gets a lock or is
explicitly per-CPU.
