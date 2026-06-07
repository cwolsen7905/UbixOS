/*
 * bios16code.S — 16-bit real-mode stub executed in VM86 mode by biosCall().
 *
 * With IOPL=3 and EFLAGS.VM=1, INT $0x10 executes directly through the
 * real-mode IVT without trapping to the kernel.  After the BIOS returns
 * via IRET, we signal completion by writing 1 to physical address 0x600
 * (free conventional memory, above the BIOS Data Area which ends at 0x4FF).
 * biosCall() polls that address and kills us via endTask() when it sees it.
 *
 * We then loop on HLT (which with IOPL=3 actually halts rather than #GP-ing)
 * so we consume no CPU while waiting to be reaped.
 */
.globl bios16Code
bios16Code:
.code16
	int  $0x10
	int  $0x69   /* signal completion — trapped by __gpf case 0x69 */
.Ldone_loop:
	hlt
	jmp  .Ldone_loop
