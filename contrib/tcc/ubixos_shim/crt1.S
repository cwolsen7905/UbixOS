/*
 * crt1.S — minimal UbixOS startup for TCC-linked executables.
 * Copyright (c) 2002-2026 The UbixOS Project. All rights reserved.
 *
 * Stack at process entry (set by i386_exec.c):
 *   [esp]     = argc
 *   [esp+4]   = argv[0]  (pointer)
 *   ...
 */

	.text
	.globl _start
_start:
	xorl   %ebp, %ebp          /* mark outermost frame */
	movl   (%esp), %eax        /* eax = argc */
	leal   4(%esp), %ecx       /* ecx = argv */
	andl   $0xFFFFFFF0, %esp   /* 16-byte align */
	subl   $8, %esp
	pushl  %ecx                /* argv */
	pushl  %eax                /* argc */
	call   main
	pushl  %eax                /* exit code */
	call   exit
1:	hlt
	jmp    1b
