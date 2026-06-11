/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Software context switching support (SMP prerequisite).
 *
 * Replaces i386 hardware task switching (ljmp to a per-task TSS) so the segment
 * registers stop being per-task TSS state — the thing that blocks %gs per-CPU.
 * See docs/design/software-task-switch-plan.md.
 *
 * This file provides:
 *   - md_setup_initial_frame(): builds a freshly created task's first kernel
 *     stack frame from its md_tss description, so the first switch_to() into it
 *     "returns" into user mode (ret_from_fork -> iret) or into a kernel thread
 *     entry point.
 *   - ret_from_fork: the trampoline a new user task is switched into; it runs
 *     the standard trap-return epilogue to iret into ring 3.
 *
 * switch_to() (the register save/restore + CR3 swap) and the sched() flip land
 * in the following steps; until then this code is built but dormant (hardware
 * ljmp switching is still active and v86 tasks keep using it).
 */

#include <ubixos/sched.h>
#include <machine/proc.h>
#include <sys/tss.h>
#include <sys/types.h>
#include <sys/gdt.h>
#include <i386/pcpu_asm.h>
#include <isa/8259.h>
#include <lib/kprintf.h>

/* The hand-written %gs loads in the entry stubs (PCPU_GS_SEL in <i386/pcpu_asm.h>)
 * must match the SEL_PCPU selector the GDT actually defines, or %gs:8 would not
 * reach g_pcpu.  Catch any divergence at compile time. */
_Static_assert(PCPU_GS_SEL == SEL_PCPU, "PCPU_GS_SEL must equal SEL_PCPU");

/*
 * The single kernel TSS used by software switching (the boot TSS the GDT's
 * selector 0x20 / TR points at).  Only its esp0/ss0 matter now: on a ring3->
 * ring0 trap the CPU reads them to find the kernel stack.  switch_to() updates
 * esp0 to the incoming task's kernel stack on every switch instead of
 * re-pointing the GDT descriptor at a per-task TSS.
 */
#define KERNEL_TSS ((struct tssStruct *)0x4200)

extern void ret_from_fork(void);
extern void enter_vm86(void);
extern void cpu_switch(u_int32_t *save_ksp_slot, u_int32_t next_ksp, u_int32_t next_cr3, u_int32_t next_tls_base);

/* Discard slot for the very first switch out of the boot context (prev == NULL);
 * the boot/kmain context is abandoned once init is dispatched. */
static u_int32_t g_boot_kstack_discard;

/**
 * Build a newly created task's initial kernel-stack frame for software switching.
 *
 * Consumes the task's md_tss (already populated by the creating path:
 * execFile/sys_fork for user tasks, execThread for kernel threads) and lays out
 * the kernel stack so the first switch_to() into this task resumes correctly:
 *
 *   - Ring 3 (user, cs RPL == 3): a full struct-trapframe is built at the top of
 *     the kernel stack (md_tss.esp0) followed by a return address of
 *     ret_from_fork.  switch_to pops the saved callee regs, returns to
 *     ret_from_fork, which pops the trapframe and irets into ring 3.
 *   - Ring 0 (kernel thread): the thread runs on its own stack (md_tss.esp,
 *     which execThread already seeded with the entry argument).  switch_to
 *     returns straight to the entry point (md_tss.eip); no iret.
 *
 * Must run AFTER md_tss is fully populated and is guarded to fire once per task.
 * v86 tasks are excluded (they keep hardware ljmp switching in the hybrid).
 */
/**
 * Initialise a freshly-allocated task's md state: point the TSS ring-0 stack
 * (esp0/ss0) at the top of the task's kernel stack.  Called by schedNewTask
 * once the kernel stack is allocated.
 */
void md_new_task(kTask_t *t)
{
	t->md.md_tss.esp0 = (u_int32_t)t->kernelStack + 65536; /* 64 KB kstack; matches schedNewTask kmalloc */
	t->md.md_tss.ss0 = 0x10;
}

/**
 * Per-dispatch arch hook invoked by the generic scheduler just before
 * switch_to().  i386 masks the timer IRQ while a VM86 (BIOS) task runs so the
 * tick cannot preempt the v86 monitor.
 */
void md_sched_pre_switch(kTask_t *t)
{
	if (t->oInfo.v86Task == 0x1)
		irqDisable(0x0); /* mask timer while v86 task runs */
}

void md_setup_initial_frame(kTask_t *t)
{
	struct tssStruct *tss = &t->md.md_tss;
	u_int32_t *sp;
	int ring3 = ((tss->cs & 0x3) == 0x3);

	if (t->oInfo.v86Task)
	{
		/*
		 * VM86 (BIOS) task: switch_to rets to enter_vm86, which does popa (BIOS
		 * registers from md_tss) then iret into VM86 mode.  At CPL0 with EFLAGS.VM
		 * set, iret pops eip,cs,eflags,esp,ss,es,ds,fs,gs (9 words), all real-mode
		 * 16-bit values from md_tss.
		 */
		sp = (u_int32_t *)(tss->esp0);

		*--sp = (u_int32_t)(tss->gs & 0xFFFF); /* iret VM86 frame (gs highest) */
		*--sp = (u_int32_t)(tss->fs & 0xFFFF);
		*--sp = (u_int32_t)(tss->ds & 0xFFFF);
		*--sp = (u_int32_t)(tss->es & 0xFFFF);
		*--sp = (u_int32_t)(tss->ss & 0xFFFF);
		*--sp = (u_int32_t)tss->esp;
		*--sp = (u_int32_t)tss->eflags; /* already has VM | IF | 2 */
		*--sp = (u_int32_t)(tss->cs & 0xFFFF);
		*--sp = (u_int32_t)tss->eip;

		*--sp = (u_int32_t)tss->eax; /* popa area (eax highest, edi lowest) */
		*--sp = (u_int32_t)tss->ecx;
		*--sp = (u_int32_t)tss->edx;
		*--sp = (u_int32_t)tss->ebx;
		*--sp = 0; /* esp slot ignored by popa */
		*--sp = (u_int32_t)tss->ebp;
		*--sp = (u_int32_t)tss->esi;
		*--sp = (u_int32_t)tss->edi;

		*--sp = (u_int32_t)enter_vm86; /* switch_to rets here */
	}
	else if (ring3)
	{
		/* Top of the kernel stack; build the trapframe downward in the exact
		 * field order of struct trapframe so ret_from_fork's epilogue pops it. */
		sp = (u_int32_t *)(tss->esp0);

		*--sp = (u_int32_t)(tss->ss & 0xFFFF); /* user SS   */
		*--sp = (u_int32_t)tss->esp;           /* user ESP  */
		*--sp = (u_int32_t)tss->eflags;
		*--sp = (u_int32_t)(tss->cs & 0xFFFF);
		*--sp = (u_int32_t)tss->eip;
		*--sp = 0; /* tf_err    (add $8 skips it)  */
		*--sp = 0; /* tf_trapno (add $8 skips it)  */
		*--sp = (u_int32_t)tss->eax;
		*--sp = (u_int32_t)tss->ecx;
		*--sp = (u_int32_t)tss->edx;
		*--sp = (u_int32_t)tss->ebx;
		*--sp = 0; /* tf_isp (ignored by popa)     */
		*--sp = (u_int32_t)tss->ebp;
		*--sp = (u_int32_t)tss->esi;
		*--sp = (u_int32_t)tss->edi;
		*--sp = (u_int32_t)(tss->ds & 0xFFFF);
		*--sp = (u_int32_t)(tss->es & 0xFFFF);
		*--sp = (u_int32_t)(tss->fs & 0xFFFF);
		*--sp = (u_int32_t)(tss->gs & 0xFFFF);

		*--sp = (u_int32_t)ret_from_fork; /* switch_to rets here */
	}
	else
	{
		/* Kernel thread: run on its own stack; switch_to rets to the entry. */
		sp = (u_int32_t *)tss->esp;
		*--sp = (u_int32_t)tss->eip; /* switch_to rets to the thread entry */
	}

	/* Saved callee-saved registers popped by cpu_switch.  Their values are
	 * immaterial for a new task — the trapframe (user) or the fresh function
	 * (kernel thread) establishes the real GP register state. */
	*--sp = 0; /* ebx */
	*--sp = 0; /* esi */
	*--sp = 0; /* edi */
	*--sp = 0; /* ebp */

	/* Kernel-context data segments restored by cpu_switch (kernel selectors).
	 * For a user task ret_from_fork's iret then installs the user segments;
	 * for a kernel thread these are the segments it runs with. */
	*--sp = 0x10; /* gs */
	*--sp = 0x10; /* fs */
	*--sp = 0x10; /* es */
	*--sp = 0x10; /* ds */

	t->md.md_kstack = (u_int32_t)sp;
}

/*
 * ret_from_fork — entry point a freshly created USER task is switched into the
 * first time.  switch_to() returns here with ESP pointing at the trapframe that
 * md_setup_initial_frame() built; run the standard trap-return epilogue to iret
 * into ring 3.  (Kernel threads do not pass through here.)
 */
asm(".globl ret_from_fork \n"
    "ret_from_fork:        \n"
    "  pop %gs             \n"
    "  pop %fs             \n"
    "  pop %es             \n"
    "  pop %ds             \n"
    "  popa                \n"
    "  add $8, %esp        \n" /* discard tf_trapno + tf_err */
    "  iret                \n");

/**
 * Software context switch from prev to next.
 *
 * Updates the single kernel TSS's esp0 to next's kernel stack (so a later
 * ring3->ring0 trap lands correctly), then hands off to cpu_switch() which saves
 * prev's callee-saved registers + kernel ESP, swaps CR3 if the address space
 * differs, loads next's kernel ESP and returns into next's context.  Replaces
 * the hardware `ljmp $0x20` switch.  Must be called with interrupts disabled.
 *
 * Setting esp0 here (in prev's context, before the stack switch) is safe: we run
 * at CPL0, so any interrupt before the switch uses the current kernel stack, not
 * esp0; esp0 is consulted only on the next ring3->ring0 entry, after the switch.
 */
void switch_to(kTask_t *prev, kTask_t *next)
{
	u_int32_t *save_slot = prev ? &prev->md.md_kstack : &g_boot_kstack_discard;

	/*
	 * Milestone B leak detector (zero behaviour change).
	 *
	 * cpu_switch pushes ...,%gs,%fs,%es,%ds and saves ESP, so the %gs selector it
	 * will reload for `next` sits at next->md_kstack + 12 (slot index 3).  In the
	 * per-CPU %gs design every kernel-context save must be SEL_PCPU; a value >=
	 * 0x60 is no GDT/LDT selector at all but a leaked VM86 real-mode segment (the
	 * old nondeterministic #GP saw 0x8298 here for the views task).  Flag it so a
	 * leak is traced to a task instead of triple-faulting.  Must not fire in the
	 * pre-Milestone-B working state.
	 */
	{
		u_int32_t saved_gs = ((u_int32_t *)next->md.md_kstack)[3] & 0xFFFF;

		if (saved_gs >= 0x60)
		{
			static u_int32_t g_gs_leak_count = 0;

			if (g_gs_leak_count < 32)
			{
				g_gs_leak_count++;
				kprintf("PCPU GS LEAK: next pid=%d name=%s saved_gs=0x%X\n",
				        next->id,
				        next->name,
				        saved_gs);
			}
		}
	}

	KERNEL_TSS->esp0 = next->md.md_tss.esp0;

	/*
	 * Set CR0.TS so the next task takes a device-not-available trap (#NM, _int7)
	 * on its first FPU use, triggering the lazy FPU save/restore (mathStateRestore).
	 * Hardware task switching set TS automatically on every ljmp; software
	 * switching must do it explicitly or stale x87 state leaks between tasks
	 * (symptom: a task's EIP jumps to the leftover FPU instruction pointer such
	 * as 0xF000:FFxx).
	 */
	__asm__ __volatile__("movl %%cr0, %%eax \n"
	                     "orl  $0x8, %%eax  \n" /* CR0.TS (bit 3) */
	                     "movl %%eax, %%cr0 \n"
	                     :
	                     :
	                     : "eax");

	cpu_switch(save_slot, next->md.md_kstack, next->md.md_tss.cr3, next->tls_base);
}

/*
 * enter_vm86 — entry point a VM86 (BIOS) task is switched into the first time.
 * popa loads the BIOS registers, iret enters VM86 mode at cs:eip.
 */
asm(".globl enter_vm86 \n"
    "enter_vm86:        \n"
    "  popa             \n"
    "  iret             \n");

/*
 * cpu_switch(save_ksp_slot, next_ksp, next_cr3, next_tls_base) — register switch.
 *
 * cdecl args (read before any callee-saved push):
 *   4(%esp) = &prev->md.md_kstack   (where to save the outgoing kernel ESP)
 *   8(%esp) = next->md.md_kstack    (incoming kernel ESP)
 *  12(%esp) = next->md.md_tss.cr3   (incoming address space)
 *  16(%esp) = next->tls_base        (incoming userland TLS base, 0 if none)
 *
 * Saves ebx/esi/edi/ebp + ESP into the prev slot, swaps CR3 if it changed
 * (kernel VA is shared across all spaces, so the stack stays mapped), loads the
 * next ESP, restores its callee-saved regs and returns — into the previous
 * cpu_switch caller (existing task) or ret_from_fork / a thread entry (new task).
 *
 * Userland TLS: all threads sharing an address space share the single LDT[1]
 * descriptor (at VMM_USER_LDT, selected by user %gs = 0xF), so after the CR3
 * swap — when VMM_USER_LDT maps the resuming task's LDT page — cpu_switch
 * re-installs next's TLS base into LDT[1] (base bytes only; the access/limit
 * bytes were set generically by set_thread_area).  Skipped when next_tls_base
 * is 0 (kernel threads, or a process before its first set_thread_area, whose
 * LDT page may not even be mapped).
 */
asm(".globl cpu_switch \n"
    "cpu_switch:        \n"
    "  movl 4(%esp), %eax  \n" /* &prev->md_kstack  */
    "  movl 8(%esp), %edx  \n" /* next_ksp          */
    "  movl 12(%esp), %ecx \n" /* next_cr3          */
    "  pushl %ebx          \n"
    "  pushl %esi          \n"
    "  pushl %edi          \n"
    "  pushl %ebp          \n"
    "  pushl %gs           \n" /* save data segments: with software switching   */
    "  pushl %fs           \n" /* the segment registers are NOT saved/restored  */
    "  pushl %es           \n" /* by hardware (the ljmp/TSS used to), so the     */
    "  pushl %ds           \n" /* switch must, or %gs (user TLS) leaks tasks.    */
    "  movl %esp, (%eax)   \n" /* *save_ksp_slot = current ESP */
    "  movl %cr3, %ebx     \n"
    "  cmpl %ecx, %ebx     \n"
    "  je 1f               \n"
    "  movl %ecx, %cr3     \n" /* swap address space */
    "1:                    \n"
    /*
     * Re-install next's userland TLS base into the shared LDT[1] descriptor
     * (VMM_USER_LDT + 8 = 0x7FF008; base bytes at +2 word, +4 byte, +7 byte).
     * Now safe: CR3 is next's, so 0x7FF00x maps next's LDT page.  esi was saved
     * above so it is free scratch; eax is caller-clobbered.  edx (next_ksp) is
     * preserved.  next_tls_base is the 4th arg: orig 16(%esp) + 32 bytes pushed.
     */
    "  movl 48(%esp), %esi \n" /* next_tls_base */
    "  testl %esi, %esi    \n"
    "  jz 2f               \n" /* 0 = no TLS (kernel thread / pre-set_thread_area) */
    "  movl %esi, %eax     \n"
    "  movw %ax, 0x007FF00A \n" /* LDT[1].baseLow  (bits 0..15)  */
    "  movl %esi, %eax     \n"
    "  shrl $16, %eax      \n"
    "  movb %al, 0x007FF00C \n" /* LDT[1].baseMed  (bits 16..23) */
    "  movl %esi, %eax     \n"
    "  shrl $24, %eax      \n"
    "  movb %al, 0x007FF00F \n" /* LDT[1].baseHigh (bits 24..31) */
    "2:                    \n"
    "  movl %edx, %esp     \n" /* load next kernel stack */
    "  popl %ds            \n" /* restore next's data segments (in next's CR3)   */
    "  popl %es            \n"
    "  popl %fs            \n"
    "  popl %gs            \n" /* popped then immediately overridden below        */
    /*
     * Milestone B: in kernel mode %gs is CPU-state, not task-state — it must be
     * SEL_PCPU (base = &g_pcpu[cpu]) so %gs:offset reaches per-CPU data.  Force
     * it here rather than trusting the popped value: a kernel context resumed by
     * cpu_switch is always at CPL0, and the real user/VM86 %gs is restored
     * separately by the trapframe (ret_from_fork) or the VM86 iret frame
     * (enter_vm86), never from this slot.  Forcing also makes the switch robust
     * against a leaked VM86 real-mode %gs reaching this point (the old #GP).
     */
    ASM_PCPU_LOAD_GS /* %gs = SEL_PCPU */
    "  popl %ebp           \n"
    "  popl %edi           \n"
    "  popl %esi           \n"
    "  popl %ebx           \n"
    "  ret                 \n");
