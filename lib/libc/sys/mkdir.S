.text
.globl mkdir
.type mkdir,@function
mkdir:
  movl $0x88,%eax   /* syscall 136 */
  int $0x80
  ret
