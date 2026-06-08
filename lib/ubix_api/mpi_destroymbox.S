/* mpi_destroyMbox — UbixOS-native syscall 51 (int $0x81 / SVC native table). */
#ifdef __aarch64__
.text
.globl mpi_destroyMbox
.type  mpi_destroyMbox, %function
mpi_destroyMbox:
	mov	x8, #0x8033	/* 51 | NATIVE_FLAG(0x8000); name already in x0 */
	svc	#0
	ret
.size mpi_destroyMbox, . - mpi_destroyMbox
#else
.text
.globl mpi_destroyMbox
.type  mpi_destroyMbox, @function
mpi_destroyMbox:
	movl $51, %eax
	leal 4(%esp), %ecx   /* name */
	int  $0x81
	ret
#endif
