/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpool — list the UbixFS storage pools mounted by the kernel (the zpool
 * analog).  Uses the uBixOS native API (int $0x81, ubix_pool_query / syscall
 * 67); pool management is OS-specific, so it does not go through the POSIX table.
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
		printf("no UbixFS pools mounted\n");
		return (0);
	}

	printf("%-16s %-12s %8s %8s %8s  %s\n", "NAME", "MOUNT", "SIZE", "ALLOC", "FREE", "SOURCE");
	for (i = 0; i < n; i++)
	{
		unsigned tot_k = (unsigned)((pools[i].total_blocks * pools[i].block_size) / 1024);
		unsigned free_k = (unsigned)((pools[i].free_blocks * pools[i].block_size) / 1024);
		printf("%-16s %-12s %7uK %7uK %7uK  %s,%s\n",
		       pools[i].pool,
		       pools[i].mountpoint,
		       tot_k,
		       tot_k - free_k,
		       free_k,
		       (pools[i].flags & UBIX_POOL_RAW) ? "raw" : "loopback",
		       (pools[i].flags & UBIX_POOL_RW) ? "rw" : "ro");
	}
	return (0);
}
