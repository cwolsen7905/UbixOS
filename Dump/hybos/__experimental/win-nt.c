/*****************************************************************************
Sector-level disk I/O code for Windows NT.
This code is public domain (no copyright).
You can do whatever you want with it.

EXPORTS:
int read_sector(disk_t *disk, unsigned long lba, unsigned char *buf);
int open_disk(disk_t *disk, unsigned drive_num);
int is_fat_bootsector(unsigned char *buf);

Tim Robinson helped with this code.
(Bugs are due to Giese :)
*****************************************************************************/
#include <windows.h>
/* FILE, fopen(), setvbuf(), fseek(), fread(), fclose(), printf(), sprintf() */
#include <stdio.h>
#include "diskio.h"
/*****************************************************************************
*****************************************************************************/
static FILE *do_open(unsigned drive_num)
{
	char dev_name[64];
	FILE *f;

/* form internal drive name */
	if(drive_num < 0x80)
		sprintf(dev_name, "\\\\.\\%c:", drive_num + 'A');
	else
		sprintf(dev_name, "\\\\.\\PhysicalDrive%u",
			drive_num - 0x80);
/* open the drive */
	f = fopen(dev_name, "r+b");
	if(f == NULL)
		printf("do_open(): drive 0x%02X (%s) does not exist\n",
			drive_num, dev_name);
	return f;
}
/*****************************************************************************
*****************************************************************************/
int read_sector(disk_t *disk, unsigned long lba, unsigned char *buf)
{
	unsigned err;
	FILE *f;

	lba += disk->partition_start;
	f = do_open(disk->drive_num);
	if(f == NULL)
		return -1;
/* seek, read, close */
	setvbuf(f, NULL, _IOFBF, BPS);
	fseek(f, lba * BPS, SEEK_SET);
	err = fread(buf, 1, BPS, f);
	fclose(f);
	return (err == BPS) ? 0 : -1;
}
/*****************************************************************************
*****************************************************************************/
int open_disk(disk_t *disk, unsigned drive_num)
{
	OSVERSIONINFO win_version;
	unsigned char buf[BPS];
	unsigned err = 0;
	FILE *f;

/* check for NT */
	win_version.dwOSVersionInfoSize = sizeof(win_version);
	GetVersionEx(&win_version);
	if(win_version.dwPlatformId != VER_PLATFORM_WIN32_NT)
	{
		printf("Sorry, Windows NT required\n"
			"Windows 9x users should use the "
			"DOS version of this program\n\n");
		return -1;
	}
	disk->drive_num = drive_num;
	disk->partition_start = 0; /* assume floppy */
	disk->use_lba = 0;	/* assume CHS */
/* open the drive, to make sure it exists */
	f = do_open(disk->drive_num);
	if(f == NULL)
		return -1;
	fclose(f);
/* no CHS geometry - NT uses LBA for everything */
	return 0;
}
/*****************************************************************************
is_fat_bootsector() is not used in this file, but still used in DEMO.C
*****************************************************************************/
int is_fat_bootsector(unsigned char *buf)
{
	int temp, ok = 1;

	DEBUG(printf("check_if_fat_bootsector:\n");)
/* must start with 16-bit JMP or 8-bit JMP plus NOP */
	if(buf[0] == 0xE9)
		/* OK */;
	else if(buf[0] == 0xEB && buf[2] == 0x90)
		/* OK */;
	else
	{
		DEBUG(printf("\tMissing JMP/NOP\n");)
		ok = 0;
	}
	temp = buf[13];
	if(temp == 0 || ((temp - 1) & temp) != 0)
	{
		DEBUG(printf("\tSectors per cluster (%u) "
			"is not a power of 2\n", temp);)
		ok = 0;
	}
	temp = buf[16];
	temp = LE16(buf + 24);
	if(temp == 0 || temp > 63)
	{
		DEBUG(printf("\tInvalid number of sectors (%u)\n", temp);)
		ok = 0;
	}
	temp = LE16(buf + 26);
	if(temp == 0 || temp > 255)
	{
		DEBUG(printf("\tInvalid number of heads (%u)\n", temp);)
		ok = 0;
	}
	return ok;
}
