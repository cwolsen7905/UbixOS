/*
 * bios16code.S — 16-bit real-mode stub executed in VM86 mode by biosCall().
 *
 * With IOPL=3 and EFLAGS.VM=1, INT executes directly through the real-mode
 * IVT (no #GP intercept).  After INT 10h returns via IRET, the HLT
 * generates a #GP (privileged in VM86 regardless of IOPL) which the trap
 * handler catches to mark the VM86 task complete (state = 0).
 */
.globl bios16Code
bios16Code:
.code16
	int  $0x10
	hlt
