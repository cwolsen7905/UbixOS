/*****************************************************************************
Sector-level disk I/O code for DOS.
This code is public domain (no copyright).
You can do whatever you want with it.

EXPORTS:
int read_sector(disk_t *disk, unsigned long lba, unsigned char *buf);
int write_sector(disk_t *disk, unsigned long lba, unsigned char *buf);
int is_fat_bootsector(unsigned char *buf);
int open_disk(disk_t *disk, unsigned drive_num);
*****************************************************************************/
#include <stdio.h> /* printf() */
/*#include <bios.h>*/ /* _DISK_..., diskinfo_t, _bios_disk() */
#include "diskio.h"
#include "dos.h" /* peekw() */

/* IMPORTS
from _16BIT.C, DJGPP, or ??? */
int lba_biosdisk(int cmd, int drive, unsigned long lba, int nsects, void *buf);
int get_hd_geometry(disk_t *disk);

/* xxx - I'm not sure of the Turbo C version where these were
introduced. They are present in Turbo C++ 3.0 (__TURBOC__ == 0x401)
but not in Turbo C++ 1.0 (__TURBOC__ == 0x296) */
#if defined(__TURBOC__)
#if __TURBOC__<0x300

struct diskinfo_t
{
	unsigned drive, head, track, sector, nsectors;
	void far *buffer;
};

unsigned _bios_disk(unsigned cmd, struct diskinfo_t *info)
{
	struct SREGS sregs;
	union REGS regs;

/* biosdisk() returns the 8-bit error code left in register AH by
the call to INT 13h. It does NOT return a combined, 16-bit error
code + number of sectors transferred, as described in the online help.

	return biosdisk(cmd, info->drive, info->head, info->track,
		info->sector, info->nsectors, info->buffer);
*/
	regs.h.ah = cmd;
	regs.h.al = info->nsectors;
	regs.x.bx = FP_OFF(info->buffer);
	regs.h.ch = info->track;
	regs.h.cl = (info->track / 256) * 64 + (info->sector & 0x3F);
	regs.h.dh = info->head;
	regs.h.dl = info->drive;
	sregs.es = FP_SEG(info->buffer);
	int86x(0x13, &regs, &regs, &sregs);
	return regs.x.ax;
}
#endif
#endif
/*****************************************************************************
*****************************************************************************/
int read_sector(disk_t *disk, unsigned long lba, unsigned char *buf)
{
	struct diskinfo_t cmd;
	unsigned tries, err;

	lba += disk->partition_start;
	cmd.drive = disk->drive_num;
/* use LBA if available */
	if(disk->use_lba)
	{
		return lba_biosdisk(_DISK_READ,
			disk->drive_num, lba, 1, buf);
	}
/* use CHS _bios_disk() */
	cmd.sector = (unsigned)(lba % disk->sectors + 1);
	cmd.head = (unsigned)((lba / disk->sectors) % disk->heads);
	cmd.track = (unsigned)((lba / disk->sectors) / disk->heads);
	cmd.nsectors = 1;
	cmd.buffer = buf;
/* make 3 attempts */
	for(tries = 3; tries != 0; tries--)
	{
		err = _bios_disk(_DISK_READ, &cmd);
		err >>= 8;
/* 0x11=="CRC/ECC corrected data error" */
		if(err == 0 || err == 0x11)
			return 0;
/* reset disk */
		_bios_disk(_DISK_RESET, &cmd);
	}
	DEBUG(printf("read_sector(): error 0x%02X\n", err);)
	return err;
}
/*****************************************************************************
*****************************************************************************/
int write_sector(disk_t *disk, unsigned long lba, unsigned char *buf)
{
	struct diskinfo_t cmd;
	unsigned tries, err;

	lba += disk->partition_start;
	cmd.drive = disk->drive_num;
/* use LBA if available */
	if(disk->use_lba)
	{
		return lba_biosdisk(_DISK_WRITE,
			disk->drive_num, lba, 1, buf);
	}
/* use CHS _bios_disk() */
	cmd.sector = (unsigned)(lba % disk->sectors + 1);
	cmd.head = (unsigned)((lba / disk->sectors) % disk->heads);
	cmd.track = (unsigned)((lba / disk->sectors) / disk->heads);
	cmd.nsectors = 1;
	cmd.buffer = buf;
/* make 3 attempts */
	for(tries = 3; tries != 0; tries--)
	{
		err = _bios_disk(_DISK_WRITE, &cmd);
		err >>= 8;
/* 0x11=="CRC/ECC corrected data error" */
		if(err == 0 || err == 0x11)
			return 0;
/* reset disk */
		_bios_disk(_DISK_RESET, &cmd);
	}
	DEBUG(printf("write_sector(): error 0x%02X\n", err);)
	return err;
}
/*****************************************************************************
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
/*****************************************************************************
*****************************************************************************/
static void probe_floppy_geometry(disk_t *disk)
{
	unsigned sectors, heads;
	unsigned char buf[BPS];

/* scan upwards for sector number where read fails */
	disk->sectors = 64 + 1;
	for(sectors = 1; sectors <= 64; sectors++)
	{
		if(read_sector(disk, sectors - 1, buf) != 0)
			break;
	}
	disk->sectors = sectors - 1;
#if 1
disk->heads = 2;
#else
/* scan upwards for head number where read fails
xxx - this probing for number of heads doesn't work */
	disk->heads = 16 + 1;
	for(heads = 1; heads < 16; heads++)
	{
		if(read_sector(disk, heads * disk->sectors, buf) != 0)
			break;
	}
	disk->heads = heads;
#endif
/* reset drive by reading sector 0 */
	(void)read_sector(disk, 0, buf);
	DEBUG(printf("probe_floppy_geometry() for fd 0x%02X: "
		"CHS=?:%u:%u\n", disk->drive_num,
		disk->heads, disk->sectors);)
}
/*****************************************************************************
*****************************************************************************/
int open_disk(disk_t *disk, unsigned drive_num)
{
	unsigned char buf[BPS];
	unsigned num_fds;
	int err;

	disk->drive_num = drive_num;
	disk->partition_start = 0; /* assume floppy */
	disk->use_lba = 0;	/* assume CHS */
/* hard disk */
	if(disk->drive_num >= 0x80)
		return get_hd_geometry(disk);
/* make sure floppy drive exists */
	num_fds = peekw(0x40, 0x10);
	if(num_fds & 0x0001)
		num_fds = ((num_fds / 64) & 3) + 1;
	else
		num_fds = 0;
	if(disk->drive_num >= num_fds)
	{
		printf("open_disk(): fd 0x%02X does not exist\n",
			disk->drive_num);
		return -1;
	}
/* temporary values to make read_sector(0) work */
	disk->heads = disk->sectors = 1;
	err = read_sector(disk, 0, buf);
	if(err != 0)
		return err;
/* if it's a FAT (DOS) disk, we get can reliable geometry info
from the BIOS parameter block (BPB) in the bootsector */
	if(is_fat_bootsector(buf))
	{
		disk->heads = LE16(buf + 26);
		disk->sectors = LE16(buf + 24);
		DEBUG(printf("open_disk() for fd 0x%02X: "
			"CHS=?:%u:%u (from BPB)\n",
			disk->drive_num,
			disk->heads, disk->sectors);)
		return 0;
	}
#if 0
/* ...otherwise, do slow probe */
	probe_floppy_geometry(disk);
#else
/* ...or just assume some values */
	disk->heads = 2;
	disk->sectors = 18;
	DEBUG(printf("open_disk() for fd 0x%02X: "
		"assuming CHS=?:2:18\n", disk->drive_num);)
#endif
	return 0;
}
