	.text
	.file	"/ubixos/usr/src/lib/csu/amd64/crt1.c"
                                        # Start of file scope inline assembly
	.ident	"$FreeBSD: releng/11.1/lib/csu/amd64/crt1.c 292000 2015-12-08 19:32:58Z emaste $"
	.ident	"$FreeBSD: releng/11.1/lib/csu/common/crtbrand.c 321309 2017-07-20 23:39:50Z gjb $"
	.ident	"$FreeBSD: releng/11.1/lib/csu/common/ignore_init.c 245133 2013-01-07 17:58:27Z kib $"

                                        # End of file scope inline assembly
	.globl	_start
	.p2align	4, 0x90
	.type	_start,@function
_start:                                 # @_start
	.cfi_startproc
# BB#0:
	pushq	%rbp
.Lcfi0:
	.cfi_def_cfa_offset 16
.Lcfi1:
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
.Lcfi2:
	.cfi_def_cfa_register %rbp
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbx
	pushq	%rax
.Lcfi3:
	.cfi_offset %rbx, -56
.Lcfi4:
	.cfi_offset %r12, -48
.Lcfi5:
	.cfi_offset %r13, -40
.Lcfi6:
	.cfi_offset %r14, -32
.Lcfi7:
	.cfi_offset %r15, -24
	movq	%rdi, %r14
	movq	(%r14), %r15
	movslq	%r15d, %rax
	leaq	16(%r14,%rax,8), %r13
	cmpq	$0, environ(%rip)
	jne	.LBB0_2
# BB#1:
	movq	%r13, environ(%rip)
.LBB0_2:
	addq	$8, %r14
	testl	%r15d, %r15d
	jle	.LBB0_9
# BB#3:
	movq	(%r14), %rax
	testq	%rax, %rax
	jne	.LBB0_4
	jmp	.LBB0_9
	.p2align	4, 0x90
.LBB0_8:                                #   in Loop: Header=BB0_4 Depth=1
	incq	%rax
.LBB0_4:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_5 Depth 2
	movq	%rax, __progname(%rip)
	jmp	.LBB0_5
	.p2align	4, 0x90
.LBB0_7:                                #   in Loop: Header=BB0_5 Depth=2
	incq	%rax
.LBB0_5:                                #   Parent Loop BB0_4 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movzbl	(%rax), %ecx
	cmpb	$47, %cl
	je	.LBB0_8
# BB#6:                                 #   in Loop: Header=BB0_5 Depth=2
	testb	%cl, %cl
	jne	.LBB0_7
.LBB0_9:
	movl	$_DYNAMIC, %eax
	testq	%rax, %rax
	je	.LBB0_11
# BB#10:
	movq	%rsi, %rdi
	callq	atexit
	jmp	.LBB0_12
.LBB0_11:
	callq	_init_tls
.LBB0_12:
	movl	$_DYNAMIC, %eax
	testq	%rax, %rax
	jne	.LBB0_23
# BB#13:
	movl	$finalizer, %edi
	callq	atexit
	movl	$__preinit_array_start, %eax
	movl	$__preinit_array_end, %ecx
	subq	%rax, %rcx
	movq	%rcx, %rax
	sarq	$63, %rax
	shrq	$61, %rax
	addq	%rcx, %rax
	sarq	$3, %rax
	je	.LBB0_18
# BB#14:
	xorl	%r12d, %r12d
	movl	$__preinit_array_start, %eax
	movl	$__preinit_array_end, %ecx
	subq	%rax, %rcx
	movq	%rcx, %rbx
	sarq	$63, %rbx
	shrq	$61, %rbx
	addq	%rcx, %rbx
	sarq	$3, %rbx
	.p2align	4, 0x90
.LBB0_15:                               # =>This Inner Loop Header: Depth=1
	movq	__preinit_array_start(,%r12,8), %rax
	cmpq	$2, %rax
	jb	.LBB0_17
# BB#16:                                #   in Loop: Header=BB0_15 Depth=1
	movl	%r15d, %edi
	movq	%r14, %rsi
	movq	%r13, %rdx
	callq	*%rax
.LBB0_17:                               #   in Loop: Header=BB0_15 Depth=1
	incq	%r12
	cmpq	%rbx, %r12
	jb	.LBB0_15
.LBB0_18:
	callq	_init
	movl	$__init_array_start, %eax
	movl	$__init_array_end, %ecx
	subq	%rax, %rcx
	movq	%rcx, %rax
	sarq	$63, %rax
	shrq	$61, %rax
	addq	%rcx, %rax
	sarq	$3, %rax
	je	.LBB0_23
# BB#19:
	xorl	%ebx, %ebx
	movl	$__init_array_start, %eax
	movl	$__init_array_end, %ecx
	subq	%rax, %rcx
	movq	%rcx, %r12
	sarq	$63, %r12
	shrq	$61, %r12
	addq	%rcx, %r12
	sarq	$3, %r12
	.p2align	4, 0x90
.LBB0_20:                               # =>This Inner Loop Header: Depth=1
	movq	__init_array_start(,%rbx,8), %rax
	cmpq	$2, %rax
	jb	.LBB0_22
# BB#21:                                #   in Loop: Header=BB0_20 Depth=1
	movl	%r15d, %edi
	movq	%r14, %rsi
	movq	%r13, %rdx
	callq	*%rax
.LBB0_22:                               #   in Loop: Header=BB0_20 Depth=1
	incq	%rbx
	cmpq	%r12, %rbx
	jb	.LBB0_20
.LBB0_23:
	movl	%r15d, %edi
	movq	%r14, %rsi
	movq	%r13, %rdx
	callq	main
	movl	%eax, %edi
	callq	exit
.Lfunc_end0:
	.size	_start, .Lfunc_end0-_start
	.cfi_endproc

	.p2align	4, 0x90
	.type	finalizer,@function
finalizer:                              # @finalizer
	.cfi_startproc
# BB#0:
	pushq	%rbp
.Lcfi8:
	.cfi_def_cfa_offset 16
.Lcfi9:
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
.Lcfi10:
	.cfi_def_cfa_register %rbp
	pushq	%rbx
	pushq	%rax
.Lcfi11:
	.cfi_offset %rbx, -24
	movl	$__fini_array_start, %eax
	movl	$__fini_array_end, %ecx
	subq	%rax, %rcx
	movq	%rcx, %rax
	sarq	$63, %rax
	shrq	$61, %rax
	addq	%rcx, %rax
	sarq	$3, %rax
	je	.LBB1_4
# BB#1:
	movl	$__fini_array_start, %eax
	movl	$__fini_array_end, %ecx
	subq	%rax, %rcx
	movq	%rcx, %rbx
	sarq	$63, %rbx
	shrq	$61, %rbx
	addq	%rcx, %rbx
	sarq	$3, %rbx
	.p2align	4, 0x90
.LBB1_2:                                # =>This Inner Loop Header: Depth=1
	movq	__fini_array_start-8(,%rbx,8), %rax
	decq	%rbx
	cmpq	$1, %rax
	jbe	.LBB1_3
# BB#5:                                 #   in Loop: Header=BB1_2 Depth=1
	callq	*%rax
.LBB1_3:                                #   in Loop: Header=BB1_2 Depth=1
	testq	%rbx, %rbx
	jne	.LBB1_2
.LBB1_4:
	addq	$8, %rsp
	popq	%rbx
	popq	%rbp
	jmp	_fini                   # TAILCALL
.Lfunc_end1:
	.size	finalizer, .Lfunc_end1-finalizer
	.cfi_endproc

	.type	abitag,@object          # @abitag
	.section	.note.tag,"a",@note
	.p2align	2
abitag:
	.long	8                       # 0x8
	.long	4                       # 0x4
	.long	1                       # 0x1
	.asciz	"FreeBSD"
	.long	1101001                 # 0x10ccc9
	.size	abitag, 24

	.type	.L.str,@object          # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.zero	1
	.size	.L.str, 1

	.type	__progname,@object      # @__progname
	.data
	.globl	__progname
	.p2align	3
__progname:
	.quad	.L.str
	.size	__progname, 8

	.type	crt_noinit_tag,@object  # @crt_noinit_tag
	.section	.note.tag,"a",@note
	.p2align	2
crt_noinit_tag:
	.long	8                       # 0x8
	.long	4                       # 0x4
	.long	2                       # 0x2
	.asciz	"FreeBSD"
	.long	0                       # 0x0
	.size	crt_noinit_tag, 24

	.type	environ,@object         # @environ
	.comm	environ,8,8
	.hidden	__preinit_array_end
	.hidden	__preinit_array_start
	.hidden	__init_array_end
	.hidden	__init_array_start
	.hidden	__fini_array_end
	.hidden	__fini_array_start
	.hidden	_init
	.hidden	_fini
	.weak	_DYNAMIC

	.ident	"FreeBSD clang version 4.0.0 (tags/RELEASE_400/final 297347) (based on LLVM 4.0.0)"
	.section	".note.GNU-stack","",@progbits
