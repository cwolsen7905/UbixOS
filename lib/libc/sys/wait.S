/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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

/* wait4(pid, status, options, rusage) — syscall 7 */
.text
.globl wait4
.type  wait4, @function
wait4:
	movl	$7, %eax
	int	$0x80
	ret
.size wait4, . - wait4

/*
 * waitpid(pid, status, options) — implemented as wait4 with rusage=NULL.
 * Rebuild the stack frame inserting the extra NULL rusage argument.
 */
.globl waitpid
.type  waitpid, @function
waitpid:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$0		/* rusage = NULL */
	pushl	16(%ebp)	/* options */
	pushl	12(%ebp)	/* status */
	pushl	8(%ebp)		/* pid */
	pushl	$0		/* fake return address (UbixOS ABI: args at esp+4) */
	movl	$7, %eax
	int	$0x80
	movl	%ebp, %esp
	popl	%ebp
	ret
.size waitpid, . - waitpid
