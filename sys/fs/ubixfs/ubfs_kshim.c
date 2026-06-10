/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * Kernel glue for the portable UbixFS core (lib/ubixfs_core).  The core is
 * written for a hosted environment and allocates with malloc()/free(); inside
 * the kernel those names resolve to these thin wrappers over the kernel heap
 * (kmalloc/kfree), so the core compiles and runs unmodified.  See the compat
 * shims in sys/fs/ubixfs/compat/ and docs/design/ubixfs-pool-plan.md (step K1).
 */
#include <sys/types.h>
#include <lib/kmalloc.h>

/**
 * Allocate @len bytes from the kernel heap (the core's malloc()).
 *
 * @return a pointer to the block, or NULL if @len is 0 or the heap is exhausted.
 */
void *malloc(size_t len)
{
	if (len == 0)
		return ((void *)0);
	return (kmalloc((u_int32_t)len));
}

/**
 * Release a block previously returned by malloc() (the core's free()).
 */
void free(void *ptr)
{
	if (ptr != (void *)0)
		kfree(ptr);
}
