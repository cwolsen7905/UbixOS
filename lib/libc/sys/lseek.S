// (C) 2002-2026 The UbixOS Project
.text
.globl lseek
.type lseek,@function
lseek:
  movl $478,%eax
  int $0x80
  ret
.size lseek, . - lseek
