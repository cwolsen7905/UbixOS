.text
.globl pidStatus
.type  pidStatus, @function
pidStatus:
	movl	$7, %eax
	int	$0x80
	ret
.size pidStatus, . - pidStatus
