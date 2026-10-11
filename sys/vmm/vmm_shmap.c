/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * MAP_SHARED write-back for the 64-bit ports (aarch64 + x86_64).
 *
 * File mappings are private frames filled from the file at mmap time; nothing
 * flushes them back on its own.  A MAP_SHARED + PROT_WRITE writer — notably
 * ld.lld's FileOutputBuffer, which mmaps the output file, fills it, then
 * munmaps to commit — would otherwise leave a zero-length or stale file.  Each
 * such mapping is recorded here at mmap time; munmap, msync and process exit
 * write its pages back through the VFS.
 *
 * The page lookup is the only arch-specific part: md_user_page_kva() turns a
 * user VA of the current task into a kernel-readable pointer (NULL if the page
 * is not mapped).  Bounded table; on overflow a mapping is simply not tracked.
 */

#include <sys/types.h>
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <vmm/vmm.h>
#include <vmm/vmm_shmap.h>
#include <fs/vfs/file.h>
#include <string.h>

struct vmm_shmap
{
	pidType pid;
	uintptr_t va;
	size_t len;
	off_t foff;
	char path[512];
	int active;
};

static struct vmm_shmap g_shmaps[32];
static struct spinLock g_shmap_lock = SPIN_LOCK_INITIALIZER;

void vmm_shmap_register(pidType pid, uintptr_t va, size_t len, off_t foff, const char *path)
{
	int i;

	if (path == NULL || path[0] == '\0' || len == 0)
		return;
	spinLock(&g_shmap_lock);
	for (i = 0; i < (int)(sizeof(g_shmaps) / sizeof(g_shmaps[0])); i++)
	{
		if (g_shmaps[i].active == 0)
		{
			g_shmaps[i].pid = pid;
			g_shmaps[i].va = va;
			g_shmaps[i].len = len;
			g_shmaps[i].foff = foff;
			strncpy(g_shmaps[i].path, path, sizeof(g_shmaps[i].path) - 1);
			g_shmaps[i].path[sizeof(g_shmaps[i].path) - 1] = '\0';
			g_shmaps[i].active = 1;
			break;
		}
	}
	spinUnlock(&g_shmap_lock);
}

/*
 * Write @len bytes of a mapping starting at user VA @va (file offset @foff) back
 * to @path.  Runs in the owning task's context, without g_shmap_lock held
 * (fopen/fwrite take vfs_io_lock).  Unmapped pages are skipped, leaving the
 * file's bytes as they were.
 */
static void shmap_write(const char *path, uintptr_t va, size_t len, off_t foff)
{
	fileDescriptor_t *fd;
	size_t done;

	fd = fopen(path, "r+");
	if (fd == NULL)
		return;
	for (done = 0; done < len; done += PAGE_SIZE)
	{
		size_t chunk = (len - done < PAGE_SIZE) ? (len - done) : PAGE_SIZE;
		void *kva = md_user_page_kva(va + done);

		fd->offset = foff + (off_t)done;
		if (kva != NULL)
			fwrite(kva, (int)chunk, 1, fd);
	}
	fclose(fd);
}

/* Write back (and, if @release, forget) every mapping of @pid overlapping [@lo, @hi),
 * or all of them if @whole. */
static void shmap_flush(pidType pid, uintptr_t lo, uintptr_t hi, int release, int whole)
{
	for (;;)
	{
		struct vmm_shmap m;
		int i, found = 0;

		spinLock(&g_shmap_lock);
		for (i = 0; i < (int)(sizeof(g_shmaps) / sizeof(g_shmaps[0])); i++)
		{
			struct vmm_shmap *e = &g_shmaps[i];

			if (e->active != 1 || e->pid != pid)
				continue;
			if (!whole && (e->va + e->len <= lo || e->va >= hi))
				continue;
			m = *e;
			if (release)
				e->active = 0;
			else
				e->active = 2; /* visited this pass (msync) */
			found = 1;
			break;
		}
		spinUnlock(&g_shmap_lock);
		if (!found)
			break;
		shmap_write(m.path, m.va, m.len, m.foff);
	}

	if (!release)
	{
		int i;

		spinLock(&g_shmap_lock);
		for (i = 0; i < (int)(sizeof(g_shmaps) / sizeof(g_shmaps[0])); i++)
			if (g_shmaps[i].active == 2)
				g_shmaps[i].active = 1;
		spinUnlock(&g_shmap_lock);
	}
}

void vmm_shmap_unmap(pidType pid, uintptr_t va, size_t len)
{
	shmap_flush(pid, va, va + (len ? len : 1), 1, 0);
}

void vmm_shmap_sync(pidType pid, uintptr_t va, size_t len)
{
	shmap_flush(pid, va, va + (len ? len : 1), 0, 0);
}

void vmm_shmap_exit(pidType pid)
{
	shmap_flush(pid, 0, 0, 1, 1);
}
