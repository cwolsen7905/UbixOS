/* mpi_createMbox — UbixOS-native syscall 50 (int $0x81 / SVC native table). */
#ifdef __aarch64__
.text
.globl mpi_createMbox
.type  mpi_createMbox, %function
mpi_createMbox:
	mov	x8, #0x8032	/* 50 | NATIVE_FLAG(0x8000); args already in x0..x5 */
	svc	#0
	ret
.size mpi_createMbox, . - mpi_createMbox
#else
.text
.globl mpi_createMbox
.type  mpi_createMbox, @function
mpi_createMbox:
	movl	$50, %eax
	int	$0x81
	ret
.size mpi_createMbox, . - mpi_createMbox
#endif
