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

extern void ret_from_fork(void);

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

	if (ring3)
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

	/* Saved callee-saved registers popped by switch_to (pop ebp/edi/esi/ebx).
	 * Their values are immaterial for a new task — the trapframe (user) or the
	 * fresh function (kernel thread) establishes the real register state. */
	*--sp = 0; /* ebx */
	*--sp = 0; /* esi */
	*--sp = 0; /* edi */
	*--sp = 0; /* ebp */

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
