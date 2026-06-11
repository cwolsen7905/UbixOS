/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpool — list the UbixFS storage pools (the `zpool list` analog).  A pool is a
 * capacity container (SIZE/ALLOC/FREE/CAP/HEALTH); it has no mountpoint — only
 * its datasets are mounted (see `ubfs`).  Uses the uBixOS native API
 * (ubix_pool_query / syscall 67); pool management is OS-specific.
 */
#include <api/ubfs_pool.h>
#include <stdio.h>

#define MAX_POOLS 32

/* Format @bytes as a short human-readable size ("2.0M", "1.7G"), zpool-style. */
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
		printf("no UbixFS pools\n");
		return (0);
	}

	printf("%-12s %7s %7s %7s %4s  %s\n", "NAME", "SIZE", "ALLOC", "FREE", "CAP", "HEALTH");
	for (i = 0; i < n; i++)
	{
		unsigned long long size = (unsigned long long)pools[i].total_blocks * pools[i].block_size;
		unsigned long long freeb = (unsigned long long)pools[i].free_blocks * pools[i].block_size;
		unsigned long long alloc = size - freeb;
		unsigned cap = size ? (unsigned)(alloc * 100 / size) : 0;
		char sz[16], al[16], fr[16];

		hsize(size, sz);
		hsize(alloc, al);
		hsize(freeb, fr);
		printf("%-12s %7s %7s %7s %3u%%  ONLINE\n", pools[i].pool, sz, al, fr, cap);
	}
	return (0);
}
