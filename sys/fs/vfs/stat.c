/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ubixos/errno.h>
#include <sys/sysproto.h>
#include <sys/sysproto_posix.h>
#include <fs/vfs/stat.h>
#include <fs/vfs/file.h>
#include <fs/vfs/vfs.h>
#include <lib/kprintf.h>
#include <sys/descrip.h>
#include <string.h>
#include <ubixos/uaccess.h>
#include <lib/kmalloc.h>
#include <fs/vfs/mount.h>

/**
 * Report regular files as executable.
 *
 * uBixOS has no per-file permission enforcement yet (sys_access is permissive),
 * and the image tools store world binaries on the UbixFS pool as 0644 — so a
 * mode-bit-checking exec (e.g. oksh's access()+stat X_OK pre-check) refuses to
 * run them.  Add the execute bits to regular files so they can be exec'd,
 * matching the FAT path (which already reports 0755).  Stopgap until the image
 * tools store real exec bits / the multi-user security model lands.
 */
static u_int32_t stat_exec_fixup(u_int32_t mode)
{
	if ((mode & 0170000) == 0100000) /* S_IFREG */
		mode |= 0111;
	return (mode);
}

/*
 * Finalize a regular file's mode for stat, given the descriptor it was opened
 * on.  The exec-fixup (force 0111 on every regular file) is a workaround for
 * filesystems that store NO mode bits — FAT — so their binaries stay runnable
 * and access(X_OK) succeeds.  The UbixFS pool DOES store real modes, so applying
 * the fixup there wrongly reported every data file as executable (0644 -> 0755,
 * so `ls` tagged Makefile/*.c with a '*').  Per backing FS:
 *   - procfs : synthetic read-only text -> 0444 (and never '*'-tagged).
 *   - UbixFS : real on-disk modes -> report verbatim (no fixup).
 *   - FAT / other modeless FSes -> exec-fixup so binaries remain runnable.
 */
static u_int32_t stat_mode_for_fd(const fileDescriptor_t *fd, u_int32_t mode)
{
	if (fd != NULL && fd->mp != NULL && fd->mp->fs != NULL)
	{
		if (fd->mp->fs->vfsType == VFS_TYPE_PROCFS)
			return (0100000u | 0444u); /* S_IFREG | r--r--r-- */
		if (fd->mp->fs->vfsType == VFS_TYPE_UBIXFS)
			return (mode); /* real on-disk mode bits — do not clobber */
	}
	return (stat_exec_fixup(mode));
}

int _sys_stat(char *path, struct stat *sb, int flags)
{
	int error = 0;
	// struct inode *inode;

	// MrOlsen kprintf("SYS_STAT {%s}", path);

	/*
	 switch (flags) {
	 case STAT_LSTAT:
	 error = namei(path, NULL, STAT_NO_FOLLOW, &inode);
	 sb->st_dev = inode->i_dev;
	 sb->st_ino = inode->i_ino;
	 sb->st_mode = inode->i_mode;
	 sb->st_nlink = inode->i_nlink;
	 sb->st_uid = inode->i_uid;
	 sb->st_gid = inode->i_gid;
	 sb->st_rdev = inode->i_rdev;
	 sb->st_size = inode->i_size;
	 sb->st_atime = inode->i_atime;
	 sb->st_mtime = inode->i_mtime;
	 sb->st_ctime = inode->i_ctime;
	 //MrOlsen kprintf("LSTAT(%i)=st_ino 0x%X, st_mode=0x%X, st_uid %i, st_gid %i, st_size=0x%X", error, sb->st_ino,
	 sb->st_mode, sb->st_uid, sb->st_gid, sb->st_size); break; default: kprintf("STAT ERROR={%s}", path); error =
	 -1; break;
	 }
	 */

	fileDescriptor_t *fd = fopen(path, "rb");
	// MrOlsen kprintf("FD=0x%X", fd);

	if (fd == 0)
	{
		/*
		 * fopen() rejects directories, so a failed open does not mean the
		 * path is absent — it may be a directory.  Probe with vfs_opendir()
		 * and, if it is a directory, report it as one (S_IFDIR).  Without
		 * this, stat() on any directory returned -1, which broke callers
		 * that gate on S_ISDIR (e.g. NetSurf's filepath_generate() dropped
		 * every resource directory, so no Messages/CSS/fonts were found).
		 */
		kDIR_t *dir = vfs_opendir(path);
		if (dir == 0x0)
		{
			error = -ENOENT; /* not a blanket -1 (musl decodes -1 as EPERM) */
		}
		else
		{
			vfs_closedir(dir);
			sb->st_dev = 0xDEADBEEF;
			sb->st_ino = 1;
			sb->st_mode = 0040755; /* S_IFDIR | 0755 */
			sb->st_nlink = 2;
			sb->st_uid = 0;
			sb->st_gid = 0;
			sb->st_rdev = 0xBEEFDEAD;
			sb->st_size = 0;
			sb->st_atime = 0;
			sb->st_mtime = 0;
			sb->st_ctime = 0;
		}
	}
	else
	{
		sb->st_dev = 0xDEADBEEF;
		sb->st_ino = fd->ino;
		sb->st_mode = fd->inode.u.ufs2_i.di_mode;
		sb->st_nlink = fd->inode.u.ufs2_i.di_nlink;
		sb->st_uid = fd->inode.u.ufs2_i.di_uid;
		sb->st_gid = fd->inode.u.ufs2_i.di_gid;
		sb->st_rdev = 0xBEEFDEAD;
		sb->st_size = fd->inode.u.ufs2_i.di_size;
		sb->st_atime = fd->inode.u.ufs2_i.di_atime;
		sb->st_mtime = fd->inode.u.ufs2_i.di_mtime;
		sb->st_ctime = fd->inode.u.ufs2_i.di_ctime;

		/* FAT stores no mode bits; fall back to a regular-file mode. */
		if (sb->st_mode == 0)
			sb->st_mode = 0x81ED; /* S_IFREG | 0755 */
		sb->st_mode = stat_mode_for_fd(fd, sb->st_mode);

		fclose(fd);
	}

	return (error);
}

int sys_fstat(struct thread *td, struct sys_fstat_args *args)
{
	struct file *fdd = 0x0;
	fileDescriptor_t *fd = 0x0;
	struct stat *sb = args->sb;

	getfd(td, &fdd, args->fd);

	if (fdd == 0x0 || sb == 0x0)
	{
		td->td_retval[0] = -EBADF;
		return (EBADF);
	}

	memset(sb, 0, sizeof(*sb));
	sb->st_dev = 0xDEADBEEF;
	sb->st_rdev = 0xBEEFDEAD;
	sb->st_nlink = 1;

	/* No underlying file: a pipe, socket or terminal.  These used to fail with
	 * EBADF, so anything that fstat()s its input (tail, cat -, sort) refused to
	 * read from a pipe. */
	if (fdd->fd == 0x0)
	{
		switch (fdd->fd_type)
		{
			case FD_TYPE_PIPE:
				sb->st_mode = 0010600; /* S_IFIFO | rw------- */
				break;
			case FD_TYPE_SOCKET:
				sb->st_mode = 0140777; /* S_IFSOCK */
				break;
			case FD_TYPE_DIR:
				sb->st_mode = 0040755; /* S_IFDIR */
				sb->st_nlink = 2;
				break;
			default:
				sb->st_mode = 0020620; /* S_IFCHR | rw--w---- : a terminal */
				break;
		}
		td->td_retval[0] = 0;
		return (0);
	}

	fd = fdd->fd;
	sb->st_ino = fd->ino;

	if (fd->res != 0x0)
	{
		/* devfs, FAT and ramfs files: no st_mode was set here before, so the
		 * caller got whatever its buffer held. */
		sb->st_size = fd->size;
		if (fd->mp != NULL && fd->mp->fs != NULL && fd->mp->fs->vfsType == VFS_TYPE_DEVFS)
			sb->st_mode = 0020666; /* S_IFCHR | rw-rw-rw- */
		else
			sb->st_mode = stat_mode_for_fd(fd, 0100644); /* S_IFREG */
	}
	else
	{
		sb->st_mode = fd->inode.u.ufs2_i.di_mode;
		sb->st_nlink = fd->inode.u.ufs2_i.di_nlink ? fd->inode.u.ufs2_i.di_nlink : 1;
		sb->st_uid = fd->inode.u.ufs2_i.di_uid;
		sb->st_gid = fd->inode.u.ufs2_i.di_gid;
		sb->st_size = fd->inode.u.ufs2_i.di_size;
		sb->st_atime = fd->inode.u.ufs2_i.di_atime;
		sb->st_mtime = fd->inode.u.ufs2_i.di_mtime;
		sb->st_ctime = fd->inode.u.ufs2_i.di_ctime;
		if (sb->st_mode == 0)
			sb->st_mode = 0100644; /* S_IFREG when the filesystem stores none */
		sb->st_mode = stat_mode_for_fd(fd, sb->st_mode);
	}

	td->td_retval[0] = 0;
	return (0);
}

int sys_fstatat(struct thread *td, struct sys_fstatat_args *args)
{
	int error = 0;

	struct file *fdd = 0x0;
	fileDescriptor_t *fd = 0x0;
	struct stat *sb = 0x0;
	int uP = 0x0;

	// kprintf("FSTATAT(%i:%s:%s)", args->fd, args->path, args->flag);

	if (strlen(args->path) > 0)
	{
		fd = fopen(args->path, "rb");
		uP = 1;
	}
	else
	{
		getfd(td, &fdd, args->fd);
		if (fdd == 0 || fdd->fd == 0x0)
		{
			error = -EBADF;
		}
		else
		{
			fd = fdd->fd;
		}
	}

	if (fd == 0x0)
	{
		/* Path case: fopen failed -> missing (-ENOENT).  fd case already set
		 * -EBADF above.  Either way, not a blanket -1 (musl decodes -1 as EPERM). */
		if (error == 0)
			error = -ENOENT;
	}
	else
	{
		sb = args->buf;
		sb->st_dev = 0xDEADBEEF;
		sb->st_ino = fd->ino;
		sb->st_mode = fd->inode.u.ufs2_i.di_mode;
		sb->st_nlink = fd->inode.u.ufs2_i.di_nlink;
		sb->st_uid = fd->inode.u.ufs2_i.di_uid;
		sb->st_gid = fd->inode.u.ufs2_i.di_gid;
		sb->st_rdev = 0xBEEFDEAD;
		sb->st_size = fd->inode.u.ufs2_i.di_size;
		sb->st_atime = fd->inode.u.ufs2_i.di_atime;
		sb->st_mtime = fd->inode.u.ufs2_i.di_mtime;
		sb->st_ctime = fd->inode.u.ufs2_i.di_ctime;

		if (sb->st_mode == 0)
			sb->st_mode = 0x81ED; /* S_IFREG | 0755 */
		sb->st_mode = stat_mode_for_fd(fd, sb->st_mode);
		if (sb->st_nlink == 0)
			sb->st_nlink = 1;
		if (sb->st_size == 0 && fd->size != 0)
			sb->st_size = (off_t)fd->size;

		if (uP == 1)
			fclose(fd);
	}

	td->td_retval[0] = error;
	return (error);
}

/* Longest path sys_statfs accepts from userland, terminator included. */
#define STATFS_PATH_MAX 1024

/**
 * Fill @sf with what UbixOS reports for any mounted filesystem.  There is no
 * per-mount accounting yet, so the sizes are fixed placeholders (large and
 * mostly free, so space checks pass) under the FreeBSD statfs layout.
 */
static void statfs_fill(struct statfs *sf)
{
	memset(sf, 0, sizeof(*sf));
	sf->f_version = 0x20030518; /* STATFS_VERSION */
	sf->f_type = 0x35;
	sf->f_bsize = 4096;
	sf->f_iosize = 32768;
	sf->f_blocks = 0x7bf9d;
	sf->f_bfree = 0x73c0c;
	sf->f_bavail = 0x69d5c;
	sf->f_files = 0x3fffe;
	sf->f_ffree = 0x3d3f0;
	sf->f_namemax = 255;
	sf->f_fsid.val[0] = 0x5A31F4F0;
	sf->f_fsid.val[1] = 0x65E7C98F;
	strncpy(sf->f_fstypename, "ubixfs", sizeof(sf->f_fstypename) - 1);
	strncpy(sf->f_mntfromname, "ubixfs", sizeof(sf->f_mntfromname) - 1);
	strncpy(sf->f_mntonname, "/", sizeof(sf->f_mntonname) - 1);
}

/**
 * Fill a struct statfs and copy it out to the user address @ubuf.  The struct
 * is ~500 bytes, so it lives on the heap (kernel-rules K-10).
 *
 * @return 0, -ENOMEM or -EFAULT.
 */
static int statfs_copyout(struct statfs *ubuf)
{
	struct statfs *sf = kmalloc(sizeof(*sf));
	int err;

	if (sf == NULL)
		return (-ENOMEM);
	statfs_fill(sf);
	err = copyout(sf, ubuf, sizeof(*sf));
	kfree(sf);
	return (err);
}

/**
 * sys_fstatfs - filesystem statistics for an open descriptor (FreeBSD 397)
 * @td: thread state for the calling task
 * @args: fd and the user's struct statfs
 *
 * @return 0; td_retval[0] is 0, -EBADF, -ENOMEM or -EFAULT.
 */
int sys_fstatfs(struct thread *td, struct sys_fstatfs_args *args)
{
	struct file *fd = NULL;
	int err;

	getfd(td, &fd, args->fd);
	err = (fd == NULL) ? -EBADF : statfs_copyout(args->buf);
	td->td_retval[0] = err;
	return (-err);
}

/**
 * sys_statfs - filesystem statistics for a path (FreeBSD 396)
 * @td: thread state for the calling task
 * @args: path (file or directory) and the user's struct statfs
 *
 * @return 0; td_retval[0] is 0, -ENOENT, -ENAMETOOLONG, -ENOMEM or -EFAULT.
 */
int sys_statfs(struct thread *td, struct sys_statfs_args *args)
{
	char *path = kmalloc(STATFS_PATH_MAX);
	fileDescriptor_t *fp;
	kDIR_t *dir;
	int err;

	if (path == NULL)
	{
		td->td_retval[0] = -ENOMEM;
		return (ENOMEM);
	}

	err = copyinstr(args->path, path, STATFS_PATH_MAX);
	if (err < 0)
	{
		/* Re-express in this file's errno numbering: the kernel's two errno
		 * headers disagree on values above EAGAIN. */
		err = (err == -EFAULT) ? -EFAULT : -ENAMETOOLONG;
	}
	else
	{
		err = -ENOENT;
		fp = fopen(path, "r");
		if (fp != NULL)
		{
			fclose(fp);
			err = 0;
		}
		else if ((dir = vfs_opendir(path)) != NULL)
		{
			vfs_closedir(dir);
			err = 0;
		}
	}
	kfree(path);

	if (err == 0)
		err = statfs_copyout(args->buf);
	td->td_retval[0] = err;
	return (-err);
}

/* Return stat of path do not follow if link return stat of link */
int sys_lstat(struct thread *td, struct sys_lstat_args *args)
{
	td->td_retval[0] = _sys_stat(args->path, args->sb, STAT_LSTAT);
	return (0x0);
}

int sys_stat(struct thread *td, struct sys_stat_args *args)
{
	td->td_retval[0] = _sys_stat(args->path, args->ub, STAT_LSTAT);
	return (0x0);
}

/**
 * Copy the inode timestamps (seconds) from a fileDescriptor into a statx result.
 *
 * Filesystems that populate fd->inode (UbixFS via ubfs_vfs_open; FAT via
 * open_fat) get real atime/mtime/ctime here; those that leave it zero report the
 * epoch, as before.  birthtime is reported as ctime (the ufs2 dinode the VFS
 * uses carries no separate birth field).
 */
static void statx_fill_times(struct statx *stx, const fileDescriptor_t *fd)
{
	stx->stx_atime.tv_sec = (int64_t)fd->inode.u.ufs2_i.di_atime;
	stx->stx_mtime.tv_sec = (int64_t)fd->inode.u.ufs2_i.di_mtime;
	stx->stx_ctime.tv_sec = (int64_t)fd->inode.u.ufs2_i.di_ctime;
	stx->stx_btime.tv_sec = (int64_t)fd->inode.u.ufs2_i.di_ctime;
}

/*
 * sys_statx — Linux statx(2) compatibility (slot 383).
 *
 * musl uses statx as the backing for fstat/stat.  We handle the two
 * common call patterns:
 *   AT_EMPTY_PATH set, path==""  → fstat(dirfd)
 *   AT_FDCWD or path given       → stat(path)
 *
 * The struct statx is zeroed first so musl's conversion to struct stat
 * sees consistent values for fields we don't populate.
 */
int sys_statx(struct thread *td, struct sys_statx_args *args)
{
	struct statx *stx = args->stx;
	int error = 0;

	if (stx == 0x0)
	{
		td->td_retval[0] = -EFAULT; /* negative: musl reads +errno as a success value */
		return (EFAULT);
	}

	memset(stx, 0, sizeof(*stx));

	if ((args->flags & AT_EMPTY_PATH) && (args->path == 0x0 || args->path[0] == '\0'))
	{
		/* fstat(dirfd) path */
		struct file *fdd = 0x0;
		fileDescriptor_t *fd = 0x0;

		getfd(td, &fdd, args->dirfd);
		if (fdd == 0x0)
		{
			td->td_retval[0] = -EBADF; /* negative: musl reads +errno as a success value */
			return (EBADF);
		}

		/* No underlying file: a terminal, pipe, socket or directory.  Report
		 * the real type (pipes used to come back as character devices). */
		if (fdd->fd == 0x0)
		{
			stx->stx_mask = args->mask & STATX_BASIC_STATS;
			stx->stx_blksize = 512;
			stx->stx_nlink = 1;
			stx->stx_uid = 0;
			stx->stx_gid = 0;
			switch (fdd->fd_type)
			{
				case FD_TYPE_PIPE:
					stx->stx_mode = 0010600; /* S_IFIFO | 0600 */
					break;
				case FD_TYPE_SOCKET:
					stx->stx_mode = 0140777; /* S_IFSOCK */
					break;
				case FD_TYPE_DIR:
					stx->stx_mode = 0040755; /* S_IFDIR */
					break;
				default:
					stx->stx_mode = 0020620; /* S_IFCHR | 0620: a terminal */
					break;
			}
			stx->stx_ino = (u_int64_t)(uintptr_t)args->dirfd + 1;
			stx->stx_size = 0;
			stx->stx_blocks = 0;
			stx->stx_dev_major = 5;
			stx->stx_dev_minor = 0;
			stx->stx_rdev_major = 5;
			stx->stx_rdev_minor = 0;
			td->td_retval[0] = 0;
			return (0);
		}
		fd = fdd->fd;

		stx->stx_mask = args->mask & STATX_BASIC_STATS;
		stx->stx_blksize = 512;
		stx->stx_nlink = fd->inode.u.ufs2_i.di_nlink ? fd->inode.u.ufs2_i.di_nlink : 1;
		stx->stx_uid = fd->inode.u.ufs2_i.di_uid;
		stx->stx_gid = fd->inode.u.ufs2_i.di_gid;
		/* FAT driver sets di_mode = S_IFREG|0755 in open_fat(); use it directly. */
		stx->stx_mode = fd->inode.u.ufs2_i.di_mode;
		if ((stx->stx_mode & 0170000) == 0)
			stx->stx_mode |= 0100755;                    /* fallback for FSes that don't set di_mode */
		stx->stx_mode = stat_mode_for_fd(fd, stx->stx_mode); /* FS-aware: no exec-fixup on UbixFS */
		stx->stx_ino = fd->ino;
		stx->stx_size = fd->size;
		stx->stx_blocks = (fd->size + 511) / 512;
		stx->stx_dev_major = 1;
		stx->stx_dev_minor = 1;
		statx_fill_times(stx, fd);
	}
	else
	{
		/* stat(path) path */
		const char *path = args->path;
		fileDescriptor_t *fd = 0x0;
		kDIR_t *dir = 0x0;

		if (path == 0x0 || path[0] == '\0')
		{
			td->td_retval[0] = -ENOENT;
			return (ENOENT);
		}

		fd = fopen(path, "rb");
		if (fd == 0x0)
		{
			/* fopen rejects directories; try vfs_opendir to detect them. */
			dir = vfs_opendir(path);
			if (dir == 0x0)
			{
				td->td_retval[0] = -ENOENT;
				return (ENOENT);
			}
			vfs_closedir(dir);

			stx->stx_mask = args->mask & STATX_BASIC_STATS;
			stx->stx_blksize = 512;
			stx->stx_nlink = 2;
			stx->stx_uid = 0;
			stx->stx_gid = 0;
			stx->stx_mode = 0040755; /* S_IFDIR | 0755 */
			stx->stx_ino = 1;
			stx->stx_size = 0;
			stx->stx_blocks = 0;
			stx->stx_dev_major = 1;
			stx->stx_dev_minor = 1;
		}
		else
		{
			stx->stx_mask = args->mask & STATX_BASIC_STATS;
			stx->stx_blksize = 512;
			stx->stx_nlink = fd->inode.u.ufs2_i.di_nlink ? fd->inode.u.ufs2_i.di_nlink : 1;
			stx->stx_uid = fd->inode.u.ufs2_i.di_uid;
			stx->stx_gid = fd->inode.u.ufs2_i.di_gid;
			/* /dev files: detect device type from the devfs mount. */
			if (fd->mp != NULL && fd->mp->fs != NULL && fd->mp->fs->vfsType == VFS_TYPE_DEVFS)
			{
				if (fd->res == (void *)-1)
				{
					/* devfs root (the /dev directory itself) */
					stx->stx_mode = 0040755; /* S_IFDIR | 0755 */
					stx->stx_nlink = 2;
				}
				else
				{
					stx->stx_mode = 0020620; /* S_IFCHR | 0620 */
					stx->stx_rdev_major = 5;
					stx->stx_rdev_minor = 0;
				}
			}
			else
			{
				/* FAT driver sets di_mode = S_IFREG|0755 in open_fat(); use it. */
				stx->stx_mode = fd->inode.u.ufs2_i.di_mode;
				if ((stx->stx_mode & 0170000) == 0)
					stx->stx_mode |= 0100755;
				stx->stx_mode = stat_mode_for_fd(fd, stx->stx_mode);
			}
			stx->stx_ino = fd->ino ? fd->ino : (u_int64_t)(uintptr_t)fd;
			stx->stx_size = fd->size;
			stx->stx_blocks = (fd->size + 511) / 512;
			stx->stx_dev_major = 1;
			stx->stx_dev_minor = 1;
			statx_fill_times(stx, fd);
			fclose(fd);
		}
	}

	td->td_retval[0] = error;
	return (error);
}
