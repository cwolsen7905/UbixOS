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
#include <vmm/vmm.h>        /* vmm_mem_map_init */
#include <ubixos/vitals.h>  /* vitals_init */
#include <fs/vfs/vfs.h>     /* vfs_init */
#include <fs/vfs/mount.h>   /* vfs_mount */
#include <fs/ramfs/ramfs.h> /* ramfs_init, ramfs_populate (initramfs root) */

/* The embedded boot program — stands in for /bin/init until the dynamic linker
 * + the real init/login/shell chain land. */
extern char _binary_hello_musl_elf_start[];
extern char _binary_hello_musl_elf_end[];

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
	vfs_init(); /* VFS core: filesystem registry + buffer cache */

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
	aarch64_procfs_demo(); /* mount /proc before the musl program (which reads it) */
	aarch64_ramfs_demo();  /* registers ramfs + exercises it at /ram */
	aarch64_musl_elf_demo();

	/* --- initramfs bootstrap: the shape of a real boot ---------------------
	 * Mount a ramfs root, lay /bin/init into it (the embedded program stands in
	 * for init until the dynamic linker + the real init/login/shell land), then
	 * exec it *from the filesystem path* — what the generic kmain will do once
	 * unified.  ramfs is already registered by the demo above. */
	kprintf("\n--- initramfs bootstrap ---\n");
	if (vfs_mount(0, 0, 0, VFS_TYPE_RAMFS, "/", "rw") == 0)
	{
		u_int32_t len = (u_int32_t)(_binary_hello_musl_elf_end - _binary_hello_musl_elf_start);
		if (ramfs_populate("/", "/bin/init", _binary_hello_musl_elf_start, len) == 0)
			aarch64_exec_file("/bin/init");
		else
			kprintf("bootstrap: failed to populate /bin/init\n");
	}
	else
	{
		kprintf("bootstrap: failed to mount ramfs root\n");
	}

	/* Spawn never-yielding CPU-bound tasks, then enable the timer — it must
	 * preempt them (proves preemptive scheduling). */
	aarch64_preempt_demo();

	gic_init();
	timer_init();

	/* Unmask IRQs (clear DAIF.I); the 100 Hz timer now drives the scheduler. */
	__asm__ volatile("msr daifclr, #2");
	kprintf("IRQs enabled; timer-driven preemption active.\n");

	for (;;)
		__asm__ volatile("wfi");
}
