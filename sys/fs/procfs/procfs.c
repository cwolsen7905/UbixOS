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
 * Mounted at /proc.
 *
 * Layout:
 *   /proc/            directory — one entry per live task
 *   /proc/N/          directory — files for task with PID N
 *   /proc/N/status    text — name, state, ppid, pgrp, sid, uid/euid, gid/egid
 *   /proc/N/cmdline   text — process argv[0] with newline
 *   /proc/N/stat      text — single-line Linux-compatible stat
 *   /proc/N/maps      text — virtual memory regions (text, data, stack)
 *   /proc/N/fd/       directory — one entry per open file descriptor
 *   /proc/N/fd/M      text — type and info for fd M of process N
 */

#include <fs/procfs/procfs.h>
#include <fs/vfs/vfs.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <sys/klog.h>
#include <sys/descrip.h>
#include <sys/thread.h>
#include <vmm/paging.h>
#include <vmm/vmm.h>
#include <string.h>

#define PROCFS_TYPE  0x02

/* fd->start values identifying the file type */
#define PFILE_DIR      0
#define PFILE_STATUS   1
#define PFILE_CMDLINE  2
#define PFILE_STAT     3
#define PFILE_MAPS     4
#define PFILE_FD_ENTRY 5   /* individual /proc/N/fd/M */

/* procfs_dir_state.type values */
#define PDIR_ROOT  0
#define PDIR_PID   1
#define PDIR_FD    2   /* /proc/N/fd/ directory */

struct procfs_dir_state {
	int      type;
	kTask_t *cursor;  /* PDIR_ROOT: next task to return */
	int      subidx;  /* PDIR_PID: file index; PDIR_FD: o_files scan position */
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
 * Content builders — write text into buf; return byte count.
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
	    "Name:\t%s\nPid:\t%u\nPPid:\t%u\nPGrp:\t%u\nSid:\t%u\n"
	    "State:\t%s\n"
	    "Uid:\t%u\t%u\n"
	    "Gid:\t%u\t%u\n",
	    t->name,
	    (unsigned)t->id,
	    (unsigned)t->ppid,
	    (unsigned)t->pgrp,
	    (unsigned)t->sid,
	    procfs_state_name(t->state),
	    (unsigned)t->uid,  (unsigned)t->euid,
	    (unsigned)t->gid,  (unsigned)t->egid);
}

static int
procfs_build_cmdline(kTask_t *t, char *buf, int bufsz)
{
	const char *src = t->cmdline[0] ? t->cmdline : t->name;
	int len = strlen(src);
	if (len == 0)
		return 0;
	if (len >= bufsz - 1)
		len = bufsz - 2;
	memcpy(buf, src, len);
	buf[len]     = '\n';
	buf[len + 1] = '\0';
	return len + 1;
}

static int
procfs_build_stat(kTask_t *t, char *buf, int bufsz)
{
	char state;
	(void)bufsz;

	switch (t->state) {
	case RUNNING:         state = 'R'; break;
	case WAIT:
	case UNINTERRUPTIBLE: state = 'D'; break;
	case INTERRUPTIBLE:   state = 'S'; break;
	case IDLE:            state = 'I'; break;
	case DEAD:            state = 'Z'; break;
	default:              state = 'S'; break;
	}

	/*
	 * Fields: pid (name) state ppid pgrp session tty_nr tpgid flags
	 *         minflt cminflt majflt cmajflt utime stime cutime cstime
	 *         priority nice num_threads itrealvalue starttime vsize rss
	 */
	return sprintf(buf,
	    "%u (%s) %c %u %u %u 0 -1 0 "
	    "0 0 0 0 0 0 0 0 "
	    "0 0 1 0 0 %lu %lu\n",
	    (unsigned)t->id, t->name, state,
	    (unsigned)t->ppid, (unsigned)t->pgrp, (unsigned)t->sid,
	    ctob(t->td.vm_tsize) + ctob(t->td.vm_dsize),  /* vsize approx */
	    ctob(t->td.vm_tsize) + ctob(t->td.vm_dsize));  /* rss approx */
}

static int
procfs_build_maps(kTask_t *t, char *buf, int bufsz)
{
	int len = 0;
	unsigned long text_start, text_end;
	unsigned long data_start, data_end;
	unsigned long stack_top, stack_bot;

	(void)bufsz;

	text_start = (unsigned long)t->td.vm_taddr;
	text_end   = text_start + (unsigned long)ctob(t->td.vm_tsize);
	data_start = (unsigned long)t->td.vm_daddr;
	data_end   = data_start + (unsigned long)ctob(t->td.vm_dsize);

	/* Stack grows down from STACK_ADDR (0xBFFFFFFF).
	 * Show a conservative 128 KB window around the current esp. */
	stack_top = ((unsigned long)STACK_ADDR + 1u);
	stack_bot = stack_top - 0x20000u;  /* 128 KB */

	if (text_start != 0u && text_end > text_start)
		len += sprintf(buf + len,
		    "%08lx-%08lx r-xp 00000000 00:00 0\n",
		    text_start, text_end);

	if (data_start != 0u && data_end > data_start)
		len += sprintf(buf + len,
		    "%08lx-%08lx rw-p 00000000 00:00 0\n",
		    data_start, data_end);

	len += sprintf(buf + len,
	    "%08lx-%08lx rwxp 00000000 00:00 0                          [stack]\n",
	    stack_bot, stack_top);

	return len;
}

static int
procfs_build_fd_entry(kTask_t *t, int fdno, char *buf, int bufsz)
{
	struct file *fp;
	const char *type_str;
	(void)bufsz;

	if (fdno < 0 || fdno >= O_FILES)
		return -1;
	fp = (struct file *)t->td.o_files[fdno];
	if (fp == NULL)
		return -1;

	switch (fp->fd_type) {
	case FD_TYPE_TTY:  type_str = "tty";  break;
	case FD_TYPE_TTYV: type_str = "ttyv"; break;
	case 3:            type_str = "pipe"; break;
	default:
		type_str = (fp->fd != NULL) ? "file" : "special";
		break;
	}

	if (fp->fd != NULL && fp->fd->fileName[0] != '\0')
		return sprintf(buf, "fd:\t%d\ntype:\t%s\nname:\t%s\n",
		    fdno, type_str, fp->fd->fileName);

	return sprintf(buf, "fd:\t%d\ntype:\t%s\n", fdno, type_str);
}

/* -----------------------------------------------------------------------
 * vfsInitFS
 * --------------------------------------------------------------------- */

static int
procfs_initialize(struct vfs_mountPoint *mp)
{
	mp->fsInfo = NULL;
	return 1;
}

/* -----------------------------------------------------------------------
 * vfsOpenFile
 * --------------------------------------------------------------------- */

static int
procfs_open(char *file, fileDescriptor_t *fd)
{
	char      path[256];
	char     *p;
	char      pidstr[32];
	char     *slash;
	int       plen;
	const char *rest;
	uint32_t  pid;
	kTask_t  *t;
	char      tmp[512];
	int       len;

	strncpy(path, file, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
	p = path;
	if (*p == '/')
		p++;

	/* Root directory */
	if (*p == '\0') {
		fd->ino   = 0;
		fd->start = PFILE_DIR;
		fd->size  = 0;
		return 1;
	}

	/* Split leading PID component from optional rest */
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

	/* /N or /N/ — PID directory open */
	if (*rest == '\0') {
		fd->ino   = pid;
		fd->start = PFILE_DIR;
		fd->size  = 0;
		return (t != NULL) ? 1 : 0;
	}

	if (!t)
		return 0;

	if (strcmp(rest, "status") == 0) {
		len = procfs_build_status(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_STATUS;
		fd->size  = (uint32_t)len;
		return 1;
	}

	if (strcmp(rest, "cmdline") == 0) {
		len = procfs_build_cmdline(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_CMDLINE;
		fd->size  = (uint32_t)len;
		return 1;
	}

	if (strcmp(rest, "stat") == 0) {
		len = procfs_build_stat(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_STAT;
		fd->size  = (uint32_t)len;
		return 1;
	}

	if (strcmp(rest, "maps") == 0) {
		len = procfs_build_maps(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_MAPS;
		fd->size  = (uint32_t)len;
		return 1;
	}

	/* /N/fd — fd directory open */
	if (strcmp(rest, "fd") == 0) {
		fd->ino   = pid;
		fd->start = PFILE_DIR;
		fd->size  = 0;
		return 1;
	}

	/* /N/fd/M — individual fd entry */
	if (strncmp(rest, "fd/", 3) == 0) {
		const char *fdstr = rest + 3;
		char tmp2[512];
		int  flen;

		if (!procfs_isdigits(fdstr))
			return 0;
		int fdno = procfs_atoi(fdstr);
		flen = procfs_build_fd_entry(t, fdno, tmp2, sizeof(tmp2));
		if (flen < 0)
			return 0;
		fd->ino   = pid;
		fd->start = PFILE_FD_ENTRY;
		fd->resid = (uint32_t)fdno;
		fd->size  = (uint32_t)flen;
		return 1;
	}

	return 0;
}

/* -----------------------------------------------------------------------
 * vfsRead — regenerate content on each read so it reflects current state.
 * --------------------------------------------------------------------- */

static int
procfs_read(fileDescriptor_t *fd, char *data, off_t offset, long size)
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

	switch (fd->start) {
	case PFILE_STATUS:
		len = procfs_build_status(t, tmp, sizeof(tmp));
		break;
	case PFILE_CMDLINE:
		len = procfs_build_cmdline(t, tmp, sizeof(tmp));
		break;
	case PFILE_STAT:
		len = procfs_build_stat(t, tmp, sizeof(tmp));
		break;
	case PFILE_MAPS:
		len = procfs_build_maps(t, tmp, sizeof(tmp));
		break;
	case PFILE_FD_ENTRY:
		len = procfs_build_fd_entry(t, (int)fd->resid, tmp, sizeof(tmp));
		if (len < 0)
			return -1;
		break;
	default:
		return -1;
	}

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

	/* Root directory */
	if (*q == '\0') {
		s->type   = PDIR_ROOT;
		s->cursor = taskList;
		dir->dirHandle = s;
		return 1;
	}

	/* Check for "N/fd" pattern (fd sub-directory) */
	{
		char *sl = q;
		while (*sl && *sl != '/') sl++;
		if (*sl == '/') {
			*sl = '\0';
			if (procfs_isdigits(q) && strcmp(sl + 1, "fd") == 0) {
				pid = (uint32_t)procfs_atoi(q);
				t   = schedFindTask(pid);
				if (!t) {
					kfree(s);
					return 0;
				}
				s->type   = PDIR_FD;
				s->pid    = pid;
				s->subidx = 0;
				dir->dirHandle = s;
				return 1;
			}
			kfree(s);
			return 0;
		}
	}

	/* "N" — per-pid directory */
	if (procfs_isdigits(q)) {
		pid = (uint32_t)procfs_atoi(q);
		t   = schedFindTask(pid);
		if (!t) {
			kfree(s);
			return 0;
		}
		s->type   = PDIR_PID;
		s->pid    = pid;
		s->subidx = 0;
		dir->dirHandle = s;
		return 1;
	}

	kfree(s);
	return 0;
}

/* -----------------------------------------------------------------------
 * vfsReadDir
 * --------------------------------------------------------------------- */

/* Per-pid regular files (index matches PFILE_* - 1); "fd" is a directory. */
static const char *procfs_pid_files[] = {
	"status", "cmdline", "stat", "maps", "fd"
};
#define PROCFS_PID_NFILES  5
#define PROCFS_PID_FD_IDX  4  /* index of "fd" entry — it's a DIR */

static int
procfs_readdir(kDIR_t *dir, struct kdirent *ent)
{
	struct procfs_dir_state *s = (struct procfs_dir_state *)dir->dirHandle;

	if (!s)
		return -1;

	/* ── Root: enumerate live tasks ── */
	if (s->type == PDIR_ROOT) {
		while (s->cursor &&
		    (s->cursor->state == DEAD ||
		     s->cursor->state == PLACEHOLDER))
			s->cursor = s->cursor->next;

		if (!s->cursor)
			return -1;

		sprintf(ent->d_name, "%u", (unsigned)s->cursor->id);
		ent->d_ino  = s->cursor->id;
		ent->d_type = KDT_DIR;
		s->cursor   = s->cursor->next;
		return 0;
	}

	/* ── Per-pid: enumerate status/cmdline/stat/maps/fd ── */
	if (s->type == PDIR_PID) {
		if (s->subidx >= PROCFS_PID_NFILES)
			return -1;
		strncpy(ent->d_name, procfs_pid_files[s->subidx],
		    sizeof(ent->d_name) - 1);
		ent->d_name[sizeof(ent->d_name) - 1] = '\0';
		ent->d_ino  = s->pid * 10u + (uint32_t)s->subidx;
		ent->d_type = (s->subidx == PROCFS_PID_FD_IDX) ? KDT_DIR : KDT_REG;
		s->subidx++;
		return 0;
	}

	/* ── fd/: enumerate open file descriptors ── */
	if (s->type == PDIR_FD) {
		kTask_t *t = schedFindTask(s->pid);
		if (!t)
			return -1;
		/* Scan forward to find next open fd */
		while (s->subidx < O_FILES && t->td.o_files[s->subidx] == NULL)
			s->subidx++;
		if (s->subidx >= O_FILES)
			return -1;
		sprintf(ent->d_name, "%d", s->subidx);
		ent->d_ino  = s->pid * 1000u + (uint32_t)s->subidx;
		ent->d_type = KDT_REG;
		s->subidx++;
		return 0;
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
 * procfs_init — register filesystem
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

	klog(KLOG_NOTICE, "procfs: registered, will be mounted by automountd");
	return 0;
}
