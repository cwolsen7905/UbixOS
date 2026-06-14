/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions, the following disclaimer and the list of authors. 2)
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions, the following disclaimer and the list of authors in
 * the documentation and/or other materials provided with the distribution. 3)
 * Neither the name of the UbixOS Project nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ubixos/version.h>
#include <ubixos/init.h>
#include <ubixos/multiboot.h>
#include <sys/gdt.h>
#include <sys/video.h>
#include <sys/tss.h>
#include <sys/bootinfo.h>
#include <ubixos/exec.h>
#include <vmm/pageout.h>
#include <ubixos/kpanic.h>
#include <ubixos/random.h>
#include <ubixos/systemtask.h>
#include <fs/vfs/mount.h>
#include <fs/vfs/vfs.h>         /* VFS_TYPE_UBIXFS */
#include <fs/vfs/file.h>        /* fopen/fread/fwrite + vfs_opendir/readdir */
#include <fs/ubixfs/ubfs_vfs.h> /* ubfs_vfs_init — UbixFS pool driver (plan K4) */
#include <lib/kprintf.h>
#include <lib/kconsole.h>
#include <lib/kmalloc.h>
#include <i386/pcpu.h>
#include <ubixos/sched.h>

#define B_ADAPTORSHIFT 24
#define B_ADAPTORMASK 0x0f
#define B_ADAPTOR(val) (((val) >> B_ADAPTORSHIFT) & B_ADAPTORMASK)
#define B_CONTROLLERSHIFT 20
#define B_CONTROLLERMASK 0xf
#define B_CONTROLLER(val) (((val) >> B_CONTROLLERSHIFT) & B_CONTROLLERMASK)
/*
 * Constants for converting boot-style device number to type,
 * adaptor (uba, mba, etc), unit number and partition number.
 * Type (== major device number) is in the low byte
 * for backward compatibility.  Except for that of the "magic
 * number", each mask applies to the shifted value.
 * Format:
 *       (4)   (8)   (4)  (8)     (8)
 *      --------------------------------
 *      |MA | SLICE | UN| PART  | TYPE |
 *      --------------------------------
 */
#define B_SLICESHIFT 20
#define B_SLICEMASK 0xff
#define B_SLICE(val) (((val) >> B_SLICESHIFT) & B_SLICEMASK)
#define B_UNITSHIFT 16
#define B_UNITMASK 0xf
#define B_UNIT(val) (((val) >> B_UNITSHIFT) & B_UNITMASK)
#define B_PARTITIONSHIFT 8
#define B_PARTITIONMASK 0xff
#define B_PARTITION(val) (((val) >> B_PARTITIONSHIFT) & B_PARTITIONMASK)
#define B_TYPESHIFT 0
#define B_TYPEMASK 0xff
#define B_TYPE(val) (((val) >> B_TYPESHIFT) & B_TYPEMASK)

/*****************************************************************************************
 Desc: The Kernels Descriptor table:
 0 - 0x00 - Dummy Entry
 1 - 0x08 - Ring 0 CS
 2 - 0x10 - Ring 0 DS
 3 - 0x18 - LDT
 4 - 0x20 - Scheduler TSS
 5 - 0x28 - Ring 3 CS
 6 - 0x30 - Ring 3 DS
 7 - 0x38 - GPF TSS
 8 - 0x40 - Stack Fault TSS
 9 - 0x48 - SMP Private Data
 10 - 0x50 - USER %GS (Stack!)
 11 - 0x58 - SMP per-CPU data (%gs base = &g_pcpu[cpuid]); see pcpu_install_gs

 Notes:

 MrOlsen: test

 *****************************************************************************************/
ubixDescriptorTable(ubixGDT, 12){
    {.dummy = 0},
    ubixStandardDescriptor(0x0000, 0xFFFFF, (dCode + dRead + dBig + dBiglim)),
    ubixStandardDescriptor(0x0000, 0xFFFFF, (dData + dWrite + dBig + dBiglim)),
    ubixStandardDescriptor(VMM_USER_LDT, 0xFFFFF, (dLdt)),
    ubixStandardDescriptor(0x4200, (sizeof(struct tssStruct)), (dTss + dDpl3)),
    ubixStandardDescriptor(0x0000, 0xFFFFF, (dCode + dRead + dBig + dBiglim + dDpl3)),
    ubixStandardDescriptor(0x0000, 0xFFFFF, (dData + dWrite + dBig + dBiglim + dDpl3)),
    ubixStandardDescriptor(0x5200, (sizeof(struct tssStruct)), (dTss + dDpl3)),
    ubixStandardDescriptor(0x6200, (sizeof(struct tssStruct)), (dTss)),
    ubixStandardDescriptor(0x0000, 0xFFFFF, (dData + dWrite + dBig + dBiglim + dDpl0)),
    ubixStandardDescriptor(0xBFC00000, 0xFFFFF, (dData + dWrite + dBig + dBiglim + dDpl3)),
    /*
     * Index 11 (selector SEL_PCPU = 0x58): per-CPU data segment for SMP.  Base
     * is patched at runtime to &g_pcpu[cpuid] by pcpu_install_gs() (the macro
     * cannot take a pointer as a constant), so %gs:offset reaches this CPU's
     * struct pcpu — e.g. %gs:8 is the running task (_current).  Placeholder
     * base 0 here; on the BSP it is set in smpInit().
     */
    ubixStandardDescriptor(0x0000, 0xFFFFF, (dData + dWrite + dBig + dBiglim + dDpl0)),
};

struct
{
	unsigned short limit __attribute__((packed));
	union descriptorTableUnion *gdt __attribute__((packed));
} loadGDT = {(12 * sizeof(union descriptorTableUnion) - 1), ubixGDT};

static char *argv_init[2] = {
    "init",
    0x0,
}; /* ARGV For Initial Process */

static char *envp_init[6] = {
    "HOME=/",
    "PWD=/",
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
    "USER=root",
    "GROUP=admin",
    0x0,
}; /* ENVP For Initial Process */

struct bootinfo _bootinfo;
char _kernelname[512];
u_long _bootdev;
u_long _boothowto;

/**
 * Idle thread entry — the lowest-priority task, dispatched only when nothing
 * else is runnable.  Halts the CPU until the next interrupt (sti before hlt so
 * an IRQ can wake it), then loops.  Never blocks, so it is always runnable.
 */
static void idle_task(void)
{
	for (;;)
		__asm__ __volatile__("sti; hlt");
}

/**
 * \brief This is the entry point into the os where all of the kernels sub
 * systems are started up.
 *
 * \param rootdev address of root device structure
 */
int kmain(u_int32_t rootdev)
{
	/* Set up counter for startup routine */
	int i = 0x0;

	/*
	 * Point the BSP's %gs at g_pcpu[0] before anything touches _current.
	 * _current is now per-CPU state at %gs:8 (g_pcpu[0].current); this patches
	 * the SEL_PCPU GDT descriptor's base to &g_pcpu[0] and loads %gs = SEL_PCPU.
	 * Must be the first thing in kmain — every later kernel path reads _current.
	 */
	/* smp-plan Phase 3: give the BSP its OWN per-CPU GDT + TSS (not the shared
	 * ubixGDT / boot TSS at 0x4200), so each CPU's %gs base and ring-0 stack are
	 * independent.  Done here, before any task/context-switch, so switch_to()'s
	 * per-CPU CUR_TSS->esp0 update stays consistent with where TR points from the
	 * very start.  Supersedes pcpu_install_gs(0) (which only loaded %gs). */
	pcpu_gdt_tss_load(0);

	/* Register the console sinks (COM1 + VGA) before the first kprintf. */
	kconsole_arch_init();

	/* Do A Clear Screen Just To Make The TEXT Buffer Nice And Empty */
	clearScreen();

	kprintf(UBIXOS_VERSION_STRING " — booting\n");

	/* Modify src/sys/include/ubixos/init.h to add a startup routine */
	for (i = 0x0; i < init_tasksTotal; i++)
	{
		if (init_tasks[i]() != 0x0)
			kpanic("Error: Initializing System Task[%i].\n", i);
	}

	/*
	 * Mount the boot partition as "sys:" using information passed by GRUB
	 * via the multiboot boot_device field.  This avoids hardcoded disk/
	 * partition numbers and works regardless of which IDE slot the disk
	 * ends up on.
	 *
	 * boot_device bits 31-24: BIOS drive (0x80 = first HD)
	 * boot_device bits 23-16: partition  (0-based; 0xFF = unpartitioned)
	 *
	 * _multiboot_info is zero when booted via the legacy FreeBSD loader,
	 * in which case we fall back to the old hardcoded values.
	 */
	{
		int sys_major = 1, sys_minor = 1;

		if (_multiboot_info != 0)
		{
			struct multiboot_info *mbi = (struct multiboot_info *)_multiboot_info;
			if (mbi->flags & MB_FLAG_BOOT_DEVICE)
			{
				sys_major = mb_drive_to_major(mbi->boot_device);
				sys_minor = mb_partition_to_minor(mbi->boot_device);
				kprintf("multiboot: boot_device=0x%X -> "
				        "major=%i minor=%i\n",
				        mbi->boot_device,
				        sys_major,
				        sys_minor);
			}
		}

		/* UbixFS driver registration must precede any pool mount. */
		ubfs_vfs_init();

		/* K5/M3 — prefer the UbixFS pool as the root filesystem.  The pool lives on
		 * its own block-device partition (type 0x9C, ad0s3 = major 1, minor 3 — see
		 * hd.c's per-partition device registration), mounted via the bcache raw vdev.
		 * If the partition holds a valid, bootable pool it becomes / (the mountable
		 * root); otherwise we fall back to the FAT root.  This is safe: on a bad or
		 * absent pool, vfs_mount's initfs returns failure and the / mountpoint is
		 * unlinked + freed (see vfs_mount), leaving / free for the FAT fallback. */
		int root_is_pool = 0;
		ubfs_vfs_set_raw(1);
		if (vfs_mount(1, 3, 0x0, VFS_TYPE_UBIXFS, "/", "rw") == 0)
		{
			fileDescriptor_t *probe_init = fopen("/bin/init", "r");
			if (probe_init != NULL)
			{
				fclose(probe_init);
				root_is_pool = 1;
				kprintf("Mounted root (UbixFS pool) from ad0s3 — mountable root live\n");
			}
			else
			{
				/* Mounted but no /bin/init (should not happen — mkimage stages a
				 * complete pool).  We cannot cleanly unmount /, so keep it + warn. */
				root_is_pool = 1;
				kprintf("ubixfs: pool is / but /bin/init missing — boot may fail\n");
			}
		}

		if (!root_is_pool)
		{
			kprintf("kmain: pool root unavailable; mounting FAT root major=%i minor=%i\n",
			        sys_major,
			        sys_minor);
			if (vfs_mount(sys_major, sys_minor, 0x0, 0xFA, "/", "rw") != 0x0)
				kprintf("Problem Mounting root (FAT) from major=%i "
				        "minor=%i\n",
				        sys_major,
				        sys_minor);
			else
				kprintf("Mounted root (FAT) from major=%i minor=%i\n", sys_major, sys_minor);
		}

		/* UbixFS loopback pool demo (plan K4 — i386 parity with aarch64): if a
		 * file-backed pool image is staged on the root, mount it read-write at /pool
		 * (ubixfs over /pool.img, the same arch-neutral driver).  Read-back proves the
		 * VFS dispatch; the boot.log write proves the write path + txg commit persist
		 * across reboot.  Present on the FAT fallback root; absent when the pool itself
		 * is / (so this silently no-ops there). */
		{
			fileDescriptor_t *probe = fopen("/pool.img", "r");
			if (probe != NULL)
			{
				fclose(probe);
				if (vfs_mount(0, 0, 0, VFS_TYPE_UBIXFS, "/pool", "rw") == 0)
				{
					fileDescriptor_t *pf = fopen("/pool/hello.txt", "r");
					char b[96];
					int n;
					if (pf != NULL)
					{
						n = (int)fread(b, 1, sizeof(b) - 1, pf);
						if (n > 0)
						{
							b[n] = '\0';
							kprintf("ubixfs: read /pool/hello.txt (%d bytes): %s", n, b);
						}
						fclose(pf);
					}
					pf = fopen("/pool/boot.log", "r");
					if (pf != NULL)
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
					if (pf != NULL)
					{
						const char *msg = "uBixOS i386 booted; UbixFS write path live.\n";
						fwrite((void *)msg, 1, 44, pf);
						fclose(pf);
						kprintf("ubixfs: wrote /pool/boot.log + committed\n");
					}
				}
			}
		}

		/* K5/M3 — confirm the root holds a COMPLETE bootable system: init, the
		 * dynamic linker, login, and the desktop assets (compositor, fonts, settings
		 * DB).  When the pool is /, read straight off the live root (prefix "").  On
		 * the FAT fallback, mount the raw pool at /poolraw (the mountable-root
		 * candidate, ad0s3) and verify it there instead. */
		{
			const char *prefix = root_is_pool ? "" : "/poolraw";
			int verify = root_is_pool;

			if (!root_is_pool)
			{
				ubfs_vfs_set_raw(1);
				verify = (vfs_mount(1, 3, 0x0, VFS_TYPE_UBIXFS, "/poolraw", "rw") == 0);
				if (!verify)
					kprintf("ubixfs: raw pool mount (/poolraw) failed or absent\n");
			}

			if (verify)
			{
				static const char *rel_files[] = {
				    "/bin/init",
				    "/bin/login",
				    "/lib/ld-musl-i386.so.1",
				    "/lib/libc.so",
				    "/etc/userdb",
				    "/bin/views",
				    "/usr/local/share/netsurf/SANS.TTF",
				    "/var/db/ubistry.db",
				};
				char path[128];
				char rb[8];
				unsigned bi;

				kprintf("ubixfs: root = %s — boot-critical files:\n",
				        root_is_pool ? "UbixFS pool (ad0s3)" : "FAT (pool at /poolraw)");
				for (bi = 0; bi < sizeof(rel_files) / sizeof(rel_files[0]); bi++)
				{
					fileDescriptor_t *pf;
					int n;

					snprintf(path, sizeof(path), "%s%s", prefix, rel_files[bi]);
					pf = fopen(path, "r");
					if (pf == NULL)
					{
						kprintf("ubixfs:   MISSING %s\n", path);
						continue;
					}
					n = (int)fread(rb, 1, 4, pf);
					kprintf("ubixfs:   %-40s %7d bytes %s\n",
					        path,
					        (int)pf->size,
					        (n == 4 && rb[0] == 0x7F && rb[1] == 'E') ? "(ELF)" : "");
					fclose(pf);
				}
			}
		}
	}

	/* Seed the kernel CSPRNG now that systemVitals is live. */
	krandom_init();

	/* Initialize the system */
	kprintf("Free Pages: [%i]\n", systemVitals->freePages);
	kprintf("MemoryMap:  [0x%X]\n", vmmMemoryMap);
	kprintf("Starting OS\n");

	kprintf("Kernel Name: [%s], Boot How To [0x%X], Boot Dev: [0x%X]\n", _kernelname, _boothowto, _bootdev);
	kprintf("B_TYPE(0x%X), B_SLICE(0x%X), B_UNIT(0x%X), B_PARTITION(0x%X)\n",
	        B_TYPE(_bootdev),
	        B_SLICE(_bootdev),
	        B_UNIT(_bootdev),
	        B_PARTITION(_bootdev));
	kprintf("_bootinfo.bi_version: 0x%X\n", _bootinfo.bi_version);
	kprintf("_bootinfo.bi_size: 0x%X\n", _bootinfo.bi_size);
	kprintf("_bootinfo.bi_bios_dev: 0x%X\n", _bootinfo.bi_bios_dev);

	execThread(systemTask, 0x2000, 0x0, "systemTask");
	execThread(pageout_daemon, 0x2000, 0x0, "pageout");

	/*
	 * Idle thread: lowest priority, runs only when every other task is blocked.
	 * It halts the CPU until the next interrupt, so tasks that sleep on a wait
	 * channel (sched_wait_event) truly give up the CPU instead of spinning, and
	 * the host isn't pegged at 100%.  Re-homed to QOS_IDLE since execThread
	 * starts threads at QOS_DEFAULT.  (Safe to set priority here: the timer IRQ
	 * is still masked until irqEnable() below, so the scheduler isn't running.)
	 */
	g_idle_task = (kTask_t *)execThread(idle_task, 0x2000, 0x0, "idle");
	sched_set_priority(g_idle_task, QOS_IDLE); /* Phase 3.5: g_idle_task tags idle vs busy ticks */

	execFile("/bin/init", argv_init, envp_init, 0x0); /* OS Initializer    */

	irqEnable(0x0);

	while (0x1)
		asm("hlt");

	/* Keep haulting until the scheduler reacts */

	/* Return to start however we should never get this far */
	return (0x0);
}
