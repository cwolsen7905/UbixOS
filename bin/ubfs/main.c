/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubfs — list the UbixFS datasets in the mounted pools (the zfs analog).  Uses
 * the uBixOS native API (ubix_pool_query / syscall 67).  v1 exposes one
 * filesystem dataset ("root") per pool; snapshots/child datasets come later.
 */
#include <api/ubfs_pool.h>
#include <stdio.h>

#define MAX_POOLS 32

int main(int argc, char **argv)
{
	struct ubix_pool_info pools[MAX_POOLS];
	int n, i;

	(void)argc;
	(void)argv;

	n = ubix_pool_query(pools, MAX_POOLS);
	if (n <= 0)
	{
		printf("no UbixFS datasets mounted\n");
		return (0);
	}

	printf("%-20s %-12s %10s  %s\n", "DATASET", "POOL", "USED", "MOUNTPOINT");
	for (i = 0; i < n; i++)
	{
		unsigned used_k = (unsigned)(pools[i].dataset_used / 1024);
		printf("%-20s %-12s %9uK  %s\n", pools[i].dataset, pools[i].pool, used_k, pools[i].mountpoint);
	}
	return (0);
}
