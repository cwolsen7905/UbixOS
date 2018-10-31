Sector-level disk I/O code for various OSes

This code is public domain (no copyright).
You can do whatever you want with it.

Chris Giese <geezer@execpc.com>	http://www.execpc.com/~geezer

================================================================
BUILD
================================================================
After a successful build, the executable is named 'diskio.exe'

DOS - Turbo C++ 1.0 or Turbo C++ 3.0:
	make

DOS - Watcom C:
	wmake /f watcom16.mak

32-bit DOS - DJGPP:
	make -f djgpp.mak

32-bit DOS - Watcom C with CauseWay DOS extender:
	wmake /f watcom32.mak

NOTE: 32-bit code built with Watcom C does not yet work.

Windows NT - MinGW or CygWin:
	make -f win-nt.mak

NOTE: Windows NT version is not tested.

Linux - GCC:
	xxx - to do

If you have Windows9x, build and run this code as a DOS program,
not a Win32 program.

================================================================
API
================================================================
/* disk_t, open_disk(), read_sector(), write_sector() */
#include "diskio.h"

int main(void) {
	unsigned long lba_sector_num;
	unsigned char buf[512];
	unsigned drive;
	disk_t disk;

/* drive values for floppies:
    0 for A:, 1 for B:, etc.
drive values for hard drives:
    0x80 for first hard drive, 0x81 for second hard drive, etc. */
	drive = 0;
/* read hard drive partition table or floppy boot sector */
	lba_sector_num = 0;
	if(open_disk(&disk, drive) == 0)
		if(read_sector(&disk, lba_sector_num, buf) == 0)
			/* success */;
	return 0; }

================================================================
TO DO
================================================================
Test if code writes to disk properly

Test Windows NT version
- See if the following work with Windows NT:
  - 1.44 meg FAT12 floppy		(my guess: YES)
  - 1.68 meg FAT12 floppy		(my guess: YES)
  - 1.44 meg non-FAT (e.g. ext2) floppy	(my guess: YES)
  - 1.68 meg non-FAT floppy		(my guess: NO)
- Is there a Windows NT ioctl() to get floppy disk geometry?
  Does it work for non-FAT floppy disks? (How do I make NT read
  a 1.68 meg non-FAT floppy?)

Make it work with Linux

Make it work with 32-bit Watcom C

Maybe add a disk cache

Maybe support disks with sector size other than 512 bytes
(e.g. 2048-byte CD-ROM sectors)?

Any way to access CD-ROM with INT 13h functions?
WITHOUT booting from the CD-ROM?
