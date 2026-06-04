/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * IRQ dispatch stubs — one per hardware IRQ (0-15).
 * Each stub saves registers, pushes the IRQ number, calls irq_dispatch(),
 * then restores and returns.  EOI is sent inside irq_dispatch().
 */

#include <i386/pcpu_asm.h>

.code32
.text

.macro irq_stub num
.globl irq_entry_\num
irq_entry_\num:
	pusha
	pushl	%gs			/* save the interrupted %gs (e.g. user TLS) */
	PCPU_LOAD_GS			/* %gs = SEL_PCPU for per-CPU data access   */
	pushl	$\num
	call	irq_dispatch
	addl	$4, %esp
	popl	%gs			/* restore the interrupted %gs */
	popa
	iret
.endm

irq_stub 0
irq_stub 1
irq_stub 2
irq_stub 3
irq_stub 4
irq_stub 5
irq_stub 6
irq_stub 7
irq_stub 8
irq_stub 9
irq_stub 10
irq_stub 11
irq_stub 12
irq_stub 13
irq_stub 14
irq_stub 15

/* Table of stub addresses for irq_register() to index into */
.section .rodata
.globl irq_entry_table
irq_entry_table:
	.long irq_entry_0
	.long irq_entry_1
	.long irq_entry_2
	.long irq_entry_3
	.long irq_entry_4
	.long irq_entry_5
	.long irq_entry_6
	.long irq_entry_7
	.long irq_entry_8
	.long irq_entry_9
	.long irq_entry_10
	.long irq_entry_11
	.long irq_entry_12
	.long irq_entry_13
	.long irq_entry_14
	.long irq_entry_15
