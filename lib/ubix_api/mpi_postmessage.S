/* mpi_postMessage — UbixOS-native syscall 52 (int $0x81 / SVC native table). */
#ifdef __aarch64__
.text
.globl mpi_postMessage
.type  mpi_postMessage, %function
mpi_postMessage:
	mov	x8, #0x8034	/* 52 | NATIVE_FLAG(0x8000); args already in x0..x5 */
	svc	#0
	ret
.size mpi_postMessage, . - mpi_postMessage
#else
.text
.globl mpi_postMessage
.type  mpi_postMessage, @function
mpi_postMessage:
	movl	$52, %eax
	int	$0x81
	ret
.size mpi_postMessage, . - mpi_postMessage
#endif
