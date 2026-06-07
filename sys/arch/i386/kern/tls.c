/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * i386 userland TLS install (the machine hook behind set_thread_area(2)).
 *
 * Moved out of the generic syscall layer (sys/posix/gen_calls.c) so generic
 * code carries no i386 segment/LDT assembly: userland TLS on i386 is the second
 * descriptor (LDT[1]) of the per-process LDT page, selected by user %gs = 0x0F
 * (TI=1, index 1, RPL 3).  All threads in an address space share this one
 * descriptor; cpu_switch() re-installs the resuming thread's base each switch
 * (see context_switch.c).  aarch64 supplies its own machine_set_tls (TPIDR_EL0).
 */

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/gdt.h>
#include <machine/vmm_layout.h>
#include <machine/tls.h>

/**
 * Install @base into LDT[1], reload the LDT register so it goes live, and set
 * the thread's return %gs to 0x0F so userland resumes with the TLS selector.
 */
void machine_set_tls(struct thread *td, uintptr_t base)
{
	struct gdtDescriptor *tls_desc = (struct gdtDescriptor *)(VMM_USER_LDT + sizeof(struct gdtDescriptor));
	u_int32_t base_addr = (u_int32_t)base;

	tls_desc->limitLow = 0xFFFF;
	tls_desc->limitHigh = 0xF;
	tls_desc->baseLow = (base_addr & 0xFFFF);
	tls_desc->baseMed = ((base_addr >> 16) & 0xFF);
	tls_desc->access = ((dData + dWrite + dBig + dBiglim + dDpl3) + dPresent) >> 8;
	tls_desc->granularity = ((dData + dWrite + dBig + dBiglim + dDpl3) & 0xFF) >> 4;
	tls_desc->baseHigh = (base_addr >> 24);

	/* Reload LDTR (selector 0x18) so the updated LDT[1] descriptor is live. */
	asm volatile("pushl %eax\n\t"
	             "movw  $0x18,%ax\n\t"
	             "lldt  %ax\n\t"
	             "popl  %eax\n\t");

	/*
	 * Propagate %gs = 0x0F to userland via the saved trapframe: sys_call_posix.S
	 * pops all segment registers on return, so writing tf_gs here is the only
	 * way to make %gs survive the iret (a direct kernel %gs write is discarded).
	 */
	td->frame->tf_gs = 0xF;
}
