.text
.globl pipe
.type pipe,@function
pipe:
  movl $0x21E,%eax   /* 542 = pipe2; kernel ignores flags arg */
  int $0x80
  ret
.size pipe, . - pipe
