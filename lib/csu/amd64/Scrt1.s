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
	movq	(%rdi), %r13
	movslq	%r13d, %rax
	leaq	16(%rdi,%rax,8), %rax
	movq	%rax, -48(%rbp)         # 8-byte Spill
	movq	environ@GOTPCREL(%rip), %rax
	cmpq	$0, (%rax)
	jne	.LBB0_2
# BB#1:
	movq	-48(%rbp), %rcx         # 8-byte Reload
	movq	%rcx, (%rax)
.LBB0_2:
	addq	$8, %rdi
	testl	%r13d, %r13d
	jle	.LBB0_10
# BB#3:
	movq	(%rdi), %rax
	testq	%rax, %rax
	je	.LBB0_10
# BB#4:
	movq	__progname@GOTPCREL(%rip), %rcx
	jmp	.LBB0_5
	.p2align	4, 0x90
.LBB0_9:                                #   in Loop: Header=BB0_5 Depth=1
	incq	%rax
.LBB0_5:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_6 Depth 2
	movq	%rax, (%rcx)
	jmp	.LBB0_6
	.p2align	4, 0x90
.LBB0_8:                                #   in Loop: Header=BB0_6 Depth=2
	incq	%rax
.LBB0_6:                                #   Parent Loop BB0_5 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movzbl	(%rax), %edx
	cmpb	$47, %dl
	je	.LBB0_9
# BB#7:                                 #   in Loop: Header=BB0_6 Depth=2
	testb	%dl, %dl
	jne	.LBB0_8
.LBB0_10:
	movq	%rdi, %r15
	cmpq	$0, _DYNAMIC@GOTPCREL(%rip)
	je	.LBB0_12
# BB#11:
	movq	%rsi, %rdi
	callq	atexit@PLT
	cmpq	$0, _DYNAMIC@GOTPCREL(%rip)
	je	.LBB0_14
	jmp	.LBB0_24
.LBB0_12:
	callq	_init_tls@PLT
	cmpq	$0, _DYNAMIC@GOTPCREL(%rip)
	jne	.LBB0_24
.LBB0_14:
	leaq	finalizer(%rip), %rdi
	callq	atexit@PLT
	leaq	__preinit_array_start(%rip), %r12
	leaq	__preinit_array_end(%rip), %rax
	subq	%r12, %rax
	movq	%rax, %rcx
	sarq	$63, %rcx
	shrq	$61, %rcx
	addq	%rax, %rcx
	sarq	$3, %rcx
	movq	%r15, %r14
	movq	%r13, %r15
	movq	%rcx, %r13
	je	.LBB0_19
# BB#15:
	xorl	%ebx, %ebx
	.p2align	4, 0x90
.LBB0_16:                               # =>This Inner Loop Header: Depth=1
	movq	(%r12), %rax
	cmpq	$2, %rax
	jb	.LBB0_18
# BB#17:                                #   in Loop: Header=BB0_16 Depth=1
	movl	%r15d, %edi
	movq	%r14, %rsi
	movq	-48(%rbp), %rdx         # 8-byte Reload
	callq	*%rax
.LBB0_18:                               #   in Loop: Header=BB0_16 Depth=1
	incq	%rbx
	addq	$8, %r12
	cmpq	%r13, %rbx
	jb	.LBB0_16
.LBB0_19:
	movq	%r15, %r13
	movq	%r14, %r15
	callq	_init
	leaq	__init_array_start(%rip), %rbx
	leaq	__init_array_end(%rip), %rax
	subq	%rbx, %rax
	movq	%rax, %r12
	sarq	$63, %r12
	shrq	$61, %r12
	addq	%rax, %r12
	sarq	$3, %r12
	je	.LBB0_24
# BB#20:
	xorl	%r14d, %r14d
	.p2align	4, 0x90
.LBB0_21:                               # =>This Inner Loop Header: Depth=1
	movq	(%rbx), %rax
	cmpq	$2, %rax
	jb	.LBB0_23
# BB#22:                                #   in Loop: Header=BB0_21 Depth=1
	movl	%r13d, %edi
	movq	%r15, %rsi
	movq	-48(%rbp), %rdx         # 8-byte Reload
	callq	*%rax
.LBB0_23:                               #   in Loop: Header=BB0_21 Depth=1
	incq	%r14
	addq	$8, %rbx
	cmpq	%r12, %r14
	jb	.LBB0_21
.LBB0_24:
	movl	%r13d, %edi
	movq	%r15, %rsi
	movq	-48(%rbp), %rdx         # 8-byte Reload
	callq	main@PLT
	movl	%eax, %edi
	callq	exit@PLT
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
	pushq	%r14
	pushq	%rbx
.Lcfi11:
	.cfi_offset %rbx, -32
.Lcfi12:
	.cfi_offset %r14, -24
	leaq	__fini_array_start(%rip), %r14
	leaq	__fini_array_end(%rip), %rax
	subq	%r14, %rax
	movq	%rax, %rbx
	sarq	$63, %rbx
	shrq	$61, %rbx
	addq	%rax, %rbx
	sarq	$3, %rbx
	je	.LBB1_3
	.p2align	4, 0x90
.LBB1_1:                                # =>This Inner Loop Header: Depth=1
	movq	-8(%r14,%rbx,8), %rax
	decq	%rbx
	cmpq	$1, %rax
	jbe	.LBB1_2
# BB#4:                                 #   in Loop: Header=BB1_1 Depth=1
	callq	*%rax
.LBB1_2:                                #   in Loop: Header=BB1_1 Depth=1
	testq	%rbx, %rbx
	jne	.LBB1_1
.LBB1_3:
	popq	%rbx
	popq	%r14
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
