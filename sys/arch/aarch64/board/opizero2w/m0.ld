/* Orange Pi Zero 2W (H618) M0 link script.  Link at the same 2 MB-aligned RAM
 * address the real kernel uses (0x40200000); U-Boot loads the Image there and
 * `booti`s it.  _image_size feeds the arm64 Image header in start.S. */
ENTRY(_start)
SECTIONS
{
	. = 0x40200000;
	.text : { KEEP(*(.text.boot)) *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) }
	.bss : { *(.bss*) *(COMMON) }
	. = ALIGN(16);
	. += 0x4000;			/* 16 KB boot stack */
	_stack_top = .;
	. = ALIGN(8);
	__end = .;
	_image_size = __end - 0x40200000;
}
