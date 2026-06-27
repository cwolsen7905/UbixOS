/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubdisk — list block devices and their partitions (a `diskutil list` analog,
 * and the CLI proof of the Disk Utility data layer).  Uses the uBixOS native
 * API ubix_disk_query (syscall 68).  See docs/design/disk-utility-plan.md.
 */
#include <api/ubix_disk.h>
#include <stdio.h>
#include <string.h>

/* Format @sectors (512-byte) as a short human-readable size ("33M", "1.7G"). */
static void hsize(unsigned long long sectors, char *out, size_t outsz)
{
	static const char unit[] = "BKMGT";
	unsigned long long bytes = sectors * 512ULL;
	unsigned long long div = 1;
	int i = 0;

	while (bytes / div >= 1024 && i < 4)
	{
		div *= 1024;
		i++;
	}
	if (i == 0)
		snprintf(out, outsz, "%llu", bytes);
	else
		snprintf(out, outsz, "%llu.%llu%c", bytes / div, (bytes % div) * 10 / div, unit[i]);
}

/* A readable partition type from the MBR type byte. */
static const char *type_label(unsigned char t, const char *fstype)
{
	if (fstype != NULL && fstype[0] != '\0')
		return (fstype);
	switch (t)
	{
		case UBIX_PT_FAT32:
			return ("fat32");
		case UBIX_PT_SWAP:
			return ("swap");
		case UBIX_PT_UBPOOL:
			return ("ubixfs");
		default:
			return ("data");
	}
}

int main(int argc, char **argv)
{
	struct ubix_disk_info ents[UBIX_DISK_MAX];
	int n, i;

	(void)argc;
	(void)argv;

	n = ubix_disk_query(ents, UBIX_DISK_MAX);
	if (n <= 0)
	{
		printf("no block devices\n");
		return (0);
	}

	for (i = 0; i < n; i++)
	{
		struct ubix_disk_info *e = &ents[i];
		char sz[16];

		hsize(e->sectors, sz, sizeof(sz));

		if (e->kind == UBIX_DK_DISK)
		{
			printf("/dev/%s (%s%s%s):\n", e->name, sz, e->model[0] ? ", " : "", e->model);
			printf("   #  %-10s %-9s %-8s %-9s %s\n", "TYPE", "NAME", "SIZE", "START", "MOUNT");
			printf("   0  %-10s %-9s %-8s\n", "disk", e->name, sz);
		}
		else
		{
			printf("   %d  %-10s %-9s %-8s %-9llu %s\n",
			       e->minor,
			       type_label(e->mbr_type, e->fstype),
			       e->name,
			       sz,
			       (unsigned long long)e->lba_start,
			       (e->flags & UBIX_DF_MOUNTED) ? e->mountpoint : "-");
		}
	}
	return (0);
}
