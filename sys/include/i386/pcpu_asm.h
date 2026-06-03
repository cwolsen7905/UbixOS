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
 * Per-CPU %gs loading for kernel entry paths — single source of truth.
 *
 * Every i386 kernel entry stub (interrupt, exception, syscall, IRQ) must run
 * with %gs = SEL_PCPU so that %gs:offset reaches the running CPU's struct pcpu
 * (e.g. %gs:8 == _current); see <ubixos/sched.h> and <i386/pcpu.h>.  Because
 * the segment registers are no longer per-task TSS state under software
 * context switching, each stub loads it explicitly on entry and restores the
 * task's real %gs on exit.
 *
 * There were a dozen-plus near-identical "movl $0x58,%eax; movw %ax,%gs" copies
 * scattered across .S files and inline asm() blocks; this header collapses them
 * to one definition so the selector (or the load sequence) is changed in a
 * single place.  Both forms clobber %eax — every caller invokes them after a
 * pusha/pushal (or with %eax otherwise free).
 *
 *   - .S files / standalone assembly:  use the PCPU_LOAD_GS macro.
 *   - inline asm() string blocks in C:  paste the ASM_PCPU_LOAD_GS string.
 *
 * PCPU_GS_SEL must equal SEL_PCPU in <sys/gdt.h>; a _Static_assert in
 * context_switch.c enforces this so the two cannot drift.
 */

#ifndef _I386_PCPU_ASM_H
#define _I386_PCPU_ASM_H

#define PCPU_GS_SEL 0x58 /* == SEL_PCPU (GDT index 11 << 3); asserted in context_switch.c */

#ifdef __ASSEMBLER__

/* clang-format off — GAS .macro syntax, not C; clang-format collapses/garbles it. */
.macro PCPU_LOAD_GS
	movl $PCPU_GS_SEL, %eax
	movw %ax, %gs
.endm
/* clang-format on */

#else

#define PCPU_STRINGIZE2(x) #x
#define PCPU_STRINGIZE(x) PCPU_STRINGIZE2(x)
#define ASM_PCPU_LOAD_GS "  movl $" PCPU_STRINGIZE(PCPU_GS_SEL) ", %eax\n  movw %ax, %gs\n"

#endif /* __ASSEMBLER__ */

#endif /* _I386_PCPU_ASM_H */
