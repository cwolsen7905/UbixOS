/*****************************************************************************
Sector-level disk I/O code for DOS, using 16-bit compiler.
This code is public domain (no copyright).
You can do whatever you want with it.

EXPORTS:
int lba_biosdisk(int cmd, int drive, unsigned long lba,
		int nsects, void *buf);
int get_hd_geometry(disk_t *disk);
*****************************************************************************/
#include <string.h> /* memset() */
#include <stdio.h> /* printf() */
#include <bios.h> /* _DISK_... */
/* union REGS, struct SREGS, int86(), int86x() */
#include <dos.h> /* FP_SEG(), FP_OFF() */
#include "diskio.h"
#include "dos.h" /* peekb() */
/*****************************************************************************
*****************************************************************************/
int lba_biosdisk(int cmd, int drive, unsigned long lba, int nsects, void *buf)
{
	struct
	{
		unsigned char pkt_len;
		unsigned char res0;
		unsigned char nsects;
		unsigned char res1;
		unsigned short buf_off;
		unsigned short buf_seg;
		unsigned long lba31_0;
		unsigned long lba63_32;
	} lba_cmd_pkt;
	unsigned tries, err = 0;
	struct SREGS sregs;
	union REGS regs;

	if(cmd != _DISK_READ && cmd != _DISK_WRITE)
		return 0x100;
/* make sure drive and BIOS support LBA */
	regs.x.bx = 0x55AA;
	regs.h.dl = drive;
	regs.h.ah = 0x41;
	int86x(0x13, &regs, &regs, &sregs);
	if(regs.x.cflag)
		return 0x100;
/* fill out the INT 13h AH=4xh command packet */
	memset(&lba_cmd_pkt, 0, sizeof(lba_cmd_pkt));
	lba_cmd_pkt.pkt_len = sizeof(lba_cmd_pkt);
	lba_cmd_pkt.nsects = nsects;
	lba_cmd_pkt.buf_off = FP_OFF(buf);
	lba_cmd_pkt.buf_seg = FP_SEG(buf);
	lba_cmd_pkt.lba31_0 = lba;
/* fill out registers for INT 13h AH=4xh */
	sregs.ds = FP_SEG(&lba_cmd_pkt);
	regs.x.si = FP_OFF(&lba_cmd_pkt);
	regs.h.dl = drive;
/* make 3 attempts */
	for(tries = 3; tries != 0; tries--)
	{
		regs.h.ah = (cmd == _DISK_READ) ? 0x42 : 0x43;
		int86x(0x13, &regs, &regs, &sregs);
		err = regs.h.ah;
		if(!regs.x.cflag)
			return 0;
/* reset disk */
		regs.h.ah = _DISK_RESET;
		int86x(0x13, &regs, &regs, &sregs);
	}
	DEBUG(printf("lba_biosdisk(): error 0x%02X\n", err);)
	return err;
}
/*****************************************************************************
*****************************************************************************/
int get_hd_geometry(disk_t *disk)
{
	union REGS regs;

/* make sure hard drive exists */
	if(disk->drive_num - 0x80 >= peekb(0x40, 0x75))
	{
		printf("get_hd_geometry(): hd 0x%02X does not exist\n",
			disk->drive_num);
		return -1;
	}
/* use LBA if drive and BIOS support it */
	regs.h.ah = 0x41;
	regs.x.bx = 0x55AA;
	regs.h.dl = disk->drive_num;
	int86(0x13, &regs, &regs);
	if(!regs.x.cflag && regs.x.bx == 0xAA55)
	{
		disk->use_lba = 1;
		DEBUG(printf("get_hd_geometry(): using LBA for hd 0x%02X\n",
			disk->drive_num);)
		return 0;
	}
/* get geometry from BIOS */
	regs.h.ah = 0x08;
	regs.h.dl = disk->drive_num;
	int86(0x13, &regs, &regs);
	if(regs.x.cflag)
	{
		printf("get_hd_geometry(): error getting geometry "
			"for hard drive 0x%02X\n", disk->drive_num);
		return -1;
	}
	disk->heads = regs.h.dh + 1;
	disk->sectors = regs.h.cl & 0x3F;
	DEBUG(printf("get_hd_geometry() for hd 0x%02X: "
		"CHS=?:%u:%u (from INT 13h AH=08h)\n",
		disk->drive_num,
		disk->heads, disk->sectors);)
	return 0;
}
