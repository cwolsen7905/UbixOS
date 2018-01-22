.text
.globl mpi_createMbox
.type mpi_createMbox,@function
mpi_createMbox:
  movl $50,%eax
  int $0x81
  ret
