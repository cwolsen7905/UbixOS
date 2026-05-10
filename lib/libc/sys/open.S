.text
.globl open
.type open,@function
open:
  movl $0x5,%eax
  int $0x80
  ret
