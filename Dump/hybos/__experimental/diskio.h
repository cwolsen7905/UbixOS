/*****************************************************************************
Sector-level disk I/O code for various OSes
This code is public domain (no copyright).
You can do whatever you want with it.

EXPORTS (API):
DEBUG(), BPS, LE16(), LE32(), disk_t,
int read_sector(disk_t *disk, unsigned long lba, unsigned char *buf);
int write_sector(disk_t *disk, unsigned long lba, unsigned char *buf);
int is_fat_bootsector(unsigned char *buf);
int open_disk(disk_t *disk, unsigned drive_num);
*****************************************************************************/
#ifndef __DISKIO_H
#define	__DISKIO_H

#ifdef __cplusplus
extern "C"
{
#endif

#if 0
#define	DEBUG(X)	X
#else
#define	DEBUG(X)	/* nothing */
#endif

#define	BPS		512 /* bytes/sector */

/* these assume little endian CPU like x86 */
#define	LE16(X)		*(uint16_t *)(X)
#define	LE32(X)		*(uint32_t *)(X)

/* STDINT.H
these assume sizeof(short)==2 and sizeof(long)==4 */
typedef unsigned short	uint16_t;
typedef unsigned long	uint32_t;

typedef struct
{
	unsigned char drive_num;
/* CHS disk geometry (heads and sectors are used only if use_lba==0) */
	unsigned use_lba : 1;
	unsigned char heads;
	unsigned char sectors;
/* LBA sector address of partition start (hard disk only) */
	unsigned long partition_start;
} disk_t;

/* these are in DISKIO.C */
int read_sector(disk_t *disk, unsigned long lba, unsigned char *buf);
int write_sector(disk_t *disk, unsigned long lba, unsigned char *buf);
int is_fat_bootsector(unsigned char *buf);
int open_disk(disk_t *disk, unsigned drive_num);

#ifdef __cplusplus
}
#endif

#endif
