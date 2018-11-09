/*****************************************************************************
Sector-level disk I/O code for various OSes
This code is public domain (no copyright).
You can do whatever you want with it.
*****************************************************************************/
#include <stdio.h> /* printf(), putchar() */
#include "diskio.h"
/*****************************************************************************
*****************************************************************************/
#define BPERL		16	/* byte/line for dump */

static void dump(unsigned char *data, unsigned count)
{
	unsigned char byte1, byte2;

	while(count != 0)
	{
		for(byte1 = 0; byte1 < BPERL; byte1++)
		{
			if(count == 0)
				break;
			printf("%02X ", data[byte1]);
			count--;
		}
		printf("\t");
		for(byte2 = 0; byte2 < byte1; byte2++)
		{
			if(data[byte2] < ' ')
				putchar('.');
			else
				putchar(data[byte2]);
		}
		printf("\n");
		data += BPERL;
	}
}
/*****************************************************************************
*****************************************************************************/
int main(void)
{
	unsigned char buf[512], *ptab_rec;
	unsigned i = 0;
	disk_t disk;

	printf("Looking for FAT (DOS) disk or partition...\n");
/* check if floppy in A: drive */
	if(open_disk(&disk, 0) == 0)
	{
/* read bootsector, check if FAT */
		if(read_sector(&disk, 0, buf) == 0)
		{
			if(is_fat_bootsector(buf))
				goto OK;
		}
	}
/* scan hard drives for FAT partition */
	for(i = 0x80; i < 0x82; i++)
	{
		if(open_disk(&disk, i) != 0)
			continue;
/* read MBR */
		if(read_sector(&disk, 0, buf) != 0)
			continue;
/* find FAT partition */
		for(i = 0; i < 4; i++)
		{
			ptab_rec = buf + 446 + 16 * i;
/* make sure it's FAT */
			if(ptab_rec[4] == 0x01 ||	/* FAT 12 */
				ptab_rec[4] == 0x04 ||	/* FAT 16 <32meg */
				ptab_rec[4] == 0x06 ||	/* FAT 16 >=32meg */
				ptab_rec[4] == 0x0E)	/* LBA type 0x06 */
			{
/* xxx - note if FAT16 or FAT12 */
				disk.partition_start = LE32(ptab_rec + 8);
				goto OK;
			}
		}
	}
	printf("No FAT partitions found on any disk\n");
	return 1;
OK:
	if(disk.drive_num >= 0x80)
		printf("Partition #%u on ", i);
	printf("INT 13h disk number 0x%02X:\n", disk.drive_num);
	if(read_sector(&disk, 0, buf) != 0)
		printf("Error reading bootsector\n");
	else
	{
		printf("Hex dump of bootsector:\n");
		dump(buf, 64);
	}
	return 0;
}
