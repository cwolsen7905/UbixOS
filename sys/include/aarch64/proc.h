/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 machine-dependent per-process state embedded in kTask_t (the 'md'
 * field).  No hardware TSS: a software context frame.  Sibling of i386/proc.h;
 * reached via <machine/proc.h>.
 */
#ifndef _AARCH64_PROC_H
#define _AARCH64_PROC_H

#include <sys/types.h>

struct md_proc
{
	u_int64_t md_ksp;   /* saved kernel SP (callee-saved frame; aarch64_ctx_switch) */
	u_int64_t md_ttbr0; /* user address-space root (TTBR0_EL1), 0 for kernel-only */
};

struct taskStruct; /* == kTask_t */

void md_setup_initial_frame(struct taskStruct *t);
void switch_to(struct taskStruct *prev, struct taskStruct *next);

#endif /* _AARCH64_PROC_H */
