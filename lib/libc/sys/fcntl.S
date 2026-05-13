.text
.globl fcntl
.type fcntl,@function
fcntl:
	movl  $0x5C, %eax      /* SYS_fcntl */
	int   $0x80
	ret
.size fcntl, . - fcntl
