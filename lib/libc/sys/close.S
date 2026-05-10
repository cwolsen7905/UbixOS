.text
.globl close
.type close,@function
close:
  movl $0x6,%eax
  int $0x80
  ret
