        .text
        .align  4
        .globl  _start
        .type   _start, @function
_start:
        .cfi_startproc
        xorl    %ebp,%ebp
        pushl   %ebp
        .cfi_def_cfa_offset 4
        movl    %esp,%ebp
        .cfi_offset %ebp,-8
        .cfi_def_cfa_register %ebp
        andl    $0xfffffff0,%esp # align stack
        leal    8(%ebp),%eax
        subl    $4,%esp
        pushl   %eax            # argv
        pushl   4(%ebp)         # argc
        pushl   %edx            # rtld cleanup
        call    _start1
        int3
        .cfi_endproc
        .size   _start, . - _start

        .section .note.GNU-stack,"",%progbits
