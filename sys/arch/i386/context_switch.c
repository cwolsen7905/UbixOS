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
extern void cpu_switch(u_int32_t *save_ksp_slot, u_int32_t next_ksp, u_int32_t next_cr3);

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

	cpu_switch(save_slot, next->md.md_kstack, next->md.md_tss.cr3);
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
 * cpu_switch(save_ksp_slot, next_ksp, next_cr3) — the register-level switch.
 *
 * cdecl args (read before any callee-saved push):
 *   4(%esp) = &prev->md.md_kstack   (where to save the outgoing kernel ESP)
 *   8(%esp) = next->md.md_kstack    (incoming kernel ESP)
 *  12(%esp) = next->md.md_tss.cr3   (incoming address space)
 *
 * Saves ebx/esi/edi/ebp + ESP into the prev slot, swaps CR3 if it changed
 * (kernel VA is shared across all spaces, so the stack stays mapped), loads the
 * next ESP, restores its callee-saved regs and returns — into the previous
 * cpu_switch caller (existing task) or ret_from_fork / a thread entry (new task).
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
    "  movl %edx, %esp     \n" /* load next kernel stack */
    "  popl %ds            \n" /* restore next's data segments (in next's CR3)   */
    "  popl %es            \n"
    "  popl %fs            \n"
    "  popl %gs            \n"
    "  popl %ebp           \n"
    "  popl %edi           \n"
    "  popl %esi           \n"
    "  popl %ebx           \n"
    "  ret                 \n");
