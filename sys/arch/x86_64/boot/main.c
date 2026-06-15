/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 bring-up C entry.  Reached from start.S in 64-bit long mode with paging
 * on (identity map of the low 1 GB).  Phase 1: COM1 banner (proof the long-mode
 * transition + 64-bit C ABI work).  Phase 2: install the IDT so CPU faults are
 * visible instead of a silent triple-fault.  The rest of the kernel grows from
 * here (paging allocator, then the i386 drivers widened to 64-bit).
 */

#include "x86_64.h"
#include <ubixos/sched.h>
#include <x86_64/pcpu.h>
#include <fs/vfs/vfs.h>   /* vfs_init, VFS_TYPE_FAT */
#include <fs/vfs/mount.h> /* vfs_mount */
#include <fs/vfs/file.h>  /* vfs_opendir/readdir/closedir, kDIR_t, struct kdirent */
#include <fs/fat/fat.h>   /* fat_init */

/**
 * x86-64 kernel C entry.  @mb_magic / @mb_info are the boot magic + info pointer
 * start.S preserved (zero-extended) in the SysV arg registers.
 */
void kmain_x86_64(u32 mb_magic, u32 mb_info)
{
	serial_init();
	x86_64_console_init(); /* register COM1 as a kconsole sink for kprintf */
	serial_puts("\nuBixOS x86_64 (long mode) - boot OK\n");
	serial_puts("x86_64 bring-up: COM1 up, PAE+LME+paging on, 64-bit C ABI live.\n");
	serial_puts("  boot magic=");
	serial_puthex(mb_magic);
	serial_puts(" info=");
	serial_puthex(mb_info);
	serial_puts("\n");

	/* Install the per-CPU block (GS_BASE -> g_pcpu[0]) before anything reads
	 * _current — once interrupts are on, the timer's sched_account_tick() reads
	 * _current via %gs:16, so GS_BASE must already point at a block whose
	 * `current` is NULL.  (aarch64 installs TPIDR_EL1 at the top of kmain for the
	 * same reason.) */
	x86_64_pcpu_install(0);

	/* Ring-3 GDT + TSS (the ring-0 stack the CPU loads on a ring3->ring0 trap). */
	x86_64_usermode_init();

	idt_init();
	serial_puts("IDT installed: 256 gates, 32 CPU-exception handlers (faults now visible).\n");

	x86_64_mem_init();

	/* Phase 3 smoke test: the kernel heap (MI kmalloc over the MI page allocator). */
	{
		extern void *kmalloc(u32);
		extern void kfree(void *);
		char *a = (char *)kmalloc(4096);
		char *b = (char *)kmalloc(256);
		serial_puts("kmalloc test: a=");
		serial_puthex((u64)a);
		serial_puts(" b=");
		serial_puthex((u64)b);
		if (a != 0)
		{
			a[0] = 0x5A;
			a[4095] = 0xA5;
			serial_puts((a[0] == 0x5A && a[4095] == (char)0xA5) ? " [rw ok]" : " [rw FAIL]");
		}
		kfree(a);
		kfree(b);
		serial_puts(" [freed]\n");
	}

	/* Phase 4a: PIC + PIT timer + interrupts. */
	pic_remap();
	pit_init(100); /* 100 Hz */
	__asm__ __volatile__("sti");
	serial_puts("PIC remapped (IRQ 32-47), PIT at 100 Hz, interrupts on.\n");

	/* Verify the timer IRQ fires: print the first 3 one-second marks. */
	{
		u64 next = 100;
		int marks = 0;
		while (marks < 3)
		{
			if (timer_ticks() >= next)
			{
				serial_puts("  timer tick ");
				serial_putdec(timer_ticks());
				serial_puts(" (~");
				serial_putdec((timer_ticks() / 100));
				serial_puts("s)\n");
				next += 100;
				marks++;
			}
			__asm__ __volatile__("hlt"); /* wait for the next interrupt */
		}
	}

	/* Phase 4b-1: verify the raw register switch (cpu_switch.S) in isolation. */
	x86_64_ctx_test();

	/* Phase 4b-2: bring up the machine-independent scheduler over that switch.
	 * sched_init() creates task 0 (the kernel task); set_current() makes this
	 * boot context that task so sched()/switch_to have a valid "prev". */
	{
		extern int sched_init(void);
		extern kTask_t *taskList;
		extern void x86_64_sched_demo(void);

		u64 boot_cr3;

		sched_init();
		set_current(taskList);
		taskList->state = RUNNING;
		taskList->priority = QOS_DEFAULT; /* share the CPU at the default QoS */
		taskList->base_priority = QOS_DEFAULT;
		/* sched_init() builds the boot task without md_new_task, leaving md_cr3 = 0;
		 * record the boot PML4 so switch_to restores it when a user task exits back
		 * to this kernel context (the boot PML4 keeps the low identity that the MI
		 * ELF loader's direct frame access relies on — user PML4s do not). */
		__asm__ __volatile__("mov %%cr3, %0" : "=r"(boot_cr3));
		taskList->md.md_cr3 = boot_cr3;
		x86_64_sched_demo();
	}

	/* Phase 5b: run a real ring-3 process the scheduler dispatches, in its own
	 * per-process address space (supersedes the 5a one-shot enter/leave demo). */
	x86_64_proc_demo();

	/* Phase 5d-C: load a synthesized static ELF64 through the arch-neutral loader
	 * and run it at the standard amd64 base (0x400000) in the clean user low half. */
	x86_64_elf_demo();

	/* Phase 5c: probe the virtio-blk-pci disk, then mount its FAT partition as the
	 * VFS root and list it — proof the block driver + buffer cache + FAT + VFS all
	 * work together (the foundation execve/init will load binaries through). */
	if (virtio_blk_init() == 0)
	{
		vfs_init();
		fat_init(); /* register the FAT driver with the VFS */

		/* Partition 1 (vtblk0s1, major 1 / minor 1) is the FAT volume. */
		if (vfs_mount(1, 1, 0, VFS_TYPE_FAT, "/", "rw") == 0)
		{
			kDIR_t *d = vfs_opendir("/");
			serial_puts("vfs: mounted FAT root on vtblk0s1; readdir / :\n");
			if (d != 0)
			{
				struct kdirent e;
				int n = 0;
				while (vfs_readdir(d, &e) == 0 && n < 32)
				{
					serial_puts("  /");
					serial_puts(e.d_name);
					serial_puts("\n");
					n++;
				}
				vfs_closedir(d);
				serial_puts("vfs: FAT root readable — disk-backed root works on x86_64.\n");
			}
			else
				serial_puts("vfs: opendir(/) failed\n");
		}
		else
			serial_puts("vfs: FAT mount failed\n");
	}

	serial_puts("x86_64 Phase 4b-2 (generic scheduler) verified. Idle.\n");
	for (;;)
		__asm__ __volatile__("hlt");
}
