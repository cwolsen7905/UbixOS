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
#include <fs/vfs/mount.h>
#include <sys/bus.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <ubixos/sched.h>
#include <ubixos/sched_internal.h>
#include <ubixos/vitals.h>
#include <ubixos/time.h> /* md_uptime — /proc/uptime */
#include <sys/klog.h>
#include <sys/descrip.h>
#include <sys/thread.h>
#include <vmm/paging.h>
#include <vmm/vmm.h>
#include <vmm/vm_filecache.h>
#include <vmm/vm_map.h> /* VMA tree — /proc/<pid>/statm size */
#include <lib/rbtree.h>
#include <string.h>

#define PROCFS_TYPE  VFS_TYPE_PROCFS

/* fd->start values identifying the file type */
#define PFILE_DIR      0
#define PFILE_STATUS   1
#define PFILE_CMDLINE  2
#define PFILE_STAT     3
#define PFILE_MAPS     4
#define PFILE_FD_ENTRY 5   /* individual /proc/N/fd/M */
#define PFILE_MOUNTS   6   /* /proc/mounts — global mount table */
#define PFILE_MEMINFO  7   /* /proc/meminfo — global memory stats */
#define PFILE_STAT_GLOBAL 8 /* /proc/stat — per-CPU busy/idle tick counters */
#define PFILE_STATM    9   /* /proc/<pid>/statm — memory sizes in pages */
#define PFILE_UPTIME   10  /* /proc/uptime — seconds since boot, idle seconds */
#define PFILE_LWIP     11  /* /proc/lwip — lwIP stats + tcpip liveness (lwip-audit) */

/* lwIP counter snapshot, defined in the net stack (sys/net/net/sys_arch.c);
 * net is always linked into the kernel. */
extern int lwip_stats_format(char *buf, int bufsz);

/* procfs_dir_state.type values */
#define PDIR_ROOT  0
#define PDIR_PID   1
#define PDIR_FD    2   /* /proc/N/fd/ directory */

struct procfs_dir_state {
	int      type;
	kTask_t *cursor;  /* PDIR_ROOT: next task to return */
	int      subidx;  /* PDIR_PID: file index; PDIR_FD: o_files scan position */
	u_int32_t pid;
	int      root_phase; /* PDIR_ROOT: 0=global files, 1=pid dirs */
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
	 *
	 * utime (field 14) carries the task's accumulated scheduler ticks
	 * (run_ticks), in raw timer-tick units — the smp-plan Phase 3.5 accounting.
	 * Rendered 32-bit (wraps only after months of CPU time); userland forms
	 * ratios against /proc/stat, which is HZ-independent.
	 */
	return sprintf(buf,
	    "%u (%s) %c %u %u %u 0 -1 0 "
	    "0 0 0 0 %u 0 0 0 "
	    "0 0 1 0 0 %lu %lu\n",
	    (unsigned)t->id, t->name, state,
	    (unsigned)t->ppid, (unsigned)t->pgrp, (unsigned)t->sid,
	    (unsigned)t->run_ticks,                       /* utime := run_ticks */
	    ctob(t->td.vm_tsize) + ctob(t->td.vm_dsize),  /* vsize approx */
	    ctob(t->td.vm_tsize) + ctob(t->td.vm_dsize));  /* rss approx */
}

/**
 * Build /proc/<pid>/statm — memory usage in pages (the file an activity monitor
 * reads for memory; stat's memory fields are notoriously fiddly).
 *
 * Linux order: size resident shared text lib data dt.  `size` is the real total
 * mapped footprint, summed from the task's VMA tree (so it includes the mmap'd
 * shared libraries that dominate a dynamically-linked process — vm_tsize/vm_dsize
 * alone read ~0 for those).  `resident` mirrors `size` for now: true RSS needs a
 * per-arch, cross-address-space page-table walk (a later accuracy pass), so this
 * is an over-estimate.  text/data come from the exec'd extents; the rest zero.
 */
static int
procfs_build_statm(kTask_t *t, char *buf, int bufsz)
{
	unsigned long text = (unsigned long)t->td.vm_tsize; /* pages */
	unsigned long data = (unsigned long)t->td.vm_dsize; /* pages */
	unsigned long resident = (unsigned long)md_resident_pages(t); /* real RSS where supported */
	unsigned long size = 0;
	struct rb_node *n;

	/* Sum every VMA's [vm_start, vm_end) — the task's total mapped footprint
	 * (VSZ).  Populated by mmap on i386; empty on aarch64 (its mmap goes through
	 * pmap, not the MI VMA tree), where md_resident_pages() carries the count. */
	for (n = rb_first(&t->vm_map.vm_root); n != NULL; n = rb_next(n))
	{
		vm_map_entry_t *e = (vm_map_entry_t *)n; /* rb is the first member */
		size += (unsigned long)((e->vm_end - e->vm_start) / PAGE_SIZE);
	}
	if (size == 0)
		size = resident;       /* aarch64: no VMA tree — use the resident walk    */
	if (size == 0)
		size = text + data;    /* kernel thread / no info — fall back to extents  */
	if (resident == 0)
		resident = size;       /* i386: no page walk yet — approximate by size    */

	(void)bufsz;
	/* size resident shared text lib data dt */
	return sprintf(buf, "%lu %lu 0 %lu 0 %lu 0\n", size, resident, text, data);
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

static const char *
procfs_fstype_name(int vfsType)
{
	switch (vfsType) {
	case VFS_TYPE_FAT:    return "fat";
	case VFS_TYPE_DEVFS:  return "devfs";
	case VFS_TYPE_PROCFS: return "procfs";
	case VFS_TYPE_UFS:    return "ufs";
	default:              return "unknown";
	}
}

static int
procfs_build_mounts(char *buf, int bufsz)
{
	struct vfs_mountPoint *mp;
	int len = 0;

	if (!systemVitals)
		return 0;

	for (mp = systemVitals->mountPoints; mp != NULL; mp = mp->next) {
		const char *fsname = "unknown";
		const char *perms  = (mp->perms == 'r') ? "ro" : "rw";
		char        dev[48];
		int         vfstype = mp->fs ? mp->fs->vfsType : -1;

		if (mp->fs)
			fsname = procfs_fstype_name(vfstype);

		if (mp->device && mp->device->dev_nameunit[0])
			snprintf(dev, sizeof(dev), "/dev/%s", mp->device->dev_nameunit);
		else if (vfstype == VFS_TYPE_PROCFS)
			snprintf(dev, sizeof(dev), "proc");
		else if (vfstype == VFS_TYPE_DEVFS)
			snprintf(dev, sizeof(dev), "devfs");
		else
			snprintf(dev, sizeof(dev), "none");

		len += snprintf(buf + len, bufsz - len,
		    "%s %s %s %s 0 0\n",
		    dev, mp->mountPoint, fsname, perms);

		if (len >= bufsz - 1)
			break;
	}

	return len;
}

/**
 * Build /proc/meminfo — total and free physical RAM, in a Linux-ish format.
 * `numPages` is the number of managed physical pages; `systemVitals->freePages`
 * is the live free count.  Each page is PAGE_SIZE bytes.
 */
static int
procfs_build_meminfo(char *buf, int bufsz)
{
	u_int32_t total_pages = numPages;
	u_int32_t free_pages  = systemVitals ? systemVitals->freePages : 0;
	u_int32_t kb_per_page = PAGE_SIZE / 1024u;

	return snprintf(buf, bufsz,
	    "MemTotal: %8u kB\n"
	    "MemFree:  %8u kB\n"
	    "MemUsed:  %8u kB\n"
	    "PageSize: %8u bytes\n"
	    "Pages:    total %u free %u\n"
	    "FileCache: %u pages\n"
	    "OrphanPages: %u\n",
	    total_pages * kb_per_page,
	    free_pages * kb_per_page,
	    (total_pages - free_pages) * kb_per_page,
	    (u_int32_t)PAGE_SIZE,
	    total_pages, free_pages,
	    vm_filecache_page_count(),
	    vmm_audit_orphan_pages());
}

/**
 * Build /proc/stat — CPU busy/idle accounting (smp-plan Phase 3.5).
 *
 * Linux-ish: a `cpu` aggregate line then one `cpuN` line per online CPU, each
 * `user nice system idle ...`.  We track only two buckets, so busy ticks land
 * in the `user` column and idle ticks in the `idle` column; the rest are 0.
 * Uniprocessor today (cpu0 == the aggregate); SMP Phase 4 adds the remaining
 * per-CPU lines.  Ticks are raw timer ticks — userland takes the
 * busy/(busy+idle) ratio, which is HZ-independent.
 */
static int
procfs_build_stat_global(char *buf, int bufsz)
{
	unsigned ncpu = sched_cpu_acct_count();
	unsigned total_busy = (unsigned)sched_cpu_busy_ticks();
	unsigned total_idle = (unsigned)sched_cpu_idle_ticks();
	int off;

	/* Aggregate ("cpu") line, then one per-core ("cpu0", "cpu1", ...) line —
	 * Linux-style, so the Activity Monitor detects the core count and draws a
	 * graph per core.  A parked/offline core simply shows all-idle. */
	off = snprintf(buf, bufsz, "cpu  %u 0 0 %u 0 0 0 0 0 0\n", total_busy, total_idle);

	for (unsigned c = 0; c < ncpu && off > 0 && off < bufsz; c++)
	{
		unsigned b = (unsigned)sched_cpu_busy_ticks_n(c);
		unsigned i = (unsigned)sched_cpu_idle_ticks_n(c);
		off += snprintf(buf + off, bufsz - off, "cpu%u %u 0 0 %u 0 0 0 0 0 0\n", c, b, i);
	}
	return off;
}

/**
 * Build /proc/uptime — "<seconds since boot> <idle seconds>", two decimals each.
 *
 * Seconds-since-boot comes from md_uptime() (the MI monotonic clock, correct on
 * both arches).  Idle seconds are derived as a HZ-free ratio of the busy/idle
 * tick counters (Phase 3.5) against that wall-clock, so no per-arch HZ constant
 * is needed in this MI file.  Centisecond fixed point — the kernel printf has no
 * %f.
 */
static int
procfs_build_uptime(char *buf, int bufsz)
{
	u_int64_t sec = 0, nsec = 0;
	u_int64_t busy = sched_cpu_busy_ticks();
	u_int64_t idle = sched_cpu_idle_ticks();
	u_int64_t total = busy + idle;
	u_int64_t up_cs, idle_cs;

	md_uptime(&sec, &nsec);
	up_cs = sec * 100u + nsec / 10000000u;            /* centiseconds since boot */
	idle_cs = (total != 0) ? (up_cs * idle / total) : up_cs;

	return snprintf(buf, bufsz, "%u.%02u %u.%02u\n",
	    (unsigned)(up_cs / 100u), (unsigned)(up_cs % 100u),
	    (unsigned)(idle_cs / 100u), (unsigned)(idle_cs % 100u));
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
	u_int32_t  pid;
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

	/* Global files: /proc/mounts */
	if (strcmp(pidstr, "mounts") == 0 && *rest == '\0') {
		char tmp2[1024];
		int  mlen = procfs_build_mounts(tmp2, sizeof(tmp2));
		fd->ino   = 0;
		fd->start = PFILE_MOUNTS;
		fd->size  = (u_int32_t)mlen;
		return 1;
	}

	/* Global files: /proc/meminfo */
	if (strcmp(pidstr, "meminfo") == 0 && *rest == '\0') {
		char tmp2[256];
		int  mlen = procfs_build_meminfo(tmp2, sizeof(tmp2));
		fd->ino   = 0;
		fd->start = PFILE_MEMINFO;
		fd->size  = (u_int32_t)mlen;
		return 1;
	}

	/* Global files: /proc/stat */
	if (strcmp(pidstr, "stat") == 0 && *rest == '\0') {
		char tmp2[512]; /* aggregate + up to CPU_ENUM_MAX per-core lines */
		int  mlen = procfs_build_stat_global(tmp2, sizeof(tmp2));
		fd->ino   = 0;
		fd->start = PFILE_STAT_GLOBAL;
		fd->size  = (u_int32_t)mlen;
		return 1;
	}

	/* Global files: /proc/lwip — lwIP stats + tcpip_thread liveness */
	if (strcmp(pidstr, "lwip") == 0 && *rest == '\0') {
		char tmp2[2048];
		int  mlen = lwip_stats_format(tmp2, sizeof(tmp2));
		fd->ino   = 0;
		fd->start = PFILE_LWIP;
		fd->size  = (u_int32_t)mlen;
		return 1;
	}

	/* Global files: /proc/uptime */
	if (strcmp(pidstr, "uptime") == 0 && *rest == '\0') {
		char tmp2[64];
		int  mlen = procfs_build_uptime(tmp2, sizeof(tmp2));
		fd->ino   = 0;
		fd->start = PFILE_UPTIME;
		fd->size  = (u_int32_t)mlen;
		return 1;
	}

	if (!procfs_isdigits(pidstr))
		return 0;

	pid = (u_int32_t)procfs_atoi(pidstr);
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
		fd->size  = (u_int32_t)len;
		return 1;
	}

	if (strcmp(rest, "cmdline") == 0) {
		len = procfs_build_cmdline(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_CMDLINE;
		fd->size  = (u_int32_t)len;
		return 1;
	}

	if (strcmp(rest, "stat") == 0) {
		len = procfs_build_stat(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_STAT;
		fd->size  = (u_int32_t)len;
		return 1;
	}

	if (strcmp(rest, "maps") == 0) {
		len = procfs_build_maps(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_MAPS;
		fd->size  = (u_int32_t)len;
		return 1;
	}

	if (strcmp(rest, "statm") == 0) {
		len = procfs_build_statm(t, tmp, sizeof(tmp));
		fd->ino   = pid;
		fd->start = PFILE_STATM;
		fd->size  = (u_int32_t)len;
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
		fd->resid = (u_int32_t)fdno;
		fd->size  = (u_int32_t)flen;
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

	/* Global files don't belong to a task */
	if (fd->start == PFILE_MOUNTS) {
		char     mtmp[1024];
		int      mlen = procfs_build_mounts(mtmp, sizeof(mtmp));
		long     mn;
		if (offset >= (long)mlen)
			return 0;
		mn = (long)mlen - offset;
		if (mn > size)
			mn = size;
		memcpy(data, mtmp + offset, mn);
		return (int)mn;
	}

	if (fd->start == PFILE_MEMINFO) {
		char     mtmp[256];
		int      mlen = procfs_build_meminfo(mtmp, sizeof(mtmp));
		long     mn;
		if (offset >= (long)mlen)
			return 0;
		mn = (long)mlen - offset;
		if (mn > size)
			mn = size;
		memcpy(data, mtmp + offset, mn);
		return (int)mn;
	}

	if (fd->start == PFILE_STAT_GLOBAL) {
		char     mtmp[512]; /* aggregate + up to CPU_ENUM_MAX per-core lines */
		int      mlen = procfs_build_stat_global(mtmp, sizeof(mtmp));
		long     mn;
		if (offset >= (long)mlen)
			return 0;
		mn = (long)mlen - offset;
		if (mn > size)
			mn = size;
		memcpy(data, mtmp + offset, mn);
		return (int)mn;
	}

	if (fd->start == PFILE_LWIP) {
		char     mtmp[2048];
		int      mlen = lwip_stats_format(mtmp, sizeof(mtmp));
		long     mn;
		if (offset >= (long)mlen)
			return 0;
		mn = (long)mlen - offset;
		if (mn > size)
			mn = size;
		memcpy(data, mtmp + offset, mn);
		return (int)mn;
	}

	if (fd->start == PFILE_UPTIME) {
		char     mtmp[64];
		int      mlen = procfs_build_uptime(mtmp, sizeof(mtmp));
		long     mn;
		if (offset >= (long)mlen)
			return 0;
		mn = (long)mlen - offset;
		if (mn > size)
			mn = size;
		memcpy(data, mtmp + offset, mn);
		return (int)mn;
	}

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
	case PFILE_STATM:
		len = procfs_build_statm(t, tmp, sizeof(tmp));
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
	u_int32_t pid;
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
				pid = (u_int32_t)procfs_atoi(q);
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
		pid = (u_int32_t)procfs_atoi(q);
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
	"status", "cmdline", "stat", "statm", "maps", "fd"
};
#define PROCFS_PID_NFILES  5
#define PROCFS_PID_FD_IDX  4  /* index of "fd" entry — it's a DIR */

static int
procfs_readdir(kDIR_t *dir, struct kdirent *ent)
{
	struct procfs_dir_state *s = (struct procfs_dir_state *)dir->dirHandle;

	if (!s)
		return -1;

	/* ── Root: global files first, then live task dirs ── */
	if (s->type == PDIR_ROOT) {
		/* Phases 0..N-1: emit global synthetic files, one per readdir call */
		static const char *const procfs_root_files[] = { "mounts", "meminfo", "stat", "uptime", "lwip" };
		if (s->root_phase < (int)(sizeof(procfs_root_files) / sizeof(procfs_root_files[0]))) {
			strncpy(ent->d_name, procfs_root_files[s->root_phase], sizeof(ent->d_name) - 1);
			ent->d_name[sizeof(ent->d_name) - 1] = '\0';
			ent->d_ino  = 0;
			ent->d_type = KDT_REG;
			s->root_phase++;
			return 0;
		}

		/* Final phase: per-pid directories */
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
		ent->d_ino  = s->pid * 10u + (u_int32_t)s->subidx;
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
		ent->d_ino  = s->pid * 1000u + (u_int32_t)s->subidx;
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
