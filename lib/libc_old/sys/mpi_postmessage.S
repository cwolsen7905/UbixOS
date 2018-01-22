.text
.globl mpi_postMessage
.type mpi_postMessage,@function
mpi_postMessage:
  movl $52,%eax
  int $0x81
  ret
