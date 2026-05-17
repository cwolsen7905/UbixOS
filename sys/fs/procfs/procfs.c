/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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

/*
 * procfs — synthetic filesystem exposing per-process information.
 * Mounted at "proc:" so processes appear as proc:/0, proc:/1, etc.
 *
 * Layout:
 *   proc:/          directory — one entry per live task
 *   proc:/N/        directory — files for task with PID N
 *   proc:/N/status  text — name, state, ppid, uid, gid
 *   proc:/N/cmdline text — process name with newline
 */

#include <fs/procfs/procfs.h>
#include <fs/vfs/vfs.h>
#include <lib/kmalloc.h>
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <sys/klog.h>
#include <string.h>

#define PROCFS_TYPE  0x02

/* Values stored in fd->start to identify the file type */
#define PFILE_DIR     0
#define PFILE_STATUS  1
#define PFILE_CMDLINE 2

/* Values stored in procfs_dir_state.type */
#define PDIR_ROOT  0
#define PDIR_PID   1

struct procfs_dir_state {
	int      type;
	kTask_t *cursor;  /* PDIR_ROOT: next task to return */
	int      subidx;  /* PDIR_PID: 0=status, 1=cmdline, 2=done */
	uint32_t pid;
};

/* -----------------------------------------------------------------------
 * Path helpers
 * --------------------------------------------------------------------- */

static int
procfs_atoi(const char *s)
{
	int n = 0;
	while (*s >= '0' && *s <= '9')
		n = n * 10 + (*s++ - '0');
	return n;
}

static int
procfs_isdigits(const char *s)
{
	if (!*s)
		return 0;
	while (*s)
		if (*s < '0' || *s++ > '9')
			return 0;
	return 1;
}

/* -----------------------------------------------------------------------
 * Content builders — generate text into caller's buffer; return byte count.
 * --------------------------------------------------------------------- */

static const char *
procfs_state_name(tState s)
{
	switch (s) {
	case NEW:             return "new";
	case READY:           return "ready";
	case RUNNING:         return "running";
	case IDLE:            return "idle";
	case FORK:            return "fork";
	case WAIT:            return "wait";
	case UNINTERRUPTIBLE: return "uninterruptible";
	case INTERRUPTIBLE:   return "interruptible";
	case DEAD:            return "dead";
	default:              return "unknown";
	}
}

static int
procfs_build_status(kTask_t *t, char *buf, int bufsz)
{
	(void)bufsz;
	return sprintf(buf,
	    "Name:\t%s\nPid:\t%u\nPPid:\t%u\nState:\t%s\nUid:\t%u\nGid:\t%u\n",
	    t->name, (unsigned)t->id, (unsigned)t->ppid,
	    procfs_state_name(t->state),
	    (unsigned)t->uid, (unsigned)t->gid);
}

static int
procfs_build_cmdline(kTask_t *t, char *buf, int bufsz)
{
	int len = strlen(t->name);
	if (len >= bufsz - 1)
		len = bufsz - 2;
	memcpy(buf, t->name, len);
	buf[len]     = '\n';
	buf[len + 1] = '\0';
	return len + 1;
}

/* -----------------------------------------------------------------------
 * vfsInitFS
 * --------------------------------------------------------------------- */

static int
procfs_initialize(struct vfs_mountPoint *mp)
{
	mp->fsInfo = NULL;
	return 0;
}

/* -----------------------------------------------------------------------
 * vfsOpenFile
 *
 * path: relative to mount point, leading '/' present (e.g. "/0/status").
 * fd->buffer is NOT yet allocated when this is called — use fd->ino and
 * fd->start to record what was opened; generate content on demand in read.
 * --------------------------------------------------------------------- */

static int
procfs_open(char *file, fileDescriptor_t *fd)
{
	char     path[256];
	char    *p;
	char     pidstr[32];
	char    *slash;
	int      plen;
	const char *rest;
	uint32_t pid;
	kTask_t *t;
	char     tmp[512];
	int      len;

	strncpy(path, file, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
	p = path;
	if (*p == '/')
		p++;

	/* Root */
	if (*p == '\0') {
		fd->ino   = 0;
		fd->start = PFILE_DIR;
		fd->size  = 0;
		return 1;
	}

	/* Split into PID component and optional file name */
	for (slash = p; *slash && *slash != '/'; slash++)
		;
	if (*slash == '/') {
		plen = (int)(slash - p);
		if (plen >= (int)sizeof(pidstr))
			return 0;
		memcpy(pidstr, p, plen);
		pidstr[plen] = '\0';
		rest = slash + 1;
	} else {
		strncpy(pidstr, p, sizeof(pidstr) - 1);
		pidstr[sizeof(pidstr) - 1] = '\0';
		rest = "";
	}

	if (!procfs_isdigits(pidstr))
		return 0;

	pid = (uint32_t)procfs_atoi(pidstr);
	t   = schedFindTask(pid);

	/* /N or /N/ — directory open */
	if (*rest == '\0') {
		fd->ino   = pid;
		fd->start = PFILE_DIR;
		fd->size  = 0;
		return (t != NULL) ? 1 : 0;
	}

	if (strcmp(rest, "status") == 0) {
		if (!t)
			return 0;
		len = procfs_build_status(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_STATUS;
		fd->size  = (uint32_t)len;
		return 1;
	}

	if (strcmp(rest, "cmdline") == 0) {
		if (!t)
			return 0;
		len = procfs_build_cmdline(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_CMDLINE;
		fd->size  = (uint32_t)len;
		return 1;
	}

	return 0;
}

/* -----------------------------------------------------------------------
 * vfsRead
 *
 * Regenerate file content on each read so it reflects current task state.
 * --------------------------------------------------------------------- */

static int
procfs_read(fileDescriptor_t *fd, char *data, long offset, long size)
{
	char     tmp[512];
	int      len;
	long     n;
	kTask_t *t;

	if (fd->start == PFILE_DIR)
		return 0;

	t = schedFindTask(fd->ino);
	if (!t)
		return -1;

	if (fd->start == PFILE_STATUS)
		len = procfs_build_status(t, tmp, sizeof(tmp));
	else if (fd->start == PFILE_CMDLINE)
		len = procfs_build_cmdline(t, tmp, sizeof(tmp));
	else
		return -1;

	if (offset >= (long)len)
		return 0;
	n = (long)len - offset;
	if (n > size)
		n = size;
	memcpy(data, tmp + offset, n);
	return (int)n;
}

/* -----------------------------------------------------------------------
 * vfsOpenDir
 *
 * path: leading '/' present (e.g. "/" or "/0").
 * --------------------------------------------------------------------- */

static int
procfs_opendir(const char *path, kDIR_t *dir)
{
	struct procfs_dir_state *s;
	char     p[256];
	char    *q;
	int      l;
	uint32_t pid;
	kTask_t *t;

	s = (struct procfs_dir_state *)kmalloc(sizeof(struct procfs_dir_state));
	if (!s)
		return 0;
	memset(s, 0, sizeof(*s));

	strncpy(p, path, sizeof(p) - 1);
	p[sizeof(p) - 1] = '\0';
	q = p;
	if (*q == '/')
		q++;

	/* Strip trailing slash */
	l = strlen(q);
	if (l > 0 && q[l - 1] == '/')
		q[l - 1] = '\0';

	if (*q == '\0') {
		s->type   = PDIR_ROOT;
		s->cursor = taskList;
	} else if (procfs_isdigits(q)) {
		pid = (uint32_t)procfs_atoi(q);
		t   = schedFindTask(pid);
		if (!t) {
			kfree(s);
			return 0;
		}
		s->type   = PDIR_PID;
		s->pid    = pid;
		s->subidx = 0;
	} else {
		kfree(s);
		return 0;
	}

	dir->dirHandle = s;
	return 1;
}

/* -----------------------------------------------------------------------
 * vfsReadDir
 * --------------------------------------------------------------------- */

static const char *procfs_pid_files[] = { "status", "cmdline" };

static int
procfs_readdir(kDIR_t *dir, struct kdirent *ent)
{
	struct procfs_dir_state *s = (struct procfs_dir_state *)dir->dirHandle;

	if (!s)
		return -1;

	if (s->type == PDIR_ROOT) {
		/* Skip dead or placeholder tasks */
		while (s->cursor &&
		    (s->cursor->state == DEAD || s->cursor->state == PLACEHOLDER))
			s->cursor = s->cursor->next;

		if (!s->cursor)
			return -1;

		sprintf(ent->d_name, "%u", (unsigned)s->cursor->id);
		ent->d_ino  = s->cursor->id;
		ent->d_type = KDT_DIR;
		s->cursor   = s->cursor->next;
		return 1;
	}

	if (s->type == PDIR_PID) {
		if (s->subidx >= 2)
			return -1;
		strncpy(ent->d_name, procfs_pid_files[s->subidx],
		    sizeof(ent->d_name) - 1);
		ent->d_name[sizeof(ent->d_name) - 1] = '\0';
		ent->d_ino  = s->pid * 10 + (uint32_t)s->subidx;
		ent->d_type = KDT_REG;
		s->subidx++;
		return 1;
	}

	return -1;
}

/* -----------------------------------------------------------------------
 * vfsCloseDir
 * --------------------------------------------------------------------- */

static int
procfs_closedir(kDIR_t *dir)
{
	if (dir->dirHandle) {
		kfree(dir->dirHandle);
		dir->dirHandle = NULL;
	}
	return 0;
}

/* -----------------------------------------------------------------------
 * procfs_init — register filesystem and mount at "proc"
 * --------------------------------------------------------------------- */

int
procfs_init(void)
{
	struct fileSystem procFS = {
		NULL,                         /* prev       */
		NULL,                         /* next       */
		(void *)procfs_initialize,    /* vfsInitFS  */
		(void *)procfs_read,          /* vfsRead    */
		NULL,                         /* vfsWrite   */
		(void *)procfs_open,          /* vfsOpenFile */
		NULL,                         /* vfsUnlink  */
		NULL,                         /* vfsMakeDir */
		NULL,                         /* vfsRemDir  */
		NULL,                         /* vfsSync    */
		PROCFS_TYPE,                  /* vfsType    */
		(void *)procfs_opendir,       /* vfsOpenDir */
		(void *)procfs_readdir,       /* vfsReadDir */
		(void *)procfs_closedir,      /* vfsCloseDir */
	};

	if (vfsRegisterFS(procFS) != 0) {
		klog(KLOG_ERR, "procfs: failed to register filesystem");
		return 1;
	}

	vfs_mount(0, 0, 0, PROCFS_TYPE, "proc", "r");
	klog(KLOG_NOTICE, "procfs: mounted at proc:");
	return 0;
}
