/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * AArch64 SVC-syscall demo (QEMU `virt` bring-up).
 *
 * The first EL0->EL1 round trip: copy the position-independent EL0 payload
 * (el0.S) into a fresh frame, map it EL0-executable plus a user stack into an
 * unused VA region (the 0xC0000000 1 GB slot — NOT block 0, which holds the
 * GIC/PL011 peripherals), then aarch64_enter_el0() drops to EL0.  The payload
 * issues two "print" syscalls and an exit syscall; the EL1 sync handler
 * (exceptions.c) decodes ESR/SVC and dispatches, and exit longjmps back here.
 *
 * Throwaway scaffolding — the real path comes with per-process address spaces,
 * trapframes and ELF exec.
 */

#include "bringup.h"
#include <vmm/vmm.h>
#include <vmm/paging.h>  /* PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>      /* memcpy */

#define USER_CODE_VA 0xC0001000UL
#define USER_STACK_VA 0xC0002000UL
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000UL

/**
 * Clean the D-cache and invalidate the I-cache for [@addr, @addr+@len) to the
 * point of unification, so the CPU fetches freshly-written code (assumes a
 * 64-byte cache line, valid for the QEMU cortex-a72).
 */
static void sync_icache(uintptr_t addr, u_int64_t len)
{
	uintptr_t p, start = addr & ~63UL, end = addr + len;

	for (p = start; p < end; p += 64)
		__asm__ volatile("dc cvau, %0" : : "r"(p) : "memory");
	__asm__ volatile("dsb ish");
	for (p = start; p < end; p += 64)
		__asm__ volatile("ic ivau, %0" : : "r"(p) : "memory");
	__asm__ volatile("dsb ish; isb");
}

/**
 * Set up a minimal EL0 payload + stack and run it, exercising the SVC path.
 */
void aarch64_syscall_demo(void)
{
	u_int64_t ttbr0;
	u_int64_t *l1;
	uintptr_t code_frame, stack_frame;
	u_int64_t code_len = (u_int64_t)(user_demo_code_end - user_demo_code_start);

	__asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
	l1 = (u_int64_t *)(uintptr_t)(ttbr0 & PTE_ADDR_MASK);

	kprintf("syscall demo: dropping to EL0 to make SVCs...\n");

	code_frame = vmm_find_free_page(sysID);
	stack_frame = vmm_find_free_page(sysID);

	memcpy((void *)code_frame, user_demo_code_start, code_len); /* identity-mapped */
	sync_icache(code_frame, code_len);

	pmap_map_user_page(l1, USER_CODE_VA, (u_int64_t)code_frame, 1);   /* executable */
	pmap_map_user_page(l1, USER_STACK_VA, (u_int64_t)stack_frame, 0); /* data/stack */

	aarch64_enter_el0(USER_CODE_VA, USER_STACK_TOP);
	/* aarch64_el0_return() (exit syscall) longjmps back to here. */

	kprintf("syscall demo: back at EL1 after EL0 exit — SVC syscalls work on aarch64.\n");
}
