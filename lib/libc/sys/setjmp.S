/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * i386 setjmp / longjmp
 *
 * jmp_buf layout (6 x int32):
 *   [0] EBX  [1] ESI  [2] EDI  [3] EBP  [4] ESP  [5] EIP
 */

.text

.globl setjmp
.type  setjmp,@function
setjmp:
	movl	4(%esp), %edx		/* edx = jmp_buf ptr */
	movl	%ebx,    0(%edx)
	movl	%esi,    4(%edx)
	movl	%edi,    8(%edx)
	movl	%ebp,   12(%edx)
	leal	4(%esp), %eax		/* caller's ESP after our ret */
	movl	%eax,   16(%edx)
	movl	0(%esp), %eax		/* return address = saved EIP */
	movl	%eax,   20(%edx)
	xorl	%eax,    %eax		/* return 0 */
	ret
.size setjmp, . - setjmp

.globl longjmp
.type  longjmp,@function
longjmp:
	movl	4(%esp), %edx		/* edx = jmp_buf ptr */
	movl	8(%esp), %eax		/* eax = val */
	testl	%eax,    %eax
	jnz	1f
	incl	%eax			/* ensure setjmp returns != 0 */
1:
	movl	 0(%edx), %ebx
	movl	 4(%edx), %esi
	movl	 8(%edx), %edi
	movl	12(%edx), %ebp
	movl	20(%edx), %ecx		/* saved EIP */
	movl	16(%edx), %esp		/* restore ESP */
	jmp	*%ecx
.size longjmp, . - longjmp
