/* Raspberry Pi 3 (BCM2837) M0 link script.  The VideoCore firmware loads
 * kernel8.img as a raw flat binary at 0x80000 and jumps there, so link at
 * 0x80000 (no arm64 Image header). */
ENTRY(_start)
SECTIONS
{
	. = 0x80000;
	.text : { KEEP(*(.text.boot)) *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) }
	.bss : { *(.bss*) *(COMMON) }
	. = ALIGN(16);
	. += 0x4000;			/* 16 KB boot stack */
	_stack_top = .;
}
