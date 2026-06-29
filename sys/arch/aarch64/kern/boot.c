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
#include <ubixos/cpu_enum.h>       /* cpu_enum_dump (smp-plan Phase 1) */
#include <aarch64/pcpu.h>          /* struct pcpu / g_pcpu / TPIDR_EL1 per-CPU (M0) */
#include <vmm/vmm.h>               /* vmm_mem_map_init */
#include <ubixos/vitals.h>         /* vitals_init */
#include <fs/vfs/vfs.h>            /* vfs_init */
#include <fs/vfs/mount.h>          /* vfs_mount */
#include <fs/ramfs/ramfs.h>        /* ramfs_init, ramfs_populate (initramfs root) */
#include <fs/fat/fat.h>            /* fat_init — disk-backed root */
#include <fs/devfs/devfs.h>        /* devfs_init — /dev/{null,zero,...} for the shell */
#include <fs/vfs/file.h>           /* fopen/fread + vfs_opendir/readdir (ubixfs K2 verify) */
#include <fs/ubixfs/ubfs_vfs.h>    /* ubfs_vfs_init — UbixFS pool driver (plan K2) */
#include <sys/bus.h>               /* struct ubx_device — virtio-blk block device */
#include <mpi/mpi.h>               /* mpi_mbox_exists — wait for the ubistry daemon */
#include <ubixos/sched.h>          /* sched_yield */
#include <ubixos/tty.h>            /* tty_init — pty pool for the GUI terminal */

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

/*
 * smp-plan Phase 3 / M0 — per-CPU blocks.  g_pcpu is BSS (zeroed by start.S), so
 * every CPU's `current`/`idle` start NULL.  aarch64_pcpu_install() points this
 * CPU's TPIDR_EL1 at its slot; thereafter _current (sched.h) resolves to
 * g_pcpu[cpu].current.  Must run before any _current access.
 */
struct pcpu g_pcpu[MAXCPU];

void aarch64_pcpu_install(u_int32_t id, u_int64_t mpidr)
{
	g_pcpu[id].cpuid = id;
	g_pcpu[id].mpidr = mpidr;
	__asm__ __volatile__("msr tpidr_el1, %0" : : "r"(&g_pcpu[id]));
	__asm__ __volatile__("isb");
}

/** Read this CPU's MPIDR_EL1 affinity (low 24 bits). */
static u_int64_t read_mpidr(void)
{
	u_int64_t v;
	__asm__ volatile("mrs %0, mpidr_el1" : "=r"(v));
	return (v & 0xFFFFFFu);
}

/**
 * First C code on aarch64: banner, EL report, exception vectors.  Returns to the
 * park loop in start.S (Phase 12b will instead enable IRQs and idle on `wfi`).
 */
void kmain_aarch64(u_int64_t dtb_phys)
{
	/*
	 * smp-plan Phase 3 / M0: point the BSP's TPIDR_EL1 at g_pcpu[0] before any
	 * _current access (sched.h resolves _current to g_pcpu[cpu].current via
	 * TPIDR_EL1).  g_pcpu is zeroed BSS, so current starts NULL — matching the old
	 * global.  Runs before MMU init; the kernel is identity-mapped, so the slot's
	 * address is stable across the MMU enable.
	 */
	aarch64_pcpu_install(0, read_mpidr());

	kconsole_arch_init(); /* register the PL011 serial sink before the first kprintf */

	kprintf("\nuBixOS aarch64 (QEMU virt) - boot OK\n");
	kprintf("CurrentEL=%lu\n", current_el());

	aarch64_vbar_init();
	kprintf("EL1 exception vectors installed (VBAR_EL1).\n");

	/* The MMU (TTBR0 identity + TTBR1 physmap) was enabled in start.S before this
	 * C bring-up so the kernel can execute from the high half; kmain runs with it
	 * already on (higher-half migration, docs/design/aarch64-higher-half-plan.md). */
	kprintf("MMU active: TTBR0 identity + TTBR1 physmap (39-bit VAs), caches on.\n");
	aarch64_physmap_verify(); /* confirm the TTBR1 physmap aliases the low identity */

	/* Core init order (the embryonic kmain): size RAM from the DTB, then the
	 * physical allocator, then the vitals node (kmalloc'd, so the allocator must
	 * be up first).  aarch64_probe_memory must precede vmm_mem_map_init — it sets
	 * the bitmap ceiling the allocator stages to. */
	aarch64_probe_memory(dtb_phys);
	aarch64_enum_cpus(); /* smp-plan Phase 1: discover CPUs from the DTB /cpus node */
	cpu_enum_dump();
	/*
	 * smp-plan M1: start the secondary cores via PSCI CPU_ON and confirm liveness.
	 * The MMU + page tables (aarch64_mmu_init) and the BSP per-CPU block are up;
	 * each AP enables its MMU on the same tables, installs its TPIDR_EL1, and
	 * bumps a heartbeat the BSP observes here.  IRQs stay masked on the AP — the
	 * per-CPU scheduler is M3.
	 */
#ifndef BOARD_RPI3
	aarch64_smp_start_aps();
#else
	/* Pi M1: BSP only.  Secondary cores need the spin-table release + the BCM
	 * mailbox IPI/per-core timer routing, which are a later Pi milestone. */
	kprintf("smp: BSP only on the Pi (secondary cores are a later milestone)\n");
#endif
	vmm_mem_map_init();
	vitals_init();
	aarch64_rtc_init(); /* boot wall-clock epoch from the PL031 RTC (else clock = 1970) */
	vfs_init();         /* VFS core: filesystem registry + buffer cache */
#ifdef AARCH64_BRINGUP_DEMOS
	ubixfs_selftest(); /* UbixFS core viability over a RAM vdev (plan K1).  Gated:
	                    * the K2/K3 disk mount below now exercises the same core
	                    * end to end, and the self-test's transient 2 MB pool is
	                    * wasteful on every boot (kmalloc never returns pages, and
	                    * aarch64 fork deep-copies — it tips the tight desktop OOM). */
#endif
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
#ifdef BOARD_RPI3
		struct ubx_device *blk = aarch64_sd_init(); /* Pi: the microSD on the EMMC */
#else
		struct ubx_device *blk = aarch64_virtio_blk_init();
#endif
		int root_is_pool = 0;
		int pool_minor;

		fat_init();      /* register the FAT driver with the VFS */
		ubfs_vfs_init(); /* register the UbixFS pool driver (needed before any mount) */

		/* K5/M4 — prefer the UbixFS pool as the root filesystem.  The pool lives on
		 * its own MBR partition (type 0x9C) on the virtio-blk disk; the parser in
		 * virtio_blk registered it as vtblk0sN, and we mount it via the bcache raw
		 * vdev — the same path proven on i386.  On a bad/absent pool the mount
		 * unwinds cleanly (initfs fails -> mountpoint freed), so we fall back to the
		 * FAT partition, then to ramfs below. */
#ifdef BOARD_RPI3
		pool_minor = (blk != 0) ? aarch64_sd_pool_minor() : -1;
#else
		pool_minor = (blk != 0) ? aarch64_virtio_blk_pool_minor() : -1;
#endif
		if (pool_minor > 0)
		{
			ubfs_vfs_set_raw(1);
#ifdef BOARD_RPI3
			/* Pi M4 v1: mount the pool READ-ONLY — the SD driver has no write path
			 * yet (CMD24/25 is a follow-up), and a RW CoW mount may write metadata. */
			if (vfs_mount(1, pool_minor, 0, VFS_TYPE_UBIXFS, "/", "ro") == 0)
#else
			if (vfs_mount(1, pool_minor, 0, VFS_TYPE_UBIXFS, "/", "rw") == 0)
#endif
			{
				fileDescriptor_t *probe = fopen("/lib/libc.so", "r");
				if (probe != 0)
				{
					fclose(probe);
					root_is_pool = 1;
					kprintf("root: UbixFS pool on vtblk0s%i (raw) — mountable root live\n",
					        pool_minor);
				}
				else
				{
					kprintf("ubixfs: pool is / but /lib/libc.so missing — falling through\n");
				}
			}
		}

		/* Disk-backed root: pool if it took, otherwise the FAT partition (vtblk0s1,
		 * minor 1).  FAT now lives in partition 1 (LBA 2048), not the whole disk. */
		if (blk != 0 && (root_is_pool || vfs_mount(1, 1, 0, VFS_TYPE_FAT, "/", "rw") == 0))
		{
			if (!root_is_pool)
				kprintf("root: FAT on vtblk0s1 (fallback)\n");

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

			/* UbixFS loopback pool (plan K2/K3): if a pool image is staged on the
			 * FAT root, mount it read-write at /pool (a file-backed loopback pool —
			 * ubixfs over /pool.img over FAT).  Skipped when the pool itself is the
			 * root (it would be a redundant second ubixfs mount, and /pool.img is not
			 * staged inside the pool).  Read-back proves the VFS dispatch; the
			 * boot.log write proves the write path + txg commit persist. */
			if (!root_is_pool && aarch64_file_exists("/pool.img"))
			{
				ubfs_vfs_set_raw(0); /* loopback, not raw */
				if (vfs_mount(0, 0, 0, VFS_TYPE_UBIXFS, "/pool", "rw") == 0)
				{
					fileDescriptor_t *pf = fopen("/pool/hello.txt", "r");
					kDIR_t *pd;
					char b[96];
					int n;
					if (pf != 0)
					{
						n = (int)fread(b, 1, sizeof(b) - 1, pf);
						if (n > 0)
						{
							b[n] = '\0';
							kprintf("ubixfs: read /pool/hello.txt (%d bytes): %s", n, b);
						}
						fclose(pf);
					}
					pd = vfs_opendir("/pool/etc");
					if (pd != 0)
					{
						struct kdirent e;
						kprintf("ubixfs: readdir /pool/etc:");
						while (vfs_readdir(pd, &e) == 0)
							kprintf(" %s", e.d_name);
						kprintf("\n");
						vfs_closedir(pd);
					}

					/* K3 write + persistence: report a marker left by a prior boot,
					 * then write a fresh one the next boot will read back. */
					pf = fopen("/pool/boot.log", "r");
					if (pf != 0)
					{
						n = (int)fread(b, 1, sizeof(b) - 1, pf);
						if (n > 0)
						{
							b[n] = '\0';
							kprintf("ubixfs: /pool/boot.log from a prior boot: %s", b);
						}
						fclose(pf);
					}
					else
					{
						kprintf("ubixfs: /pool/boot.log absent (first writable boot)\n");
					}
					pf = fopen("/pool/boot.log", "w");
					if (pf != 0)
					{
						const char *msg = "uBixOS booted; UbixFS write path live.\n";
						int w = (int)fwrite((void *)msg, 1, 39, pf);
						fclose(pf);
						kprintf("ubixfs: wrote /pool/boot.log (%d bytes) + committed\n", w);
					}
				}
			}

			aarch64_intc_init();
			timer_init();
			__asm__ volatile("msr daifclr, #2");
			kprintf("IRQs enabled; timer-driven preemption active.\n");

			/* Bring up the virtio-net NIC (polling) + the lwIP stack: tcpip
			 * thread, the virtio netif, an RX poll thread, and DHCP. */
			aarch64_virtio_net_init();
			aarch64_net_init();

			/* Bring up the scanout framebuffer + input devices (for views/objGFX).
			 * The Pi uses the VideoCore mailbox framebuffer (M6); QEMU uses
			 * virtio-gpu + virtio-input/sound. */
#ifdef BOARD_RPI3
			aarch64_bcm_fb_init();
			aarch64_fbcon_init(); /* on-screen kernel console (boot log/panic) */
#else
			aarch64_virtio_gpu_init();
			aarch64_fbcon_init(); /* on-screen kernel console (boot log/panic) */
			aarch64_virtio_input_init();
			aarch64_virtio_sound_init(); /* /dev/audio (virtio-sound PCM playback) */
#endif

			/* Hand off to the SAME userland bootstrap as i386: exec /bin/init
			 * (PID 1), which starts the services from /etc/init.d (automountd,
			 * logd, ubistry, authd, netcfg, …) and then runs the system-console
			 * primary — the views compositor on a desktop image, or /bin/login on
			 * a base image (init picks by what was staged).  The kernel no longer
			 * hand-launches the desktop; both arches converge on one init path.
			 * Never returns. */
			kprintf("\n--- disk-backed userland: exec /bin/init ---\n");
			aarch64_run_dynamic_init("/bin/init");
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
			aarch64_intc_init();
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
