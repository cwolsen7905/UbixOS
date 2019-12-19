#ifndef _FAT_H
#define _FAT_H

typedef unsigned char	uint8_t;
typedef unsigned short	uint16_t;
typedef unsigned long	uint32_t;

/**
 * FAT12 and FAT16 bootsector
 */
typedef struct __FAT_BOOTSECTOR	/* MUST be packed */
{
	uint8_t	jump[3];					/* 16-bit JMP to boot code, or 8-bit JMP + NOP */
	uint8_t	oem_id[8];				/* e.g., 'MSWIN4.0' */
	uint16_t	bytes_per_sector;		/* typically 512 */
	uint8_t	sectors_per_cluster;
	uint16_t	num_boot_sectors;		/* typically 1 */
	uint8_t	num_fats;				/* typically 2 */
	uint16_t	num_root_dir_ents;
	uint16_t	total_sectors;			/* 16-bit; 0 if num sectors > 65535 */
	uint8_t	media_ID_byte;			/* typically 0F0h */
	uint16_t	sectors_per_fat;
	uint16_t	sectors_per_track;
	uint16_t	heads;
	uint32_t	hidden_sectors;		/* LBA partition start */
	uint32_t	total_sectors_large;	/* 32-bit; 0 if num sectors < 65536 */
	uint8_t	boot_code[474];
	uint8_t	magic[2];				/* 55h, 0AAh */
} FAT_BOOTSECTOR;						/* 512 bytes total */

/**
 * FAT directory entries
 *
 * As far as I know, this should be valid for FAT12/16/32
 * Entries are 32 bytes long
 */
typedef struct __FAT_DIRSTRUCT	/* MUST be packed */
{
	uint8_t	name[8];					/* all caps, pad right with spaces */
	uint8_t	ext[3];					/* all caps, pad right with spaces */
	uint8_t	attrib;					/* attribute byte */
	uint8_t	reserved;				/* 0 */
	uint8_t	ctime_ms;				/* file creation time, 10ms units */
	uint16_t	ctime;					/* file creation time, in DOS format */
	uint16_t	cdate;					/* file creation date, in DOS format */
	uint16_t	adate;					/* DOS date of last file access */
	uint16_t	st_clust_msw;			/* high 16 bits of starting cluster (FAT32) */
	uint16_t	mtime;					/* DOS time of last file modification */
	uint16_t	mdate;					/* DOS date of last file modification */
	uint16_t	st_clust;				/* starting cluster */
	uint32_t	file_size;				/* in bytes */
} FAT_DIRSTRUCT;						/* 32 bytes total */

/**
 * DOS time and date structs
 */
typedef struct __DOS_TIME			/* MUST be packed */
{
	unsigned two_secs	: 5;			/* low 5 bits: 2-second increments */
	unsigned minutes	: 6;			/* middle 6 bits: minutes */
	unsigned hours		: 5;			/* high 5 bits: hours (0-23) */
} DOS_TIME;								/* 2 bytes total */

typedef struct __DOS_DATE			/* MUST be packed */
{
	unsigned date		: 5;			/* low 5 bits: date (1-31) */
	unsigned month		: 4;			/* middle 4 bits: month (1-12) */
	unsigned year		: 7;			/* high 7 bits: year - 1980 */
} DOS_DATE;								/* 2 bytes total */

/**
 * DOS File attributes
 */
typedef struct __DOS_ATTRIB		/* MUST be packed */
{
	int read_only		: 1;			/* b0 */
	int hidden			: 1;
	int system			: 1;
	int volume_label	: 1;
	int directory		: 1;
	int reserved		: 2;			/* b6, b7 */
} DOS_ATTRIB;							/* 1 byte total */

#endif /* ! _FAT_H */
