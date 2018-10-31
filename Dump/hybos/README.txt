OSD demo code - Interim release (January, 2003)

I am working a new set of kernels, but it's slow going, so here
are the old kernels with some bug fixes and improvements.

Location of these files: http://www.execpc.com/~geezer/osd/code

The GRUB bootloader is GPL, all other files are PUBLIC DOMAIN
(no copyright). You may do whatever you want with them.

================================================================
QUICK START
================================================================
1. Make sure you have all the software tools you need. You need NASM:
        http://nasm.sourceforge.net

   You need the GNU C compiler (GCC), including binutils, GNU Make,
   and sed. Users of DOS and Win9x should get GCC for DOS (DJGPP):
        http://www.delorie.com/djgpp

   Find a mirror site near you and download the following
   packages (NNN is version number):
        compiler        v2gnu/gccNNNNb.zip
        binutils        v2gnu/bnuNNNNb.zip
        libraries       v2/djdevNNN.zip
        Make            v2gnu/makNNNNb.zip
        sed             v2gnu/sedNNb.zip
        CWSDPMI         v2misc/csdpmiN.zip

   Users of WinNT/Win2000/WinXP should get GCC for Win32 (MinGW):
        http://mingw.sourceforge.net

   Users of Linux can use the GNU tools that come with Linux. If
   they're not already installed, check the CD-ROMs or FTP site
   from where you got Linux.

   DOS and Windows users also need John Fine's Partial Copy (PARTCOPY):
        http://www.execpc.com/~geezer/johnfine/#zero

2. Create a formatted floppy disk with the GRUB bootloader
   installed on it. The procedure to do this is described below.

3. Got your tools? Got your GRUB floppy? Then you're ready to
   build and install the demo kernels.

   If you're using DJGPP, type          make -f dj.mak install

   If you're using MinGW, type          make -f ming.mak install

   If you're using Linux, type          make -f linux.mak install

Hopefully, everything will work properly. If it does, you'll be
left with a bootable GRUB floppy with 7 demo kernels on it
(OSD 3 - OSD 9).

================================================================
MAKING A BOOTABLE FLOPPY DISK WITH THE GRUB BOOTLOADER ON IT
================================================================
GRUB is a GPLed bootloader.

Home page:      http://www.gnu.org/software/grub
Binaries:       ftp://alpha.gnu.org/gnu/grub/grub-0.90-i386-pc.tar.gz
Source code:    ftp://alpha.gnu.org/gnu/grub/grub-0.90.tar.gz
HOW-TOs:        http://www.washingdishes.freeuk.com/grubtut.html
                http://www.execpc.com/~geezer/osd/boot/grub-how.txt

1. You will need
   - Two 1.44 meg floppy disks, one of them formatted with a
     filesystem that GRUB recognizes (e.g. FAT12 or ext2).
     The other floppy (the "unformatted" floppy) may contain
     a filesystem, but it will be destroyed.
   - The GRUB binaries: files "stage1" and "stage2". These are
     in the OSD distribution, in the directory "boot/grub".
   - A "menu.lst" configuration file for GRUB. Again, this
     is provided with OSD in the "boot/grub" directory.

2. On the formatted floppy disk, create the subdirectory
   "/boot/grub/", and copy the files "stage1", "stage2", and
   "menu.lst" into this subdirectory.

3. Concatenate the binary files "stage1" and "stage2" into a
   single binary file named "boot":
	(DOS/Windows):  copy /b stage1 + stage2 boot
	(Linux):        cat stage1 stage2 >boot

4. Write the file "boot" directly to the unformatted floppy.
   This is a sector-level write that does not use (and will
   destroy) any filesystem present on the disk:
	(DOS/Windows):  partcopy boot 0 168000 -f0
	(Linux):        cat boot >/dev/fd0

   PARTCOPY will display an error message because "boot" is
   much shorter than 0x168000 bytes, but this is OK.

5. Boot your PC from the unformatted floppy disk.

6. After GRUB has started, eject the unformatted floppy and
   insert the formatted floppy, containing the "stage1",
   "stage2", and "menu.lst" files, all in the "/boot/grub/"
   subdirectory. Type:
	setup (fd0)

7. The formatted floppy is now bootable. Do not move, modify,
   or delete the file "/boot/grub/stage2" on this floppy.

================================================================
BUGS/GOTCHAS/"IT DOESN'T WORK!"
================================================================
- DJGPP crashes under Windows XP
  The DJGPP team is working on a way to fix this. If you have
  any flavor of WindowsNT (including WindowsXP) use MinGW
  instead of DJGPP.

- Will CygWin work instead of MinGW?
  Probably, but I haven't tested it.

- 'make -f ming.mak clean' doesn't delete .o files in /lib/
   I know, but I can't figure out how to fix it. Make sure
   these files get deleted if you switch between MinGW and
   DJGPP. The linkers can't distinguish between DJGPP COFF
   .o files and Win32 PE COFF .o files, but they ARE different,
   and your kernels will crash if you mix the two COFF types.

- BFD: Dwarf Error: Abbrev offset (1075154) greater than or
    equal to .debug_abbrev size (3198).
  This is a bug in new versions of DJGPP. It's ugly, but you
  can ignore it.

- Do not use MinGW GCC 2.x with NASM
    MinGW based on GCC 2.95.2 stores the BSS size in the wrong
    field of the section header. Because of this bug, this
    version of MinGW will not interoperate with other toolchains,
    including NASM, Microsoft compilers, and Borland compilers.

- Screen fills with errors when compiling under Linux
    There are two possible causes for this:
    1. Linux GCC chokes on DOS-style newlines in the source code.
       Run all source code files and makefiles through 'fromdos'
       to convert the DOS newlines (CR-LF) to UNIX newlines (LF).
    2. There is no newline at the very end of the file.

- ld: krnl.x: Not enough room for program headers, try linking with -N
    This error occurs if you do not use AT() in the linker script
    when making an ELF kernel.

- Exception 10 (bad TSS)
    If the NT bit in the EFLAGS register is set, IRET will attempt
    a TSS-based task-switch, instead of a normal IRET. Code was
    added to osd9/kstart.asm to prevent this. (GRUB 0.90 leaves
    the NT bit set when it jumps to the kernel.)

- Errors from GRUB:
  'Error: Loading below 1M not supported'
  'Error 28: Selected item won't fit into memory'
   These errors are usually caused by improperly-linked ELF
   kernels. If your kernel is ELF, try this:
        objdump --private-headers -h krnl.x
   The vaddr and paddr fields for each program header (segment)
   must be at or above 1 meg (00100000h) but smaller than your
   RAM size. If you have readelf, you can also do this:
	readelf --segments krnl.x
   If you find a segment with bad values of VirtAddr or
   PhysAddr, look at the section-to-segment mapping to see
   which section is causing the problem.

================================================================
DEMO CODE OVERVIEW
================================================================
osd3    mixing C and asm, C calling convention, underscores
        (xxx - current code does not demonstrate these very well)

osd4    putch() and kprintf()

osd5    C library code moved to separate directory

osd6    software interrupts, exception handlers in asm and C

osd7    hardware interrupts, reprogramming the 8259 interrupt
        controller chips, simple keyboard driver

osd8    multitasking preliminaries: multiple (virtual) consoles
        backed with video memory, ANSI/VT escapes for moving
	the cursor and changing text color. Improved keyboard
	driver; can press Ctrl+Alt+Del to reboot.

osd9    cooperative multitasking with setjmp() and longjump()
        (static tasks; linked into the kernel at compile-time)

================================================================
WHAT CHANGED?
================================================================
- Updated GRUB to version 0.90
- Wrote MinGW makefiles, and fixed code to work with MinGW
  based on GCC 3.x (and generate error message for GCC 2.x)
- Fixed definition of MAKEFILE in lib/linux.mak
- BIG changes to linker script
  - Unified ELF/DJGPP COFF/Win32 PE COFF linker scripts into one
  - Added AT() statements so ELF kernels link properly, and
    work properly
  - Now handling .rodata section properly, so ELF kernels work
    properly (including multiple .rodata sections with GCC 3.x)
- Rewrote setjmp() and longjmp() because they were buggy (though,
  strangely enough, the bug did not manifest itself)
- Added code to osd9/kstart.asm to zero NT bit in EFLAGS,
  preventing exception 10
- Now enabling interrupts for each task in function init_tasks()
  of osd9/sched.c. In previous code, interrupts were disabled
  for all tasks except the idle task, making the blinking
  character in the upper left corner of the screen appear to
  blink much more slowly than 18 Hz.
