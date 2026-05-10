.text
.globl dup2
.type dup2,@function
dup2:
  movl $0x5A,%eax
  int $0x80
  ret
