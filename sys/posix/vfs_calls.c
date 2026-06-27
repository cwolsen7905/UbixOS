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

#include <ubixos/sched.h>
#include <ubixos/tty.h>
#include <ubixos/signal.h>
#include <machine/signal.h>
#include <isa/rs232.h>
#include <isa/kbd.h>
#include <isa/atkbd.h>
#include <lib/vesa.h>
#include <sys/thread.h>
#include <sys/sysproto_posix.h>
#include <sys/descrip.h>
#include <sys/video.h>
#include <sys/pipe.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <string.h>
#include <fs/ufs/ufs.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <sys/klog.h>
#include <fs/vfs/file.h>
#include <fs/vfs/mount.h>
#include <fs/devfs/devfs.h> /* devfs_resolve — device path -> (major, minor) for block mounts */
/* fat_filelib.h removed — use VFS unlink() for file removal */

/* Socket read/write/close moved to the socket fileops (sys/net/net/sys_arch.c);
 * the lwIP symbols are no longer referenced here. */

/* True when an unblocked signal is pending — used to make blocking I/O
 * loops interruptible.  td must be struct thread *. */
#define SIG_PENDING_UNBLOCKED(td) ((td)->sig_pending & ~(td)->sigmask.__bits[0])

/* Signals whose SIG_DFL action is "ignore".  Per POSIX an ignored signal must
 * not interrupt a blocking syscall (and shouldn't accumulate as pending); a
 * SIG_DFL SIGCHLD from an unreaped child must not EINTR-spin a blocking read. */
#define SIG_DFL_IGNORE_MASK                                                                                            \
	((1u << (SIGCHLD - 1)) | (1u << (SIGCONT - 1)) | (1u << (SIGURG - 1)) | (1u << (SIGWINCH - 1)) |               \
	 (1u << (SIGIO - 1)))

/* Pending, unblocked signals that would actually be DELIVERED — caught by a
 * handler, or default-terminate/stop.  Excludes ignored signals (SIG_IGN, or
 * SIG_DFL of a default-ignore signal) so they can't interrupt blocking I/O.
 * This is the disposition-aware replacement for SIG_PENDING_UNBLOCKED in the
 * EINTR decision of a blocking syscall. */
static u_int32_t sig_deliverable_pending(struct thread *td)
{
	u_int32_t unblocked = td->sig_pending & ~td->sigmask.__bits[0];
	int sig;

	if (unblocked == 0)
		return (0);
	for (sig = 1; sig <= 31; sig++)
	{
		u_int32_t bit = 1u << (sig - 1);
		void *h;

		if (!(unblocked & bit))
			continue;
		h = (void *)td->sigact[sig].sa_handler;
		if (h == (void *)0x1) /* SIG_IGN */
			unblocked &= ~bit;
		else if (h == 0x0 && (bit & SIG_DFL_IGNORE_MASK)) /* SIG_DFL default-ignore */
			unblocked &= ~bit;
	}
	return (unblocked);
}

/* sched_wait_event predicate: a blocking pipe reader can proceed once the pipe
 * has data or all writers have closed (EOF). */
static int pipe_readable(void *arg)
{
	struct pipeInfo *p = (struct pipeInfo *)arg;
	return (p->bCNT != 0 || p->wfdCNT <= 0);
}

#define FD_TYPE_DIR 4

int sys_open(struct thread *td, struct sys_open_args *args)
{
	return (kern_openat(td, AT_FDCWD, args->path, args->flags, args->mode));
}

/**
 * posix_openpt(2) — allocate a pseudo-terminal master.
 *
 * FreeBSD-faithful: returns a raw master fd directly (no /dev/ptmx).  The slave
 * is reached at /dev/pts/<n>, where <n> is obtained via ioctl(TIOCGPTN) on the
 * master (see sys_ioctl).  @flags (O_RDWR|O_NOCTTY) are accepted but the master
 * is always read-write and never a controlling terminal.
 *
 * @return master fd, or -EAGAIN (pool exhausted) / -EMFILE (fd table full).
 */
int sys_posix_openpt(struct thread *td, struct sys_posix_openpt_args *args)
{
	struct file *nfp = 0x0;
	int fd = 0;
	int slot;

	(void)args;
	slot = pty_open_master();
	if (slot < 0)
	{
		td->td_retval[0] = -EAGAIN;
		return (EAGAIN);
	}
	if (falloc(td, &nfp, &fd) != 0)
	{
		pty_close_master(slot);
		td->td_retval[0] = -EMFILE;
		return (EMFILE);
	}
	nfp->fd = NULL;
	nfp->fd_type = FD_TYPE_PTMASTER;
	nfp->data = (void *)(uintptr_t)slot;
	nfp->f_ops = g_ptm_ops;
	td->td_retval[0] = fd;
	return (0);
}

int sys_openat(struct thread *td, struct sys_openat_args *args)
{
	return (kern_openat(td, args->fd, args->path, args->flag, (int)args->mode));
}

int sys_close(struct thread *td, struct sys_close_args *args)
{
	struct file *fd = 0x0;
	struct pipeInfo *p_fd = 0x0;

	getfd(td, &fd, args->fd);

	/* Path B (fileops): dispatch through f_ops once the fd type registers it. */
	if (fd != NULL && fd->f_ops != NULL && fd->f_ops->close != NULL)
		return (fd->f_ops->close(fd, td, args->fd));

	// kprintf("[sC:%i:0x%X:0x%X]", args->fd, fd, fd->fd);

#ifdef DEBUG_VFS_CALLS
	kprintf("[sC::0x%X:0x%X]", args->fd, fd, fd->fd);
#endif

	if (fd == 0x0)
	{
		/* close() of an already-closed fd is a benign EBADF in POSIX —
		 * logging it floods serial output once close()-of-fd-0-1-2
		 * actually starts working. */
		td->td_retval[0] = -1;
	}
	else
	{
		switch (fd->fd_type)
		{
			case FD_TYPE_DIR:
				if (fd->data != NULL)
				{
					vfs_closedir((kDIR_t *)fd->data);
					fd->data = NULL;
				}
				fdestroy(td, fd, args->fd);
				td->td_retval[0] = 0;
				break;
			case 3:
				p_fd = fd->data;
				/* Use the pipe_end tag — fd numbers diverge from
				 * pi->rFD/wFD after dup2/fork. */
				if (fd->pipe_end == PIPE_END_READ)
					p_fd->rfdCNT--;
				else if (fd->pipe_end == PIPE_END_WRITE)
					p_fd->wfdCNT--;

				if (fdestroy(td, fd, args->fd) != 0)
				{
					klog(KLOG_ERR, "sys_close: fdestroy failed for pipe fd %d", args->fd);
				}

				if (p_fd->rfdCNT <= 0 && p_fd->wfdCNT <= 0)
				{
					struct pipeBuf *pbuf = p_fd->headPB;
					while (pbuf != NULL)
					{
						struct pipeBuf *next = pbuf->next;
						kfree(pbuf->buffer);
						kfree(pbuf);
						pbuf = next;
					}
					kfree(p_fd);
				}

				td->td_retval[0] = 0;
				break;
			case FD_TYPE_TTY:
			case FD_TYPE_TTYV:
				/* tty fds have no underlying fileDescriptor_t to close.
				 * Destroy the per-process fd slot regardless of whether
				 * it's 0/1/2 — busybox tools use
				 *   close(STDIN_FILENO); open(file)
				 * to redirect, and that pattern needs slot 0 actually
				 * freed so the open() picks it up. */
				fdestroy(td, fd, args->fd);
				td->td_retval[0] = 0;
				break;
			default:
				if (fd->fd != NULL && fclose(fd->fd) != 0)
				{
					td->td_retval[0] = -1;
				}

				if (fdestroy(td, fd, args->fd) != 0x0)
				{
					kprintf("[%s:%i] fdestroy(0x%X, 0x%X) failed\n",
					        __FILE__,
					        __LINE__,
					        fd,
					        td->o_files[args->fd]);
				}

				td->td_retval[0] = 0;
		}
	}
	return (0);
}

int sys_read(struct thread *td, struct sys_read_args *args)
{
	struct file *fd = 0x0;

	struct pipeInfo *p_fd = 0x0;
	struct pipeBuf *rp_fd = 0x0;

	size_t nbytes;

	getfd(td, &fd, args->fd);

	/* Path B (fileops): socket fds (and eventually tty/pipe) dispatch through
	 * f_ops, registered at fd creation, so this core no longer references the
	 * lwIP/tty symbols directly.  FILE fds leave f_ops NULL and fall through to
	 * the VFS path below. */
	if (fd != NULL && fd->f_ops != NULL && fd->f_ops->read != NULL)
		return (fd->f_ops->read(fd, td, (void *)args->buf, args->nbyte));

	/* Check fd_type first so dup2'd pipe fds work on stdin slot (0-3) */
	if (fd != 0x0 && fd->fd_type == 3)
	{
		p_fd = fd->data;
		if (fd->f_flag & O_NONBLOCK)
		{
			/* Non-blocking: return 0 immediately if no data */
			if (p_fd->bCNT == 0)
			{
				td->td_retval[0] = 0;
				return (0);
			}
		}
		else
		{
			/* Blocking: sleep until data arrives, the writer side closes,
			 * or a deliverable signal fires.  Sleeping (sched_wait_event) instead
			 * of spinning on sched_yield keeps an idle reader at 0% CPU; the writer
			 * wakes us via sched_wakeup_chan(p_fd).  A 1 s safety timeout re-checks
			 * even if a wakeup is ever missed.  Only DELIVERABLE signals interrupt
			 * (sig_deliverable_pending) — an ignored SIG_DFL SIGCHLD from an
			 * unreaped child must not turn this into an EINTR spin. */
			p_fd->reader_pid = (int)_current->id;
			while (p_fd->bCNT == 0)
			{
				/* Writers all gone and nothing left to drain → EOF. */
				if (p_fd->wfdCNT <= 0)
				{
					p_fd->reader_pid = 0;
					td->td_retval[0] = 0;
					return (0);
				}
				if (sig_deliverable_pending(td))
				{
					p_fd->reader_pid = 0;
					td->td_retval[0] = -EINTR;
					return (EINTR);
				}
				sched_wait_event_timeout(p_fd, pipe_readable, p_fd, 100);
			}
			p_fd->reader_pid = 0;
		}
		{
			/* Min of requested and what's available in the head buffer.
			 * (size_t subtraction wraps when requested < available, so
			 * a "diff <= 0" test would always pick the wrong branch.) */
			size_t avail = p_fd->headPB->nbytes - p_fd->headPB->offset;
			nbytes = (args->nbyte < avail) ? args->nbyte : avail;
			memcpy(args->buf, p_fd->headPB->buffer + p_fd->headPB->offset, nbytes);
			p_fd->headPB->offset += nbytes;

			if (p_fd->headPB->offset >= p_fd->headPB->nbytes)
			{
				rp_fd = p_fd->headPB;
				p_fd->headPB = p_fd->headPB->next;
				if (p_fd->headPB == NULL)
				{
					p_fd->tailPB = NULL;
				}
				kfree(rp_fd->buffer);
				kfree(rp_fd);
				p_fd->bCNT--;
			}

			td->td_retval[0] = nbytes;
		}
	}
	else if (fd == NULL)
	{
		td->td_retval[0] = -1;
		return (-1);
	}
	else if (fd->fd != NULL)
	{
		/* Regular file with a fileDescriptor_t: read via VFS */
		td->td_retval[0] = fread(args->buf, 1, args->nbyte, fd->fd);
	}
	else if (g_console_ops != NULL && g_console_ops->read != NULL)
	{
		/* Controlling-terminal placeholder (fds 0/3 or dup2'd copies): the tty read
		 * path (serial / VGA + the Ctrl+Alt+Fn VT switch) lives in the tty fileops
		 * (path B).  Specific /dev/tty(X) fds already dispatched via f_ops above. */
		return (g_console_ops->read(fd, td, (void *)args->buf, args->nbyte));
	}
	else
	{
		td->td_retval[0] = -1;
	}
	return (0);
}

int sys_pread(struct thread *td, struct sys_pread_args *args)
{
	int offset = 0;

	struct file *fd = 0x0;

	getfd(td, &fd, args->fd);

	if (fd == NULL)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	if (fd->fd != NULL)
	{
		/* Regular file: positional read at args->offset, leaving the shared
		 * fd->offset undisturbed.  Dispatch on the fd TYPE (a real fileDescriptor_t),
		 * NOT the fd NUMBER — a freshly opened file legitimately lands on fd 3 when
		 * stdin/stdout/stderr (0/1/2) are the only lower fds, and the old "fd > 3"
		 * test routed that file read to the console tty, which blocks forever (the
		 * on-device clang/cc1 hang: pread(fd=3) of the source file went to the
		 * console instead of the FS).  This mirrors sys_read's fd->fd dispatch. */
		offset = fd->fd->offset;
		fd->fd->offset = args->offset;
		td->td_retval[0] = fread(args->buf, args->nbyte, 1, fd->fd);
		fd->fd->offset = offset;
	}
	else if (g_console_ops != NULL && g_console_ops->read != NULL)
	{
		/* Console placeholder fds (no backing file): tty read via the fileops
		 * (path B); the pread offset is meaningless for a terminal, as before. */
		return (g_console_ops->read(fd, td, (void *)args->buf, args->nbyte));
	}
	else
	{
		td->td_retval[0] = -1;
	}
	return (0);
}

int sys_write(struct thread *td, struct sys_write_args *uap)
{
	struct file *fd = 0x0;

	struct pipeInfo *p_fd = 0x0;
	struct pipeBuf *p_buf = 0x0;

	getfd(td, &fd, uap->fd);

	/* Path B (fileops): socket fds dispatch through f_ops (registered at fd
	 * creation); FILE fds fall through to the VFS path below. */
	if (fd != NULL && fd->f_ops != NULL && fd->f_ops->write != NULL)
		return (fd->f_ops->write(fd, td, (const void *)uap->buf, uap->nbyte));

	/* Check fd_type first so dup2'd pipe fds work on stdout/stderr slots */
	if (fd != 0x0 && fd->fd_type == 3)
	{
		p_fd = fd->data;
		p_buf = (struct pipeBuf *)kmalloc(sizeof(struct pipeBuf));
		if (!p_buf)
		{
			td->td_retval[0] = -1;
			return (-1);
		}
		p_buf->buffer = kmalloc(uap->nbyte);
		if (!p_buf->buffer)
		{
			kfree(p_buf);
			td->td_retval[0] = -1;
			return (-1);
		}
		p_buf->next = 0x0;
		p_buf->offset = 0;

		memcpy(p_buf->buffer, uap->buf, uap->nbyte);

		p_buf->nbytes = uap->nbyte;

		if (p_fd->tailPB)
		{
			p_fd->tailPB->next = p_buf;
		}

		p_fd->tailPB = p_buf;

		if (!p_fd->headPB)
		{
			p_fd->headPB = p_buf;
		}

		p_fd->bCNT++;

		/* Wake the reader sleeping in sched_wait_event_timeout(p_fd, …) above, then
		 * give it an I/O priority boost so it runs before CPU-bound tasks. */
		sched_wakeup_chan(p_fd);
		if (p_fd->reader_pid != 0)
		{
			kTask_t *reader = schedFindTask((u_int32_t)p_fd->reader_pid);
			if (reader != NULL)
				sched_io_wakeup(reader);
		}

		td->td_retval[0] = uap->nbyte;
	}
	else if (fd != NULL && fd->fd == NULL && g_console_ops != NULL && g_console_ops->write != NULL)
	{
		/* Controlling-terminal placeholder (fds 1/2 or dup2'd copies): the tty
		 * write path lives in the tty fileops (path B, sys/posix/tty.c).
		 * Specific /dev/tty(X) fds already dispatched via f_ops at the top. */
		return (g_console_ops->write(fd, td, (const void *)uap->buf, uap->nbyte));
	}
	else
	{
		if (fd && fd->fd)
		{
			td->td_retval[0] = fwrite(uap->buf, 1, uap->nbyte, fd->fd);
		}
		else
		{
			td->td_retval[0] = -1;
		}
	}
	return (0x0);
}

/**
 * access(2) — there is no per-file permission enforcement yet, so the mode bits
 * (R_OK/W_OK/X_OK) are not checked, but EXISTENCE must be honest: shells walk
 * $PATH with access(X_OK) and treat ENOENT as "try the next directory".
 * Returning 0 unconditionally made every missing candidate look present, so a
 * missing command (e.g. `pwd` with no /bin/pwd) was exec'd as a non-existent
 * file and surfaced as EACCES ("permission denied") instead of ENOENT
 * ("command not found").  Probe existence the same way readlink/utimensat do
 * (fopen rejects directories, so fall back to vfs_opendir).
 *
 * @return 0 if @path resolves (any mode), -ENOENT if it does not.
 */
int sys_access(struct thread *td, struct sys_access_args *args)
{
	fileDescriptor_t *fp;

	if (args->path == NULL)
	{
		td->td_retval[0] = -EFAULT;
		return (EFAULT);
	}

	fp = fopen(args->path, "r");
	if (fp != NULL)
	{
		fclose(fp);
	}
	else
	{
		kDIR_t *dir = vfs_opendir(args->path);
		if (dir == NULL)
		{
			td->td_retval[0] = -ENOENT;
			return (ENOENT);
		}
		vfs_closedir(dir);
	}

	td->td_retval[0] = 0;
	return (0);
}

int sys_getdirentries(struct thread *td, struct sys_getdirentries_args *args)
{
	struct file *fd = NULL;
	struct kdirent kent;
	u_int8_t *buf;
	u_int32_t remaining, total, namelen, reclen;

	getfd(td, &fd, args->fd);

	if (fd == NULL || fd->fd_type != FD_TYPE_DIR)
	{
		td->td_retval[0] = EBADF;
		return (EBADF);
	}

	buf = (u_int8_t *)args->buf;
	remaining = args->count;
	total = 0;

	while (remaining > 0)
	{
		if (vfs_readdir((kDIR_t *)fd->data, &kent) != 0)
		{
			break;
		}

		namelen = strlen(kent.d_name);
		if (namelen == 0)
		{
			continue;
		}
		/*
		 * linux_dirent64 layout (musl/Linux i386 ABI):
		 *   d_ino    u_int64_t  offset 0  (8 bytes)
		 *   d_off    int64_t   offset 8  (8 bytes)
		 *   d_reclen u_int16_t  offset 16 (2 bytes)
		 *   d_type   u_int8_t   offset 18 (1 byte)
		 *   d_name   char[]    offset 19 (variable, NUL-terminated)
		 * Total header = 19 bytes; reclen aligned to 8.
		 */
		reclen = (19 + namelen + 1 + 7) & ~7u;

		if (reclen > remaining)
		{
			break;
		}

		u_int8_t *p = buf + total;
		memset(p, 0, reclen);
		*(u_int64_t *)(p + 0) = (u_int64_t)kent.d_ino;       /* d_ino */
		*(u_int64_t *)(p + 8) = (u_int64_t)(total + reclen); /* d_off */
		*(u_int16_t *)(p + 16) = (u_int16_t)reclen;
		*(u_int8_t *)(p + 18) = kent.d_type;
		memcpy(p + 19, kent.d_name, namelen + 1);

		total += reclen;
		remaining -= reclen;
	}

	td->td_retval[0] = (int)total;
	return (0);
}

int sys_readlink(struct thread *thr, struct sys_readlink_args *args)
{
	const char *path = args->path;

	/* /proc/self/fd/N → synthesize a path from the open fd. */
	if (strncmp(path, "/proc/self/fd/", 14) == 0)
	{
		int fdno = 0;
		const char *p = path + 14;
		while (*p >= '0' && *p <= '9')
			fdno = fdno * 10 + (*p++ - '0');
		struct file *fp = NULL;
		char target[32];
		if (fdno >= 0 && fdno < O_FILES)
			fp = (struct file *)thr->o_files[fdno];
		if (fp == NULL)
		{
			thr->td_retval[0] = -EBADF;
			return (EBADF);
		}
		/* Synthesize a stable name: fd objects don't have a stored path,
		 * so return the VFS cwd as a stand-in for terminal fds. */
		if (fp->fd == NULL)
		{
			/* TTY or pipe fd — return "terminal" so callers know. */
			snprintf(target, sizeof(target), "/dev/tty");
		}
		else
		{
			snprintf(target, sizeof(target), "/proc/self/fd/%d", fdno);
		}
		size_t n = strlen(target);
		if ((size_t)args->count < n)
			n = (size_t)args->count;
		memcpy(args->buf, target, n);
		thr->td_retval[0] = (int)n;
		return (0);
	}

	/* Real filesystem symlink (ubixfs): ask the FS for the target.  Returns the
	 * target length, or <0 if @path is missing / not a link (fall through). */
	{
		char target[1024];
		int tn = vfs_readlink(path, target, (int)sizeof(target));
		if (tn >= 0)
		{
			size_t n = (size_t)tn;
			if ((size_t)args->count < n)
				n = (size_t)args->count;
			memcpy(args->buf, target, n);
			thr->td_retval[0] = (int)n;
			return (0);
		}
	}

	/*
	 * uBixOS filesystems (FAT/devfs) have no symbolic links, so no existing
	 * path is ever a symlink.  POSIX requires readlink() on an existing
	 * non-symlink to fail with EINVAL ("not a symbolic link"), and on a
	 * missing path to fail with ENOENT.  Distinguishing the two matters for
	 * musl's realpath(), which aborts on any errno other than EINVAL but
	 * treats EINVAL as "use the literal component and continue": returning
	 * ENOENT for an existing directory broke resource discovery in ported
	 * apps (e.g. NetSurf's Messages/CSS/font lookup), while returning EINVAL
	 * for a missing path would let realpath() wrongly succeed on it.  Probe
	 * existence (fopen rejects directories, so fall back to vfs_opendir) to
	 * return the POSIX-correct errno.
	 */
	fileDescriptor_t *fd = fopen(path, "rb");
	if (fd != 0x0)
	{
		fclose(fd);
	}
	else
	{
		kDIR_t *dir = vfs_opendir(path);
		if (dir == 0x0)
		{
			thr->td_retval[0] = -ENOENT;
			return (ENOENT);
		}
		vfs_closedir(dir);
	}

	thr->td_retval[0] = -EINVAL;
	return (EINVAL);
}

int kern_openat(struct thread *thr, int afd, char *path, int flags, int mode)
{
	int error = 0x0;
	int fd = 0x0;
	struct file *nfp = 0x0;
	int oflags = flags;

	/*
	 * Only one of the O_EXEC, O_RDONLY, O_WRONLY and O_RDWR flags
	 * may be specified.
	 */
	if (flags & O_EXEC)
	{
		if (flags & O_ACCMODE)
		{
			return (EINVAL);
		}
	}
	else if ((flags & O_ACCMODE) == O_ACCMODE)
	{
		return (EINVAL);
	}
	else
	{
		flags = FFLAGS(flags);
	}

	error = falloc(thr, &nfp, &fd);

	if (error)
	{
		thr->td_retval[0] = error;
		return (error);
	}

	nfp->f_flag = flags & FMASK;

	/* Special case: /dev/tty opens the calling process's controlling terminal.
	 * Fall back to _current->term when ct_tty is NULL (e.g. right after
	 * setsid()) — UbixOS has no /dev/ttyN device for the app to open and
	 * then call TIOCSCTTY on, so we treat the inherited term as good enough. */
	if (strcmp(path, "/dev/tty") == 0)
	{
		if (_current->ct_tty == NULL && _current->term == NULL)
		{
			/* No TTY layer / controlling terminal (e.g. the aarch64 bring-up,
			 * which has a single global console but no tty_term registry): if the
			 * arch provides console fileops, hand back a console descriptor so
			 * interactive shells that open /dev/tty (tcsh, vi) still work. */
			if (g_console_ops != NULL)
			{
				nfp->fd = NULL;
				nfp->fd_type = FD_TYPE_TTY;
				nfp->f_ops = g_console_ops;
				thr->td_retval[0] = fd;
				return (0);
			}
			fdestroy(thr, nfp, fd);
			thr->td_retval[0] = -ENXIO;
			return (ENXIO);
		}
		/* If no explicit ct_tty yet, promote the inherited term. */
		if (_current->ct_tty == NULL)
			_current->ct_tty = _current->term;
		/* Claim foreground pgrp so tcgetpgrp() matches the opener's pgrp. */
		if (_current->ct_tty->t_pgrp == 0)
			_current->ct_tty->t_pgrp = (int)_current->pgrp;
		nfp->fd = NULL;
		nfp->fd_type = FD_TYPE_TTY;
		nfp->f_ops = g_console_ops; /* path B: tty write dispatches through fileops */
		thr->td_retval[0] = fd;
		return (0);
	}

	/* /dev/console — always the system console (ttyv0). */
	if (strcmp(path, "/dev/console") == 0)
	{
		tty_term *t = g_tty_find ? (tty_term *)g_tty_find(0) : NULL;
		if (t == NULL)
		{
			fdestroy(thr, nfp, fd);
			thr->td_retval[0] = -ENODEV;
			return (ENODEV);
		}
		nfp->fd = NULL;
		nfp->fd_type = FD_TYPE_TTYV;
		nfp->data = t;
		nfp->f_ops = g_console_ops; /* path B: tty write dispatches through fileops */
		thr->td_retval[0] = fd;
		return (0);
	}

	/* /dev/stdin, /dev/stdout, /dev/stderr — dup the process's fd 0/1/2. */
	{
		int src_fd = -1;
		if (strcmp(path, "/dev/stdin") == 0)
			src_fd = 0;
		else if (strcmp(path, "/dev/stdout") == 0)
			src_fd = 1;
		else if (strcmp(path, "/dev/stderr") == 0)
			src_fd = 2;
		if (src_fd >= 0)
		{
			struct file *src = (struct file *)thr->o_files[src_fd];
			if (src == NULL)
			{
				fdestroy(thr, nfp, fd);
				thr->td_retval[0] = -EBADF;
				return (EBADF);
			}
			memcpy(nfp, src, sizeof(struct file));
			if (src->fd != NULL)
				src->fd->dup++;
			thr->td_retval[0] = fd;
			return (0);
		}
	}

	/* /dev/ttyvN — open a specific virtual terminal or pty pool slot.
	 * Accepts any decimal slot index (ttyv0-3 fixed consoles, 5+ ptys). */
	{
		const char *tvp = NULL;
		if (strncmp(path, "/dev/ttyv", 9) == 0)
			tvp = path + 9;
		if (tvp != NULL && *tvp >= '0' && *tvp <= '9')
		{
			int idx = 0;
			const char *p = tvp;
			while (*p >= '0' && *p <= '9')
				idx = idx * 10 + (*p++ - '0');
			if (*p == '\0')
			{
				tty_term *t = g_tty_find ? (tty_term *)g_tty_find((u_int16_t)idx) : NULL;
				if (t == NULL)
				{
					fdestroy(thr, nfp, fd);
					thr->td_retval[0] = ENODEV;
					return (ENODEV);
				}
				nfp->fd = NULL;
				nfp->fd_type = FD_TYPE_TTYV;
				nfp->data = t;
				/* A pty slave dispatches through the VT100 tty fileops (g_tty_ops);
				 * where that's not separately installed (i386, where the tty layer
				 * also is the console) fall back to g_console_ops. */
				nfp->f_ops = g_tty_ops ? g_tty_ops : g_console_ops;
				/* POSIX: session leader opening a terminal without
				 * O_NOCTTY implicitly acquires it as controlling tty. */
				if (!(oflags & O_NOCTTY) && _current->ct_tty == NULL)
				{
					_current->ct_tty = t;
					if (t->t_pgrp == 0)
						t->t_pgrp = (int)_current->pgrp;
				}
				thr->td_retval[0] = fd;
				return (0);
			}
		}
	}

	/* /dev/pts/N — pseudo-terminal slave (posix_openpt slave side).  Resolves to
	 * the pool slot whose raw master posix_openpt() handed out; opened by the
	 * dropbear session child as the shell's controlling terminal.  Same fd shape
	 * as /dev/ttyvN (FD_TYPE_TTYV slave), but keyed by the pts unit number. */
	{
		const char *pp = NULL;
		if (strncmp(path, "/dev/pts/", 9) == 0)
			pp = path + 9;
		if (pp != NULL && *pp >= '0' && *pp <= '9')
		{
			int n = 0;
			const char *p = pp;
			while (*p >= '0' && *p <= '9')
				n = n * 10 + (*p++ - '0');
			if (*p == '\0')
			{
				int slot = pty_slot_for_pts(n);
				tty_term *t =
				    (slot >= 0 && g_tty_find) ? (tty_term *)g_tty_find((u_int16_t)slot) : NULL;
				if (t == NULL)
				{
					fdestroy(thr, nfp, fd);
					thr->td_retval[0] = ENODEV;
					return (ENODEV);
				}
				nfp->fd = NULL;
				nfp->fd_type = FD_TYPE_TTYV;
				nfp->data = t;
				nfp->f_ops = g_tty_ops ? g_tty_ops : g_console_ops;
				if (!(oflags & O_NOCTTY) && _current->ct_tty == NULL)
				{
					_current->ct_tty = t;
					if (t->t_pgrp == 0)
						t->t_pgrp = (int)_current->pgrp;
				}
				thr->td_retval[0] = fd;
				return (0);
			}
		}
	}

	/* Directory open: use VFS dir layer instead of fopen */
	if (oflags & O_DIRECTORY)
	{
		kDIR_t *kdir = vfs_opendir(path);
		if (kdir == NULL)
		{
			fdestroy(thr, nfp, fd);
			thr->td_retval[0] = -ENOENT;
			return (ENOENT);
		}
		nfp->fd_type = FD_TYPE_DIR;
		nfp->data = kdir;
		nfp->fd = NULL;
		thr->td_retval[0] = fd;
		return (0);
	}

	if ((oflags & O_WRONLY) && (oflags & O_APPEND))
	{
		nfp->fd = fopen(path, "a");
	}
	else if (oflags & O_WRONLY)
	{
		nfp->fd = fopen(path, "w");
	}
	else if (oflags & O_RDWR)
	{
		if (oflags & O_TRUNC)
		{
			nfp->fd = fopen(path, "w+b"); /* read-write, truncate (and create) */
		}
		else
		{
			/* read-write in place — must NOT truncate an existing file. */
			nfp->fd = fopen(path, "r+b");
			if (nfp->fd == NULL && (oflags & O_CREAT))
				nfp->fd = fopen(path, "w+b"); /* create a new empty file */
		}
	}
	else
	{
		nfp->fd = fopen(path, "r");
	}

	if (nfp->fd == 0x0)
	{
		if (fdestroy(thr, nfp, fd) != 0x0)
		{
			kprintf("[%s:%i] fdestroy() failed.", __FILE__, __LINE__);
		}

		thr->td_retval[0] = -ENOENT;
		return (ENOENT);
	}

	thr->td_retval[0] = fd;
	return (0);
}

int sys_unlink(struct thread *td, struct sys_unlink_args *uap)
{
	int r = unlink(uap->path);
	td->td_retval[0] = r;
	return (r == 0 ? 0 : -r);
}

/*
 * sys_mount — POSIX mount(2), FreeBSD ABI syscall 21.
 *
 * type: filesystem type string ("devfs", "procfs", "fat")
 * path: POSIX mount point path (must already exist as a directory)
 * flags: ignored for now
 * data:  the backing device path for a block-device FS, e.g. "/dev/ad0s1"
 *        (resolved to a major/minor via devfs).  NULL or "none" => deviceless,
 *        used by the pseudo-FSes (devfs/procfs) which need no device.  This is
 *        how fstab "/dev/ad0s1 /boot fat rw" mounts a specific partition.
 *
 * Maps type string to the internal vfsType integer used by vfsRegisterFS, then
 * delegates to vfs_mount().
 */
int sys_mount(struct thread *td, struct sys_mount_args *uap)
{
	static const struct
	{
		const char *name;
		int type;
		const char *perms;
	} fs_map[] = {
	    {"devfs", 1, "rw"}, {"procfs", 0x02, "r"}, {"fat", 0xFA, "rw"}, {"msdos", 0xFA, "rw"}, {NULL, 0, NULL}};
	int i, vfs_type = -1;
	const char *fs_perms = "r";
	char mpath[1024];

	if (uap->type == NULL || uap->path == NULL)
	{
		td->td_retval[0] = -EINVAL;
		return (EINVAL);
	}

	for (i = 0; fs_map[i].name != NULL; i++)
	{
		if (strcmp(uap->type, fs_map[i].name) == 0)
		{
			vfs_type = fs_map[i].type;
			fs_perms = fs_map[i].perms;
			break;
		}
	}
	if (vfs_type < 0)
	{
		kprintf("sys_mount: unknown fstype \"%s\"\n", uap->type);
		td->td_retval[0] = -EINVAL;
		return (EINVAL);
	}

	strncpy(mpath, uap->path, sizeof(mpath) - 1);
	mpath[sizeof(mpath) - 1] = '\0';

	/* Resolve the backing device, if any.  A block-device FS (fat) names its
	 * partition via data ("/dev/ad0s1"); devfs maps that to (major, minor).  A
	 * NULL or "none" device is deviceless (devfs/procfs) -> (0, 0). */
	int major = 0, minor = 0;
	const char *dev = (const char *)uap->data;
	if (dev != NULL && dev[0] != '\0' && strcmp(dev, "none") != 0)
	{
		u_int16_t mj = 0, mn = 0;
		if (devfs_resolve(dev, &mj, &mn) != 0)
		{
			kprintf("sys_mount: device \"%s\" not found\n", dev);
			td->td_retval[0] = -ENODEV;
			return (ENODEV);
		}
		major = (int)mj;
		minor = (int)mn;
	}

	if (vfs_mount(major, minor, 0, vfs_type, mpath, fs_perms) != 0)
	{
		kprintf("sys_mount: vfs_mount(%s, %s) failed\n", uap->type, mpath);
		td->td_retval[0] = -EIO;
		return (EIO);
	}

	kprintf("sys_mount: mounted %s at %s (dev %i:%i)\n", uap->type, mpath, major, minor);
	td->td_retval[0] = 0;
	return (0);
}

/*
 * sys_umount — POSIX unmount(2), FreeBSD ABI syscall 22.
 *
 * Finds the mount point by exact path match and removes it from the table.
 * Does not call a per-FS cleanup hook (none registered yet).
 */
int sys_umount(struct thread *td, struct sys_umount_args *uap)
{
	if (uap->path == NULL)
	{
		td->td_retval[0] = -EINVAL;
		return (EINVAL);
	}

	if (vfs_umount(uap->path) != 0)
	{
		kprintf("sys_umount: no mount at \"%s\"\n", uap->path);
		td->td_retval[0] = -EINVAL;
		return (EINVAL);
	}

	kprintf("sys_umount: unmounted %s\n", uap->path);
	td->td_retval[0] = 0;
	return (0);
}
