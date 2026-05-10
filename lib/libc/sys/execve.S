.text
.globl execve
.type execve,@function
execve:
  movl $0x3B,%eax
  int $0x80
  ret
