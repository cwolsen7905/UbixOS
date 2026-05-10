.text
.globl mpi_fetchMessage
.type mpi_fetchMessage,@function
mpi_fetchMessage:
  movl $53,%eax
  int $0x81
  ret
