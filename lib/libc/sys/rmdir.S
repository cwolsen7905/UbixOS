.text
.globl rmdir
.type rmdir,@function
rmdir:
  movl $0x89,%eax   /* syscall 137 */
  int $0x80
  ret
