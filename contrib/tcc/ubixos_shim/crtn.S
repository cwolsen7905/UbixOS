/*
 * crtn.S — end of .init and .fini sections for UbixOS TCC executables.
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 */
.section .init,"ax",@progbits
	popl	%ebp
	ret

.section .fini,"ax",@progbits
	popl	%ebp
	ret
