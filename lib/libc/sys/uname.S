.text
.globl uname
.type uname,@function
uname:
  movl $0xA4,%eax   /* syscall 164 */
  int $0x80
  ret
.size uname, . - uname
