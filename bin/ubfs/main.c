/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubfs — list the UbixFS datasets (the `zfs list` analog).  Datasets are what is
 * mounted (a pool has no mountpoint).  Following ZFS, the pool's top-level
 * filesystem dataset carries the pool's name (pool "tank" -> dataset "tank"
 * mounted at its mountpoint); child datasets read "pool/child".  Uses the uBixOS
 * native API (ubix_pool_query / syscall 67).  v1 exposes one filesystem dataset
 * per pool; snapshots/children come later.
 */
#include <api/ubfs_pool.h>
#include <stdio.h>
#include <string.h>

#define MAX_POOLS 32

/* Format @bytes as a short human-readable size ("2.0M", "1.7G"), zfs-style. */
static void hsize(unsigned long long bytes, char *out)
{
	static const char unit[] = "BKMGT";
	unsigned long long div = 1;
	int i = 0;

	while (bytes / div >= 1024 && i < 4)
	{
		div *= 1024;
		i++;
	}
	sprintf(out, "%u.%u%c", (unsigned)(bytes / div), (unsigned)((bytes % div) * 10 / div), unit[i]);
}

int main(int argc, char **argv)
{
	struct ubix_pool_info pools[MAX_POOLS];
	int n, i;

	(void)argc;
	(void)argv;

	n = ubix_pool_query(pools, MAX_POOLS);
	if (n <= 0)
	{
		printf("no UbixFS datasets\n");
		return (0);
	}

	printf("%-16s %8s %8s %8s  %s\n", "NAME", "USED", "AVAIL", "REFER", "MOUNTPOINT");
	for (i = 0; i < n; i++)
	{
		unsigned long long avail = (unsigned long long)pools[i].free_blocks * pools[i].block_size;
		char name[80], used[16], av[16];

		/* The pool's root filesystem dataset is named after the pool (ZFS
		 * convention); a future child dataset would read "pool/child". */
		if (strcmp(pools[i].dataset, "root") == 0)
			snprintf(name, sizeof(name), "%s", pools[i].pool);
		else
			snprintf(name, sizeof(name), "%s/%s", pools[i].pool, pools[i].dataset);

		hsize(pools[i].dataset_used, used);
		hsize(avail, av);
		/* REFER == USED for a leaf dataset with no snapshots/children. */
		printf("%-16s %8s %8s %8s  %s\n", name, used, av, used, pools[i].mountpoint);
	}
	return (0);
}
