.text
.globl mpi_destroyMbox
.type  mpi_destroyMbox, @function
mpi_destroyMbox:
	movl $51, %eax
	leal 4(%esp), %ecx   /* name */
	int  $0x81
	ret
