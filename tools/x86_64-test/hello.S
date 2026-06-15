	.text
	.globl _start
_start:
	movq $4, %rax            /* SYS_write */
	movq $1, %rdi
	leaq msg(%rip), %rsi
	movq $(msg_end - msg), %rdx
	syscall
	movq $1, %rax            /* SYS_exit */
	xorq %rdi, %rdi
	syscall
1:	jmp 1b
msg:
	.ascii "hello from a real ELF64 on disk (x86_64 execve)\n"
msg_end:
