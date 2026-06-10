/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 bring-up C entry (QEMU `virt`).
 *
 * Phase 11: print a banner over the PL011.  Phase 12a: install the EL1
 * exception vectors so faults are visible.  GIC + generic timer (interrupts)
 * are Phase 12b; MMU is Phase 13.
 */

#include "bringup.h"
#include <ubixos/sched.h>          /* sched_init, RUNNING, QOS_DEFAULT */
#include <ubixos/sched_internal.h> /* taskList, set_current (scheduler bootstrap) */
#include <vmm/vmm.h>        /* vmm_mem_map_init */
#include <ubixos/vitals.h>  /* vitals_init */
#include <fs/vfs/vfs.h>     /* vfs_init */
#include <fs/vfs/mount.h>   /* vfs_mount */
#include <fs/ramfs/ramfs.h> /* ramfs_init, ramfs_populate (initramfs root) */
#include <fs/fat/fat.h>     /* fat_init — disk-backed root */
#include <fs/devfs/devfs.h> /* devfs_init — /dev/{null,zero,...} for the shell */
#include <sys/bus.h>        /* struct ubx_device — virtio-blk block device */
#include <mpi/mpi.h>        /* mpi_mbox_exists — wait for the ubistry daemon */
#include <ubixos/sched.h>   /* sched_yield */
#include <ubixos/tty.h>     /* tty_init — pty pool for the GUI terminal */

/* The static boot triad — init forks login, login execs sh — laid into the
 * ramfs root as /bin/{init,login,sh}.  Stands in for the real (dynamically
 * linked) init/login/shell until the dynamic linker lands. */
extern char _binary_init_elf_start[];
extern char _binary_init_elf_end[];
extern char _binary_login_elf_start[];
extern char _binary_login_elf_end[];
extern char _binary_sh_elf_start[];
extern char _binary_sh_elf_end[];
extern char _binary_spin_elf_start[];
extern char _binary_spin_elf_end[];
extern char _binary_mpitest_elf_start[];
extern char _binary_mpitest_elf_end[];
extern char _binary_pipetest_elf_start[];
extern char _binary_pipetest_elf_end[];
extern char _binary_faulttest_elf_start[];
extern char _binary_faulttest_elf_end[];
extern char _binary_dirtest_elf_start[];
extern char _binary_dirtest_elf_end[];
extern char _binary_authd_min_elf_start[];
extern char _binary_authd_min_elf_end[];

/* The freestanding demo program, laid into /bin/hello so the shell has a real
 * command to fork/execve. */
extern char _binary_hello_elf_start[];
extern char _binary_hello_elf_end[];

/* Dynamic-linker bring-up test: a PIE program (/bin/hello_dyn) + the musl
 * dynamic linker (= libc.so, laid into /lib/ld-musl-aarch64.so.1). */
extern char _binary_hello_dyn_elf_start[];
extern char _binary_hello_dyn_elf_end[];
extern char _binary_ld_musl_aarch64_so_1_start[];
extern char _binary_ld_musl_aarch64_so_1_end[];

/* A real world binary (busybox cat, PIE) — proves the relinked dynamic world
 * runs via the kernel's dynamic loader, not just a hand-built test. */
extern char _binary_worldcat_start[];
extern char _binary_worldcat_end[];

/**
 * Lay an embedded ELF blob into the ramfs root at @path.
 *
 * @return 0 on success, non-zero on failure (logged).
 */
static int install_bin(const char *path, const char *start, const char *end)
{
	u_int32_t len = (u_int32_t)(end - start);
	if (ramfs_populate("/", path, start, len) != 0)
	{
		kprintf("bootstrap: failed to populate %s\n", path);
		return (-1);
	}
	return (0);
}

/**
 * Return the current exception level (0-3) from CurrentEL[3:2].
 */
static u_int64_t current_el(void)
{
	u_int64_t v;
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
	return (v >> 2) & 0x3;
}

/**
 * First C code on aarch64: banner, EL report, exception vectors.  Returns to the
 * park loop in start.S (Phase 12b will instead enable IRQs and idle on `wfi`).
 */
void kmain_aarch64(void)
{
	kconsole_arch_init(); /* register the PL011 serial sink before the first kprintf */

	kprintf("\nuBixOS aarch64 (QEMU virt) - boot OK\n");
	kprintf("CurrentEL=%lu\n", current_el());

	aarch64_vbar_init();
	kprintf("EL1 exception vectors installed (VBAR_EL1).\n");

	aarch64_mmu_init();
	kprintf("MMU enabled: TTBR0 identity map (39-bit VA), caches on.\n");

	/* Core init order (the embryonic kmain): physical allocator, then the
	 * vitals node (kmalloc'd, so the allocator must be up first). */
	vmm_mem_map_init();
	vitals_init();
	vfs_init();             /* VFS core: filesystem registry + buffer cache */
	ubixfs_selftest();      /* UbixFS lite-ZFS core viability check (RAM vdev; plan K1) */
	aarch64_console_init(); /* PL011 -> VFS console fileops (stdin/stdout/stderr) */
	tty_init();             /* pty pool + VT100 engine (g_tty_ops) for the GUI terminal */

	/* Bootstrap the generic scheduler: initialise the task list and adopt the
	 * boot context as the running task.  This is load-bearing for everything that
	 * schedules a task (the desktop chain, dirtest, the demos) — it must run
	 * regardless of whether the bring-up demos below are built in. */
	sched_init();
	set_current(taskList);
	taskList->state = RUNNING;
	taskList->priority = QOS_DEFAULT; /* share the CPU at the default QoS */
	taskList->base_priority = QOS_DEFAULT;

#ifdef AARCH64_BRINGUP_DEMOS
	aarch64_vmm_demo();
	aarch64_pmap_demo();
	aarch64_aspace_demo();
	aarch64_syscall_demo();
	aarch64_elf_demo();
	aarch64_ctx_demo();
	aarch64_sched_demo();
	aarch64_proc_demo();
	aarch64_fork_demo();
	aarch64_user_elf_demo();
#endif
	aarch64_procfs_demo(); /* mount /proc before the program (which reads it) */
	aarch64_ramfs_demo();  /* registers ramfs + exercises it at /ram */

	/* --- disk-backed root: if a virtio-blk device with a FAT filesystem is
	 * present, mount it at "/" and run a real program *off the disk* (interp +
	 * libc + the binary all read via virtio-blk -> bcache -> FAT).  This is the
	 * disk-backed dynamic world; the embedded ramfs path below is the fallback
	 * when no disk is attached. */
	{
		struct ubx_device *blk = aarch64_virtio_blk_init();

		fat_init(); /* register the FAT driver with the VFS */
		if (blk != 0 && vfs_mount(0, 0, 0, VFS_TYPE_FAT, "/", "rw") == 0)
		{
			kprintf("root: FAT on virtio-blk vtblk0\n");

			/* Phase-3 HVF-bisect harness: read the real FAT /bin (opendir/
			 * getdents + stat — the pointer-arg POSIX calls `ls` uses) before
			 * the desktop launches, so a syscall pruned onto the shared table can
			 * be observed here headlessly. */
			kprintf("\n--- dir-syscall self-test (FAT /bin) ---\n");
			aarch64_run_elf_image(_binary_dirtest_elf_start, "dirtest");
			kprintf("--- end dir-syscall self-test ---\n");

			/* Mount devfs at /dev so the shell can open /dev/null, /dev/tty,
			 * etc.  devfs_init() registers the FS + queues the pseudo-devices;
			 * the mount replays them into /dev. */
			devfs_init();
			if (vfs_mount(0, 0, 0, VFS_TYPE_DEVFS, "/dev", "rw") == 0)
				kprintf("dev: devfs mounted at /dev\n");

			gic_init();
			timer_init();
			__asm__ volatile("msr daifclr, #2");
			kprintf("IRQs enabled; timer-driven preemption active.\n");

			/* Bring up the virtio-net NIC (polling) + the lwIP stack: tcpip
			 * thread, the virtio netif, an RX poll thread, and DHCP. */
			aarch64_virtio_net_init();
			aarch64_net_init();

			/* Bring up the virtio-gpu scanout framebuffer + input devices
			 * (for views/objGFX). */
			aarch64_virtio_gpu_init();
			aarch64_fbcon_init();        /* on-screen kernel console (boot log/panic) */
			aarch64_virtio_input_init();
			aarch64_virtio_sound_init(); /* /dev/audio (virtio-sound PCM playback) */

			/* Image profile selector: the desktop profile stages /bin/views, the
			 * base (headless/IoT/safe-mode) profile does not.  Branch on what is
			 * present — same kernel, the mkimage profile decides what runs. */
			if (aarch64_file_exists("/bin/views"))
			{
				/* DESKTOP profile: start authd (the "authd" MPI mailbox for
				 * credential checks), then run the views compositor off disk — it
				 * owns the virtio-gpu framebuffer (sys_mapfb), composites the
				 * desktop, and forks /bin/vlogin for the graphical login. */
				kprintf("\n--- disk-backed desktop ---\n");

				/* Start the ubistry registry daemon and wait (bounded) for its
				 * mailbox before launching the desktop, so views/vlogin read real
				 * settings (wallpaper/theme/per-user prefs) instead of falling back
				 * to defaults.  ubistry creates its mailbox only after loading
				 * /var/db/ubistry.db, so the mailbox existing = fully ready. */
				aarch64_spawn_dynamic("/bin/ubistry");
				{
					int tries = 100000;
					while (!mpi_mbox_exists("ubistry") && --tries > 0)
						sched_yield();
					if (tries == 0)
						kprintf("desktop: ubistry not ready — using defaults\n");
				}

				aarch64_spawn_dynamic("/bin/authd");    /* real authd (PBKDF2/BearSSL) from disk */
				aarch64_run_dynamic_init("/bin/views"); /* compositor -> forks vlogin; never returns */
			}
			else
			{
				/* BASE profile: no compositor.  Authenticate against authd and run
				 * the text-console login on the kernel console (fbcon on screen +
				 * PL011 serial); login forks the shell.  Never returns. */
				kprintf("\n--- disk-backed base console ---\n");
				aarch64_spawn_dynamic("/bin/authd"); /* login authenticates via the authd mailbox */
				aarch64_run_dynamic_init("/bin/login");
			}
		}
	}

	/* --- initramfs bootstrap: the shape of a real boot ---------------------
	 * Mount a ramfs root, lay /bin/init into it (the embedded program stands in
	 * for init until the dynamic linker + the real init/login/shell land), then
	 * exec it *from the filesystem path* — what the generic kmain will do once
	 * unified.  ramfs is already registered by the demo above. */
	/* --- userland bootstrap: the real boot chain ---------------------------
	 * Mount a ramfs root, lay the static boot triad + a demo command into it,
	 * then run /bin/init — which forks /bin/login, login execs /bin/sh.  This is
	 * the terminal boot action: aarch64_run_init() schedules init and turns this
	 * (boot) thread into the idle loop, so it never returns.  EL0 tasks run with
	 * IRQs enabled (SPSR.I clear), so the 100 Hz timer preempts them — CPU-bound
	 * EL0 code time-slices; blocking syscalls (console read, wait4) yield/sleep
	 * cooperatively.  wait4 blocks via sched_sleep(WAIT) so the timer-driven
	 * reaper wakes it correctly (no run-queue double-enqueue). */
	kprintf("\n--- uBixOS aarch64 userland bootstrap ---\n");
	if (vfs_mount(0, 0, 0, VFS_TYPE_RAMFS, "/", "rw") == 0)
	{
		int ok = install_bin("/bin/init", _binary_init_elf_start, _binary_init_elf_end) == 0 &&
		         install_bin("/bin/login", _binary_login_elf_start, _binary_login_elf_end) == 0 &&
		         install_bin("/bin/sh", _binary_sh_elf_start, _binary_sh_elf_end) == 0 &&
		         install_bin("/bin/spin", _binary_spin_elf_start, _binary_spin_elf_end) == 0 &&
		         install_bin("/bin/mpitest", _binary_mpitest_elf_start, _binary_mpitest_elf_end) == 0 &&
		         install_bin("/bin/pipetest", _binary_pipetest_elf_start, _binary_pipetest_elf_end) == 0 &&
		         install_bin("/bin/faulttest", _binary_faulttest_elf_start, _binary_faulttest_elf_end) == 0 &&
		         install_bin("/bin/dirtest", _binary_dirtest_elf_start, _binary_dirtest_elf_end) == 0 &&
		         install_bin("/bin/hello", _binary_hello_elf_start, _binary_hello_elf_end) == 0;
		if (ok)
		{
			gic_init();
			timer_init();
			__asm__ volatile("msr daifclr, #2"); /* unmask IRQ — timer drives preemption */
			kprintf("IRQs enabled; timer-driven preemption active.\n");

			/* MPI self-test: prove the native message-passing syscalls work.
			 * mpitest is a static ET_EXEC, so run it via the minimal-stack path
			 * (its phdrs are not in a PT_LOAD, so the full auxv's AT_PHDR is N/A). */
			kprintf("\n--- MPI self-test ---\n");
			aarch64_run_elf_image(_binary_mpitest_elf_start, "mpitest");
			kprintf("--- end MPI self-test ---\n");

			/* pipe(2) self-test: pipe + fork round-trip (the taskbar app
			 * launcher depends on pipe).  Static ET_EXEC like mpitest. */
			kprintf("\n--- pipe self-test ---\n");
			aarch64_run_elf_image(_binary_pipetest_elf_start, "pipetest");
			kprintf("--- end pipe self-test ---\n");

			/* Fault-containment self-test: faulttest dereferences a bad
			 * pointer; the kernel must terminate just that process and keep
			 * running.  If this line's "end" marker prints, the OS survived a
			 * userland-triggered fault (the robustness backstop works). */
			kprintf("\n--- fault-containment self-test ---\n");
			aarch64_run_elf_image(_binary_faulttest_elf_start, "faulttest");
			kprintf("--- end fault-containment self-test (OS survived) ---\n");

			/* Dynamic-linker bring-up test: lay the musl dynamic linker (= libc.so)
			 * at its INTERP path + a PIE program, then run it through the kernel's
			 * dynamic loader (interp + auxv).  Proves the path the real dynamic
			 * world needs before the static triad takes over. */
			if (install_bin("/lib/ld-musl-aarch64.so.1",
			                _binary_ld_musl_aarch64_so_1_start,
			                _binary_ld_musl_aarch64_so_1_end) == 0 &&
			    install_bin("/lib/libc.so",
			                _binary_ld_musl_aarch64_so_1_start,
			                _binary_ld_musl_aarch64_so_1_end) == 0 &&
			    install_bin("/bin/hello_dyn", _binary_hello_dyn_elf_start, _binary_hello_dyn_elf_end) == 0)
			{
				kprintf("\n--- dynamic-linker test ---\n");
				aarch64_run_dynamic("/bin/hello_dyn");

				/* And a *real* relinked-PIE world binary (busybox cat): proves the
				 * world build's dynamic binaries run, not just a hand-built test. */
				if (install_bin("/bin/cat", _binary_worldcat_start, _binary_worldcat_end) == 0)
					aarch64_run_dynamic("/bin/cat");
				kprintf("--- end dynamic-linker test ---\n\n");
			}

			aarch64_run_init("/bin/init"); /* never returns */
		}
	}
	else
	{
		kprintf("bootstrap: failed to mount ramfs root\n");
	}

	kprintf("bootstrap failed; parking.\n");
	for (;;)
		__asm__ volatile("wfi");
}
