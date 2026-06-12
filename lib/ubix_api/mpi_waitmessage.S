/* mpi_waitMessage — UbixOS-native syscall 69: blocking MPI receive (int $0x81 /
 * SVC native table).  Sleeps until the mailbox has a message or the tick timeout
 * elapses, so a service loop blocks instead of busy-polling fetch+yield. */
#ifdef __aarch64__
.text
.globl mpi_waitMessage
.type  mpi_waitMessage, %function
mpi_waitMessage:
	mov	x8, #0x8045	/* 69 | NATIVE_FLAG(0x8000); args already in x0..x5 */
	svc	#0
	ret
.size mpi_waitMessage, . - mpi_waitMessage
#else
.text
.globl mpi_waitMessage
.type  mpi_waitMessage, @function
mpi_waitMessage:
	movl	$69, %eax
	int	$0x81
	ret
.size mpi_waitMessage, . - mpi_waitMessage
#endif
