/*
 * crti.S — beginning of .init and .fini sections for UbixOS TCC executables.
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 */
.section .init,"ax",@progbits
.globl _init
.type  _init,@function
_init:
	pushl	%ebp
	movl	%esp, %ebp

.section .fini,"ax",@progbits
.globl _fini
.type  _fini,@function
_fini:
	pushl	%ebp
	movl	%esp, %ebp
