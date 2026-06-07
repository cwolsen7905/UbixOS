/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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
 * Okay, this file contains the code that's going to bootstrap the AP cpus
 */


	.globl	ap_trampoline_start,ap_trampoline_end
	.text
	.code16
ap_trampoline_start:
	cli
	cld

	movw	%cs,%ax		// The CPU knows its CS already, so lets use it for the other segments
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss

	// Do some bochs-specific bullshit
	mov	$0x31,%al // '1'
	mov	$0xe9,%dx
	outb	%al,%dx
        //lgdt    ap_gdt;	
	lgdt	ap_trampoline_gdt_limit - ap_trampoline_start
	movl	%cr0,%eax
	orl	$0x1,%eax
	movl	%eax,%cr0	// PMODE!

.code32
	.byte 0x66
	ljmp	$0x08,$(ap_trampoline_32 - ap_trampoline_start)		// 0x08 == KERNEL_CS

ap_trampoline_32:
	mov	$0x32,%al // '2'
	mov	$0xe9,%dx
	outb	%al,%dx

	mov	$0x10,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%fs
	mov	%ax,%gs
	mov	%ax,%ss

	// Spinlock
	mov	ap_trampoline_spl - ap_trampoline_start,%edi
ap_spl:
	//cmp	$1,(%edi)
	//je	ap_spl

	mov	$1,%eax	     // Value to be set
	xchgl	(%edi),%eax
	cmp	$0,%eax
	je	ap_spl
	// /Spinlock

	mov	$0x30,%al // '0'
	mov	$0xe9,%dx
	outb	%al,%dx
		
	mov	ap_trampoline_stackptr - ap_trampoline_start,%ebx
	mov	%ebx,%esp
	add	$0x1000,%ebx
	mov	%ebx,ap_trampoline_stackptr - ap_trampoline_start

	mov	$0x31,%al // '1'
	mov	$0xe9,%dx
	outb	%al,%dx

	// spinunlock
	mov	$0,%eax
	mov	ap_trampoline_spl - ap_trampoline_start,%edi
	xchgl	(%edi),%eax
	// /spinunlock

	mov	$0x33,%al // '3'
	mov	$0xe9,%dx
	outb	%al,%dx

	mov	ap_trampoline_epoint,%eax
	call	*%eax
1:
	hlt
	jmp	1b		// Halt if we ever get here somehow

	// Stack.. This sucks, since CPU initialization isn't serialized
ap_trampoline_stackptr:
	.long	0x10000		// 256KB
ap_trampoline_epoint:
	.long	c_ap_boot

ap_trampoline_spl:
	.long	0
ap_gdt:
	.long ubixGDT
	
	// GDT
ap_trampoline_gdt:
	.word	0
ap_trampoline_gdt_limit:
	.word	128	// Room for 32 descriptors
ap_trampoline_gdt_base:
	.long	0x20000		// 128KB (move this later)


ap_trampoline_end:
