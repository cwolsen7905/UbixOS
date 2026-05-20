/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <fs/vfs/vfs.h>
#include <fs/vfs/file.h>
#include <sys/bus.h>
#include <ubixos/sched.h>
#include <ubixos/kpanic.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>

#include <fs/fat/fat_internal.h>
#include <fs/fat/fat_bpb.h>
#include <fs/fat/fat_dir.h>
#include <fs/fat/fat_file.h>

static volatile int	 fat_busy = 0;

static void
fat_acquire(void)
{
	while (__sync_lock_test_and_set(&fat_busy, 1))
		sched_yield();
}

static void
fat_release(void)
{
	__sync_lock_release(&fat_busy);
}

/* Retrieve the fat_fs for a mount point (stored in mp->fsInfo). */
static inline struct fat_fs *
fat_fs_from_mp(struct vfs_mountPoint *mp)
{
	return ((struct fat_fs *)mp->fsInfo);
}

/*
 * Strip a "mountpoint:" prefix if present.  The open/readdir paths already
 * receive stripped paths from the VFS layer, but unlink/mkdir/rmdir may not.
 */
static const char *
strip_prefix(const char *path)
{
	const char	*c = path;

	while (*c && *c != ':' && *c != '/')
		c++;
	if (*c == ':')
		return (c + 1);
	return (path);
}

/* ------------------------------------------------------------------ */
/* VFS callbacks                                                       */
/* ------------------------------------------------------------------ */

int
fat_initialize(struct vfs_mountPoint *mp)
{
	struct fat_fs *fs;

	fs = (struct fat_fs *)kmalloc(sizeof(*fs));
	if (fs == NULL) {
		kprintf("fat: kmalloc failed\n");
		return (0);
	}
	memset(fs, 0, sizeof(*fs));
	fs->mp   = mp;
	mp->fsInfo = fs;

	if (fat_bpb_parse(fs) != 0) {
		kprintf("fat: BPB parse failed\n");
		kfree(fs);
		mp->fsInfo = NULL;
		return (0);
	}
	kprintf("fat: mounted FAT%d  clusters=%u\n",
	    fs->type, fs->total_clusters);
	return (1);
}

int
open_fat(const char *path, fileDescriptor_t *fd)
{
	struct fat_fs	*fs = fat_fs_from_mp(fd->mp);
	uint8_t		 mode;
	struct fat_file	*f;

	if ((fd->mode & fileRead) && !(fd->mode & fileWrite))
		mode = FAT_MODE_R;
	else if (fd->mode & fileAppend)
		mode = FAT_MODE_A;
	else
		mode = FAT_MODE_W;

	fat_acquire();
	f = fat_file_open(fs, path, mode);
	fat_release();

	if (f == NULL)
		return (0);

	fd->res   = f;
	fd->size  = f->file_size;
	fd->ino   = f->start_cluster;
	fd->perms = 0x1;
	return (1);
}

int
close_fat(void *res)
{
	fat_acquire();
	fat_file_close((struct fat_file *)res);
	fat_release();
	return (0);
}

int
read_fat(fileDescriptor_t *fd, char *data, off_t offset, long size)
{
	struct fat_file	*f = (struct fat_file *)fd->res;
	uint32_t	 got = 0;

	(void)fat_fs_from_mp(fd->mp); /* assert mp->fsInfo is set */

	if (size <= 0)
		return (0);

	fat_acquire();
	fat_file_seek(f, (uint32_t)offset);
	fat_file_read(f, data, (uint32_t)size, &got);
	fat_release();

	return ((int)got);
}

int
write_fat(fileDescriptor_t *fd, char *data, off_t offset, long size)
{
	struct fat_file	*f = (struct fat_file *)fd->res;

	(void)fat_fs_from_mp(fd->mp); /* assert mp->fsInfo is set */

	if (size <= 0)
		return (0);

	fat_acquire();
	fat_file_seek(f, (uint32_t)offset);
	fat_file_write(f, data, (uint32_t)size);
	fat_file_flush(f);
	fat_release();

	/* Update fd->size to reflect any growth. */
	fd->size = f->file_size;

	return ((int)size);
}

int
fat_opendir(const char *path, kDIR_t *dir)
{
	struct fat_fs		*fs = fat_fs_from_mp(dir->mp);
	struct fat_dir_iter	*it;
	uint32_t		 cluster;

	it = (struct fat_dir_iter *)kmalloc(sizeof(struct fat_dir_iter));
	if (it == NULL)
		return (0);

	fat_acquire();
	if (fat_path_to_dir_cluster(fs, path, &cluster) != 0) {
		fat_release();
		kfree(it);
		return (0);
	}
	fat_dir_iter_open(fs, cluster, it);
	fat_release();

	dir->dirHandle = it;
	return (1);
}

int
fat_readdir(kDIR_t *dir, struct kdirent *ent)
{
	struct fat_dir_iter	*it = (struct fat_dir_iter *)dir->dirHandle;
	struct fat_raw_dirent	 raw;
	char			 name[256];
	uint32_t		 sec;
	uint16_t		 off;
	int			 r;

	fat_acquire();
	r = fat_dir_iter_next(it, name, &raw, &sec, &off);
	fat_release();

	if (r != 0)
		return (-1);

	ent->d_ino  = ((uint32_t)raw.clus_hi << 16) | raw.clus_lo;
	ent->d_type = (raw.attr & FAT_ATTR_DIR) ? KDT_DIR : KDT_REG;
	strncpy(ent->d_name, name, 255);
	ent->d_name[255] = '\0';
	return (0);
}

int
fat_closedir(kDIR_t *dir)
{
	if (dir->dirHandle != NULL) {
		kfree(dir->dirHandle);
		dir->dirHandle = NULL;
	}
	return (0);
}

int
mkdir_fat(char *path, void *vfd)
{
	struct fat_fs	*fs = fat_fs_from_mp(((fileDescriptor_t *)vfd)->mp);
	int		 r;

	fat_acquire();
	r = fat_dir_mkdir(fs, strip_prefix(path));
	fat_release();
	return (r);
}

int
rmdir_fat(char *path, void *vmp)
{
	struct fat_fs	*fs = fat_fs_from_mp((struct vfs_mountPoint *)vmp);
	int		 r;

	fat_acquire();
	r = fat_dir_rmdir(fs, strip_prefix(path));
	fat_release();
	return (r);
}

int
unlink_fat(char *path, void *vmp)
{
	struct fat_fs	*fs = fat_fs_from_mp((struct vfs_mountPoint *)vmp);
	int		 r;

	fat_acquire();
	r = fat_dir_unlink(fs, strip_prefix(path));
	fat_release();
	return (r);
}

/* ------------------------------------------------------------------ */
/* Registration                                                        */
/* ------------------------------------------------------------------ */

int
fat_init(void)
{
	struct fileSystem	 ubixFileSystem = {
		NULL,
		NULL,
		(void *)fat_initialize,
		(void *)read_fat,
		(void *)write_fat,
		(void *)open_fat,
		(void *)unlink_fat,
		(void *)mkdir_fat,
		(void *)rmdir_fat,
		NULL,		/* vfsSync */
		0xFA,		/* vfsType */
		(void *)fat_opendir,
		(void *)fat_readdir,
		(void *)fat_closedir,
		(void *)close_fat,	/* vfsClose */
	};

	if (vfsRegisterFS(ubixFileSystem) != 0x0) {
		kpanic("Unable To Enable FAT");
		return (1);
	}

	return (0);
}
