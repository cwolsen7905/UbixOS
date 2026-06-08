/* mpi_fetchMessage — UbixOS-native syscall 53 (int $0x81 / SVC native table). */
#ifdef __aarch64__
.text
.globl mpi_fetchMessage
.type  mpi_fetchMessage, %function
mpi_fetchMessage:
	mov	x8, #0x8035	/* 53 | NATIVE_FLAG(0x8000); args already in x0..x5 */
	svc	#0
	ret
.size mpi_fetchMessage, . - mpi_fetchMessage
#else
.text
.globl mpi_fetchMessage
.type  mpi_fetchMessage, @function
mpi_fetchMessage:
	movl	$53, %eax
	int	$0x81
	ret
.size mpi_fetchMessage, . - mpi_fetchMessage
#endif
