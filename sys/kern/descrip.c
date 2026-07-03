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

#include <sys/descrip.h>
#include <sys/errno.h>
#include <sys/bus.h>
#include <sys/ioctl.h>
#include <fs/devfs/devfs.h>
#include <pci/ac97.h>

#include <sys/sysproto_posix.h>
#include <sys/thread.h>
#include <lib/kprintf.h>
#include <ubixos/endtask.h>
#include <lib/kmalloc.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>
#include <ubixos/signal.h>
#include <machine/signal.h>
#include <ubixos/vitals.h>
#include <isa/pit.h>
#include <isa/rs232.h>
#include <isa/kbd.h>
#include <isa/atkbd.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/pipe.h>
#include <string.h>

static int close_fdslot(struct thread *td, int fd);
static int duplicate_descriptor(struct thread *td, int from, int to);

/* (lwip_select is no longer called directly — see the g_socket_select hook.) */
struct timeval;
struct fd_set;

/* Console/TTY fileops (path B): defined here in the generic fd layer so it
 * resolves on every arch; installed with &tty_ops by tty_init() (sys/posix/tty.c)
 * and left NULL on arches without a TTY layer. */
struct fileOps *g_console_ops = 0x0;
/* Pty (FD_TYPE_TTYV) fileops; see sys/include/sys/descrip.h.  NULL until the tty
 * layer installs it (it does so only where it differs from g_console_ops). */
struct fileOps *g_tty_ops = 0x0;
/* Raw pty master (FD_TYPE_PTMASTER) fileops; installed by tty_init(). */
struct fileOps *g_ptm_ops = 0x0;
void *(*g_tty_find)(u_int16_t slot) = 0x0;
/* sys_ioctl hooks (path B): installed by the tty + devfs/device layers; NULL on
 * arches that don't link them, where the corresponding ioctls become no-ops. */
void (*g_tty_inject)(void *term, char ch) = 0x0;
void (*g_tty_signal)(void *term, int sig) = 0x0; /* post a signal to a tty's pgrp (SIGWINCH) */
int (*g_dev_char_ioctl)(struct file *fp, u_int32_t com, void *data) = 0x0;
/* sys_select/sys_poll hooks (path B): g_socket_select is lwIP's select (the only
 * socket symbol — the fd_set math is generic); g_console_stdin_ready drains the
 * controlling-terminal input and reports whether a line is ready.  Installed by
 * the socket + tty layers; NULL where those aren't linked (no socket fds can
 * exist there, and the console poll is simply skipped). */
int (*g_socket_select)(int nfds, struct fd_set *r, struct fd_set *w, struct fd_set *e, struct timeval *tv) = 0x0;
int (*g_console_stdin_ready)(void) = 0x0;
/* Lower-VFS-layer hooks (path B): keep file.c/mount.c's optional couplings out
 * of the generic build.  All NULL where the providing layer isn't linked.
 *   g_device_find     major/minor -> block device   (installed by isa_bus_init)
 *   g_fs_rename/_trunc FAT rename/truncate shortcuts (installed by fat_init)
 *   g_tty_print       blocking console write          (installed by tty_init)
 *   g_tty_getchar     blocking console getchar        (installed by tty_init) */
void *(*g_device_find)(int major, int minor) = 0x0;
int (*g_fs_rename)(void *fs, const char *src, const char *dst) = 0x0;
int (*g_fs_truncate)(void *file, u_int32_t length) = 0x0;
void (*g_tty_print)(const char *buf, void *term) = 0x0;
int (*g_tty_getchar)(void) = 0x0;
void (*g_serial_putc)(char c) = 0x0;

int fcntl(struct thread *td, struct sys_fcntl_args *uap)
{
	struct file *fp = 0x0;
	int i = 0;

	if (uap->fd < 0 || uap->fd >= O_FILES || td->o_files[uap->fd] == 0x0)
		return (-EBADF);

	fp = (struct file *)td->o_files[uap->fd];

	switch (uap->cmd)
	{
		case F_DUPFD:
		{
			int start = (uap->arg < 5) ? 5 : uap->arg;
			for (i = start; i < O_FILES; i++)
			{
				if (td->o_files[i] == 0x0)
				{
					if (duplicate_descriptor(td, uap->fd, i) != 0)
						return (-EMFILE);
					td->td_retval[0] = i;
					break;
				}
			}
			if (td->td_retval[0] == 0)
				return (-EMFILE);
			break;
		}
		case F_GETFL:
			td->td_retval[0] = fp->f_flag;
			break;
		case F_SETFD:
			/* store close-on-exec and other fd flags; silently accept */
			td->td_retval[0] = 0;
			break;
		case F_SETFL:
			fp->f_flag &= ~FCNTLFLAGS;
			fp->f_flag |= FFLAGS(uap->arg & ~O_ACCMODE) & FCNTLFLAGS;
			break;
		default:
			kprintf("fcntl: unhandled cmd=%i fd=%i\n", uap->cmd, uap->fd);
	}

	return (0x0);
}

int sys_fcntl(struct thread *td, struct sys_fcntl_args *uap)
{
	return (fcntl(td, uap));
}

int falloc(struct thread *td, struct file **resultfp, int *resultfd)
{

	struct file *fp = 0x0;
	int i = 0;

	fp = (struct file *)kmalloc(sizeof(struct file));
	if (fp == 0x0)
	{
		if (resultfp)
			*resultfp = 0x0;
		if (resultfd)
			*resultfd = 0x0;
		return (ENOMEM);
	}
	memset(fp, 0, sizeof(struct file));

	/* Allocate the lowest free slot.  Userland tools rely on the POSIX
	 * "open returns the lowest free fd" rule for the close(N); open(file)
	 * redirect idiom; starting the search at 5 broke that. */
	for (i = 0; i < O_FILES; i++)
	{
		if (td->o_files[i] == 0x0)
		{
			td->o_files[i] = (void *)fp;
			if (resultfd)
				*resultfd = i;
			if (resultfp)
				*resultfp = fp;
			goto allocated;
		}
	}

	kfree(fp);

	if (resultfp)
		*resultfp = 0x0;
	if (resultfd)
		*resultfd = 0x0;

	return (EMFILE);

allocated:

	return (0x0);
}

/**
 * @brief   This destroys a thread local file descriptor.
 *
 * @details This function will destroy the thread local file file specified as fd,
 *          for sanity it requires the file descriptor id and memory address to check before destroying.
 *
 * @note    This text shall only show you, how such a \"note\" section
 *          is looking. There is nothing which really needs your notice,
 *          so you do not really need to read this section.
 *
 * @param   td Pointer to thread which initiated the syscall.
 * @param   fp Pointer to the thread local file which you want to destroy.
 * @param   fd File descriptor id that points to the thread local file which you want to destroy.
 *
 * @return  The error return code of the function.
 *
 * @retval  0 The function is successfully executed
 * @retval  -1 An error occurred
 */
int fdestroy(struct thread *td, struct file *fp, int fd)
{
	int error = 0;

	if (td->o_files[fd] != fp)
	{
		error = -1;
	}
	else
	{
		kfree(td->o_files[fd]);
		td->o_files[fd] = 0x0;
	}

	return (error);
}

static int close_fdslot(struct thread *td, int fd)
{
	if (fd < 0 || fd >= O_FILES || td->o_files[fd] == NULL)
		return (-1);

	struct file *f = (struct file *)td->o_files[fd];

	switch (f->fd_type)
	{
		case FD_TYPE_DIR:
			if (f->data != NULL)
				vfs_closedir((kDIR_t *)f->data);
			return fdestroy(td, f, fd);

		case FD_TYPE_TTY:
		case FD_TYPE_TTYV:
			return fdestroy(td, f, fd);

		case FD_TYPE_PTMASTER:
			/* Run the master close (SIGHUP slave + free ring/slot) before destroying
			 * the fd; sys_close() also does this, but dup2()/teardown reach here. */
			if (f->f_ops != NULL && f->f_ops->close != NULL)
				f->f_ops->close(f, td, fd);
			return fdestroy(td, f, fd);

		case FD_TYPE_PIPE:
			if (f->data != NULL)
			{
				struct pipeInfo *pi = (struct pipeInfo *)f->data;
				/* Use the struct file's pipe_end tag rather than comparing fd
				 * numbers — dup2/fork copies keep the same tag but get new
				 * fd slots, so the original rFD/wFD numbers stop matching. */
				if (f->pipe_end == PIPE_END_READ)
					pi->rfdCNT--;
				else if (f->pipe_end == PIPE_END_WRITE)
					pi->wfdCNT--;
				if (pi->rfdCNT <= 0 && pi->wfdCNT <= 0)
				{
					struct pipeBuf *pbuf = pi->headPB;
					while (pbuf != NULL)
					{
						struct pipeBuf *next = pbuf->next;
						kfree(pbuf->buffer);
						kfree(pbuf);
						pbuf = next;
					}
					kfree(pi);
					f->data = NULL;
				}
			}
			return fdestroy(td, f, fd);

		default:
			if (f->fd != NULL)
				fclose(f->fd);
			return fdestroy(td, f, fd);
	}
}

static int duplicate_descriptor(struct thread *td, int from, int to)
{
	struct file *fp = NULL;
	struct file *dup_fp = NULL;

	if (from < 0 || from >= O_FILES || td->o_files[from] == NULL)
		return (-1);
	if (to < 0 || to >= O_FILES)
		return (-1);
	if (from == to)
		return (0);

	if (td->o_files[to] != NULL)
	{
		if (close_fdslot(td, to) != 0)
			return (-1);
	}

	fp = (struct file *)td->o_files[from];
	dup_fp = (struct file *)kmalloc(sizeof(struct file));
	if (dup_fp == NULL)
		return (-ENOMEM);
	memcpy(dup_fp, fp, sizeof(struct file));

	if (fp->fd != NULL)
		fp->fd->dup++;

	/* Pipes: bump the refcount on the shared pipeInfo so a later close
	 * of either fd doesn't drop it to zero with copies still live. */
	if (fp->fd_type == FD_TYPE_PIPE && fp->data != NULL)
	{
		struct pipeInfo *pi = (struct pipeInfo *)fp->data;
		if (fp->pipe_end == PIPE_END_READ)
			pi->rfdCNT++;
		else if (fp->pipe_end == PIPE_END_WRITE)
			pi->wfdCNT++;
	}

	/* posix_openpt master: dropbear dup2()s the master onto the channel fds, so
	 * the dup must share the ref — else closing the original tears the pty down
	 * (SIGHUP) while the relay still holds a copy. */
	if (fp->fd_type == FD_TYPE_PTMASTER)
		pty_master_fork_ref((int)(uintptr_t)fp->data);

	td->o_files[to] = (void *)dup_fp;
	return (0);
}

int close(struct thread *td, struct close_args *uap)
{
#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif
	if (uap->fd < 0 || uap->fd >= O_FILES || td->o_files[uap->fd] == 0x0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	if (close_fdslot(td, uap->fd) != 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	td->td_retval[0] = 0x0;
	return (0x0);
}

/*!
 * \brief return data table size
 */
int getdtablesize(struct thread *td, struct getdtablesize_args *uap)
{
#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif
	td->td_retval[0] = O_FILES;
	return (0);
}

/* HACK */
int fstat(struct thread *td, struct sys_fstat_args *uap)
{
	struct file *fp = 0x0;

#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif

	fp = (struct file *)_current->td.o_files[uap->fd];
	uap->sb->st_mode = 0x2180;
	uap->sb->st_blksize = 0x1000;
	return (0x0);
}

/*!
 * \brief ioctl functionality not implimented yet
 *
 * \returns NULL for now
 */
int ioctl(struct thread *td, struct ioctl_args *uap)
{
	td->td_retval[0] = 0x0;
	return (0x0);
}

/*!
 * \brief get pointer to file fd in specified thread
 *
 * \return returns fp
 */
int getfd(struct thread *td, struct file **fp, int fd)
{
#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif

	if (fd < 0 || fd >= O_FILES)
	{
		*fp = NULL;
		return (-1);
	}

	*fp = (struct file *)td->o_files[fd];

	return (*fp == NULL ? -1 : 0);
}

int sys_ioctl(struct thread *td, struct sys_ioctl_args *args)
{
	struct file *tty_fp = NULL;
	getfd(td, &tty_fp, args->fd);

	/* ioctl request codes are 32-bit (the _IOC encoding never exceeds 32 bits).
	 * musl declares ioctl()'s request as `int`, so a command with IOC_IN set
	 * (0x80000000 — e.g. TIOCSPGRP, TIOCSETD) is negative and the aarch64 syscall
	 * path sign-extends it into the 64-bit com register (0xFFFFFFFF8.......).
	 * Truncate to 32 bits so it matches the case labels.  No-op on i386, where
	 * u_long is already 32-bit. */
	args->com = (u_int32_t)args->com;

	/* Raw posix_openpt master (FD_TYPE_PTMASTER): fd->data holds the pts/pool slot.
	 * Service the pty-master ioctls musl issues from openpty()/ptsname()/unlockpt()
	 * and forward window-size changes to the slave so the SSH client can resize. */
	if (tty_fp != NULL && tty_fp->fd_type == FD_TYPE_PTMASTER)
	{
		int slot = (int)(uintptr_t)tty_fp->data;

		switch (args->com)
		{
			case TIOCGPTN: /* ptsname(): slave unit number */
				if (args->data != NULL)
					*(int *)args->data = slot;
				td->td_retval[0] = 0;
				return (0);
			case TIOCSPTLCK: /* unlockpt(): master is always unlocked */
				td->td_retval[0] = 0;
				return (0);
			case TIOCSWINSZ: /* SSH window-size change → slave winsize */
			{
				tty_term *mt = g_tty_find ? (tty_term *)g_tty_find((u_int16_t)slot) : NULL;
				if (mt != NULL && args->data != NULL)
					mt->t_winsize = *(struct winsize *)args->data;
				td->td_retval[0] = 0;
				return (0);
			}
			default:
				td->td_retval[0] = 0; /* accept-and-ignore other master ioctls */
				return (0);
		}
	}

	/* FD_TYPE_TTYV fds carry their own tty_term in fd->data. */
	tty_term *term;
	if (tty_fp != NULL && tty_fp->fd_type == FD_TYPE_TTYV)
		term = (tty_term *)tty_fp->data;
	else
		term = _current->ct_tty ? _current->ct_tty : _current->term;

	/*
	 * on_tty: fd is a tty when it has no underlying fileDescriptor_t (raw
	 * tty fd inherited from parent), was opened via /dev/tty (FD_TYPE_TTY),
	 * or is a specific vty (FD_TYPE_TTYV).
	 */
	int on_tty = (tty_fp != NULL &&
	              (tty_fp->fd == NULL || tty_fp->fd_type == FD_TYPE_TTY || tty_fp->fd_type == FD_TYPE_TTYV)) &&
	             term != NULL;

	switch (args->com)
	{
		case TIOCGETA:
			if (on_tty)
			{
				struct termios *t = (struct termios *)args->data;
				*t = term->t_termios;
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCSETA:
		case TIOCSETAW:
			if (on_tty)
			{
				struct termios *t = (struct termios *)args->data;
				term->t_termios = *t;
				term->t_raw = (t->c_lflag & ICANON) ? 0 : 1;
				term->t_echo = (t->c_lflag & ECHO) ? 1 : 0;
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCSETAF:
			/* Flush pending input before applying new attributes. */
			if (on_tty)
			{
				struct termios *t = (struct termios *)args->data;
				term->stdinSize = 0;
				term->t_linelen = 0;
				term->t_termios = *t;
				term->t_raw = (t->c_lflag & ICANON) ? 0 : 1;
				term->t_echo = (t->c_lflag & ECHO) ? 1 : 0;
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCDRAIN:
			/* Output is synchronous — drain is always complete. */
			td->td_retval[0] = 0;
			return (0);

		case TIOCFLUSH:
			if (on_tty)
			{
				int flags = args->data ? *(int *)args->data : (FREAD | FWRITE);
				if (flags & FREAD)
				{
					term->stdinSize = 0;
					term->t_linelen = 0;
				}
				/* FWRITE: output is synchronous, nothing to flush. */
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCGWINSZ:
			if (on_tty)
			{
				struct winsize *win = (struct winsize *)args->data;
				*win = term->t_winsize;
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCSWINSZ:
			if (on_tty)
			{
				struct winsize *win = (struct winsize *)args->data;
				term->t_winsize = *win;
				if (g_tty_signal != NULL)
					g_tty_signal(term, SIGWINCH);
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCSCTTY:
			/* Claim this tty as the controlling terminal for this session. */
			if (on_tty)
			{
				_current->ct_tty = term;
				term->t_pgrp = (int)_current->pgrp;
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCGPGRP:
			if (on_tty)
			{
				*(int *)args->data = term->t_pgrp;
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCSPGRP:
			if (on_tty)
			{
				term->t_pgrp = *(int *)args->data;
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case FIONREAD:
			if (on_tty)
			{
				*(int *)args->data = term->stdinSize;
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCEXCL:
			if (on_tty)
			{
				term->t_exclusive = 1;
				td->td_retval[0] = 0;
			}
			else
				td->td_retval[0] = -1;
			return (0);

		case TIOCNXCL:
			if (on_tty)
			{
				term->t_exclusive = 0;
				td->td_retval[0] = 0;
			}
			else
				td->td_retval[0] = -1;
			return (0);

		case TIOCNOTTY:
			_current->ct_tty = NULL;
			td->td_retval[0] = 0;
			return (0);

		case TIOCGETD:
			/* Report the line discipline.  We run a single built-in tty/VT100
			 * discipline, reported as TTYDISC (0).  A job-control shell (tcsh's
			 * setdisc()) calls TIOCGETD before claiming the tty; failing it makes
			 * the shell abort job-control setup with "no access to tty". */
			if (on_tty)
			{
				if (args->data)
					*(int *)args->data = 0; /* TTYDISC */
				td->td_retval[0] = 0;
			}
			else
				td->td_retval[0] = -1;
			return (0);

		case TIOCSETD:
			/* Accept a line-discipline change (we only have the one discipline,
			 * so this is a no-op success on a tty). */
			td->td_retval[0] = on_tty ? 0 : -1;
			return (0);

		case TIOCOUTQ:
			/* Output is synchronous — queue is always empty. */
			if (args->data)
				*(int *)args->data = 0;
			td->td_retval[0] = 0;
			return (0);

		case TIOCSTI:
			/* Inject one byte into the tty line discipline (via the tty hook). */
			if (on_tty && args->data && g_tty_inject != NULL)
			{
				g_tty_inject(term, *(char *)args->data);
				td->td_retval[0] = 0;
			}
			else
			{
				td->td_retval[0] = -1;
			}
			return (0);

		case TIOCCONS:
			/* Redirect console to this tty — stub, accept silently. */
			td->td_retval[0] = 0;
			return (0);

		default:
		{
			/* Route char-device ioctls (e.g. audio) to the driver via the devfs
			 * hook; -1 when the fd is not a char device or the ioctl is unhandled. */
			struct file *fp = NULL;
			getfd(td, &fp, args->fd);
			td->td_retval[0] =
			    (g_dev_char_ioctl != NULL) ? g_dev_char_ioctl(fp, (u_int32_t)args->com, args->data) : -1;
			return (0);
		}
	}
}

int sys_select(struct thread *td, struct sys_select_args *args)
{
	int i, j, nd, total;
	fd_set lwip_rfds, lwip_wfds;
	fd_set watch_in, watch_out;
	int kern_to_lwip_r[MAX_FILES]; /* kernel fd → lwIP socket for read set */
	int kern_to_lwip_w[MAX_FILES]; /* kernel fd → lwIP socket for write set */
	int tty_rd_fds[MAX_FILES];     /* read fds that are pseudo-terminals (FD_TYPE_TTYV) */
	int n_tty_rd;
	int ptm_rd_fds[MAX_FILES];     /* read fds that are posix_openpt masters (FD_TYPE_PTMASTER) */
	int n_ptm_rd;
	int pty_wr_fds[MAX_FILES];     /* write fds that are pty master/slave (always writable) */
	int n_pty_wr;
	int max_lwip;
	int has_stdin_rd;
	struct timeval zero_tv;

	nd = (args->nd < MAX_FILES) ? args->nd : MAX_FILES;
	FD_ZERO(&lwip_rfds);
	FD_ZERO(&lwip_wfds);
	memset(kern_to_lwip_r, -1, sizeof(kern_to_lwip_r));
	memset(kern_to_lwip_w, -1, sizeof(kern_to_lwip_w));
	max_lwip = 0;
	has_stdin_rd = 0;
	n_tty_rd = 0;
	n_ptm_rd = 0;
	n_pty_wr = 0;

	/* Copy and clear input sets immediately so args->in/ou become result sets. */
	if (args->in)
	{
		watch_in = *args->in;
		FD_ZERO(args->in);
	}
	if (args->ou)
	{
		watch_out = *args->ou;
		FD_ZERO(args->ou);
	}

	/* Scan read fd_set: separate stdin from socket fds */
	if (args->in)
	{
		for (i = 0; i < nd; i++)
		{
			if (!FD_ISSET(i, &watch_in))
				continue;
			if (i == 0 &&
			    !(td->o_files[0] != NULL && ((struct file *)td->o_files[0])->fd_type == FD_TYPE_TTYV))
			{
				/* fd 0 backed by the console placeholder (FD_TYPE_TTY / none): the
				 * global console-readiness hook tracks it.  A GUI pty on fd 0 falls
				 * through to the FD_TYPE_TTYV case below (its own line buffer). */
				has_stdin_rd = 1;
			}
			else
			{
				struct file *f = td->o_files[i];
				if (f && f->fd_type == FD_TYPE_SOCKET)
				{
					kern_to_lwip_r[i] = f->socket;
					FD_SET(f->socket, &lwip_rfds);
					if (f->socket + 1 > max_lwip)
						max_lwip = f->socket + 1;
				}
				else if (f && f->fd_type == FD_TYPE_PIPE)
				{
					/* Pipe: mark immediately ready if data available */
					struct pipeInfo *pi = (struct pipeInfo *)f->data;
					if (pi && pi->bCNT > 0)
					{
						FD_SET(i, args->in);
						has_stdin_rd = 1; /* reuse flag to skip the poll loop */
					}
				}
				else if (f && f->fd_type == FD_TYPE_FILE)
				{
					/* Regular file: always ready */
					FD_SET(i, args->in);
				}
				else if (f && f->fd_type == FD_TYPE_TTYV)
				{
					/* A specific pseudo-terminal (/dev/ttyN, tty_term in f->data):
					 * checked per-fd in the poll loop via its own line buffer. */
					tty_rd_fds[n_tty_rd++] = i;
				}
				else if (f && f->fd_type == FD_TYPE_PTMASTER)
				{
					/* posix_openpt master: readable when the slave produced output
					 * (checked per-fd in the poll loop via ptm_output_pending). */
					ptm_rd_fds[n_ptm_rd++] = i;
				}
				else if (f && f->fd_type == FD_TYPE_TTY)
				{
					/* /dev/tty — the process's controlling terminal: global hook. */
					has_stdin_rd = 1;
				}
			}
		}
	}

	/* Scan write fd_set */
	if (args->ou)
	{
		for (i = 0; i < nd; i++)
		{
			if (!FD_ISSET(i, &watch_out))
				continue;
			struct file *f = (i < MAX_FILES) ? td->o_files[i] : NULL;
			if (f && f->fd_type == FD_TYPE_SOCKET)
			{
				kern_to_lwip_w[i] = f->socket;
				FD_SET(f->socket, &lwip_wfds);
				if (f->socket + 1 > max_lwip)
					max_lwip = f->socket + 1;
			}
			else if (f && (f->fd_type == FD_TYPE_PTMASTER || f->fd_type == FD_TYPE_TTYV))
			{
				/* pty master/slave: writes go to a bounded ring/line buffer and never
				 * block — always writable (set + counted in the poll loop). */
				pty_wr_fds[n_pty_wr++] = i;
			}
		}
	}

	zero_tv.tv_sec = 0;
	zero_tv.tv_usec = 0;

	/* Compute deadline in sysTicks (PIT_TIMER ticks/sec).
	 * deadline == 0 means no timeout (block until ready). */
	u_int32_t deadline = 0;
	if (args->tv != NULL)
	{
		/* Clamp tv_sec to avoid overflow: 3600 s = 720,000 ticks at 200 Hz */
		long tv_sec = args->tv->tv_sec;
		if (tv_sec < 0)
			tv_sec = 0;
		if (tv_sec > 3600)
			tv_sec = 3600;
		u_int32_t ms = (u_int32_t)(tv_sec * 1000 + args->tv->tv_usec / 1000);
		deadline = systemVitals->sysTicks + ms * PIT_TIMER / 1000 + 1;
	}

	/* Poll loop: yield until stdin or a socket becomes ready */
	for (;;)
	{
		total = 0;

		/* Check TTY stdin readiness */
		/* TTY stdin readiness: the tty layer drains the console + reports a
		 * complete line (path B hook; no-op where there's no TTY layer). */
		if (has_stdin_rd && g_console_stdin_ready != NULL && g_console_stdin_ready())
		{
			if (args->in)
				FD_SET(0, args->in);
			total++;
		}

		/* Check sockets (non-blocking) via the socket-select hook. */
		if (max_lwip > 0 && g_socket_select != NULL)
		{
			fd_set tmp_r = lwip_rfds;
			fd_set tmp_w = lwip_wfds;
			int r = g_socket_select(max_lwip, &tmp_r, &tmp_w, NULL, &zero_tv);
			if (r > 0)
			{
				for (i = 0; i < nd; i++)
				{
					if (args->in && kern_to_lwip_r[i] >= 0 && FD_ISSET(kern_to_lwip_r[i], &tmp_r))
					{
						FD_SET(i, args->in);
						total++;
					}
					if (args->ou && kern_to_lwip_w[i] >= 0 && FD_ISSET(kern_to_lwip_w[i], &tmp_w))
					{
						FD_SET(i, args->ou);
						total++;
					}
				}
			}
		}

		/* TTYV (pseudo-terminal) read fds — e.g. the GUI terminal's stdin: ready when
		 * a complete canonical line sits in the pty's own line discipline (filled by
		 * the terminal app via sys_ptyinject).  Checked per-fd here rather than via the
		 * global g_console_stdin_ready hook, which tracks the *console* (serial/VGA),
		 * not an arbitrary pty — so a select() mixing a pty stdin with a socket wakes
		 * on either. */
		for (j = 0; j < n_tty_rd; j++)
		{
			struct file *tf = td->o_files[tty_rd_fds[j]];
			tty_term *tt = (tf != NULL) ? (tty_term *)tf->data : NULL;
			if (tt != NULL && tt->stdinSize > 0)
			{
				if (args->in)
					FD_SET(tty_rd_fds[j], args->in);
				total++;
			}
		}

		/* posix_openpt master read fds — ready when the slave (shell) has produced
		 * output for the master (dropbear) to forward to the SSH channel. */
		for (j = 0; j < n_ptm_rd; j++)
		{
			struct file *mf = td->o_files[ptm_rd_fds[j]];
			if (mf != NULL && ptm_output_pending((int)(uintptr_t)mf->data) > 0)
			{
				if (args->in)
					FD_SET(ptm_rd_fds[j], args->in);
				total++;
			}
		}

		/* pty master/slave write fds — always writable. */
		for (j = 0; j < n_pty_wr; j++)
		{
			if (args->ou)
				FD_SET(pty_wr_fds[j], args->ou);
			total++;
		}

		if (total > 0)
			break;

		/* Timeout: return 0 if deadline reached */
		if (args->tv != NULL && TICKS_AFTER(systemVitals->sysTicks, deadline))
			break;

		sched_yield();
	}

	td->td_retval[0] = total;
	return (0);
}

/*
 * sys_poll — implemented on top of the same yield loop as sys_select.
 *
 * Translates pollfd[] into lwIP fd_sets, polls non-blocking each iteration,
 * drains the kbd ring for TTY stdin fds, and writes revents on return.
 */
int sys_poll(struct thread *td, struct sys_poll_args *args)
{
	unsigned int i;
	unsigned int nfds;
	int total, max_lwip;
	fd_set lwip_rfds, lwip_wfds;
	int kern_to_lwip[MAX_FILES]; /* kernel fd → lwIP socket */
	struct timeval zero_tv;

	if (args->fds == NULL || args->nfds == 0)
	{
		td->td_retval[0] = 0;
		return (0);
	}

	nfds = args->nfds;

	/* clear all revents */
	for (i = 0; i < nfds; i++)
		args->fds[i].revents = 0;

	FD_ZERO(&lwip_rfds);
	FD_ZERO(&lwip_wfds);
	memset(kern_to_lwip, -1, sizeof(kern_to_lwip));
	max_lwip = 0;

	/* build lwIP fd_sets from poll events */
	for (i = 0; i < nfds; i++)
	{
		int kfd = args->fds[i].fd;
		short ev = args->fds[i].events;
		if (kfd < 0 || kfd >= MAX_FILES)
			continue;
		if (kfd == 0)
			continue; /* stdin handled separately */
		struct file *f = td->o_files[kfd];
		if (f && f->fd_type == FD_TYPE_SOCKET)
		{
			kern_to_lwip[kfd] = f->socket;
			if (ev & (POLLIN | POLLRDNORM))
				FD_SET(f->socket, &lwip_rfds);
			if (ev & (POLLOUT | POLLWRNORM))
				FD_SET(f->socket, &lwip_wfds);
			if (f->socket + 1 > max_lwip)
				max_lwip = f->socket + 1;
		}
	}

	/* A pty (FD_TYPE_TTYV) on any pollfd — incl. fd 0 in the GUI terminal — is
	 * checked via its own line-discipline buffer in the loop below, not the global
	 * console hook (which tracks the serial/VGA console, not an arbitrary pty). */

	zero_tv.tv_sec = 0;
	zero_tv.tv_usec = 0;

	/* deadline in sysTicks; 0 = infinite (-1 timeout), else absolute tick */
	u_int32_t deadline = 0;
	if (args->timeout > 0)
		deadline = systemVitals->sysTicks + (u_int32_t)args->timeout * PIT_TIMER / 1000 + 1;

	for (;;)
	{
		total = 0;

		/* stdin readiness: the tty layer drains the console + reports a complete
		 * line (path B hook; no-op where there's no TTY layer).  Only fd 0 backed by
		 * the console placeholder (not a pty — that is handled per-fd below). */
		if (g_console_stdin_ready != NULL && g_console_stdin_ready())
		{
			for (i = 0; i < nfds; i++)
			{
				struct file *f0 = (args->fds[i].fd == 0) ? td->o_files[0] : NULL;
				if (args->fds[i].fd == 0 && (f0 == NULL || f0->fd_type != FD_TYPE_TTYV) &&
				    (args->fds[i].events & (POLLIN | POLLRDNORM)))
				{
					args->fds[i].revents |= POLLIN;
					total++;
				}
			}
		}

		/* pty (FD_TYPE_TTYV) readiness: a complete line in the pty's own line
		 * discipline (the GUI terminal's stdin path). */
		for (i = 0; i < nfds; i++)
		{
			int kfd = args->fds[i].fd;
			struct file *f = (kfd >= 0 && kfd < MAX_FILES) ? td->o_files[kfd] : NULL;
			if (f != NULL && f->fd_type == FD_TYPE_TTYV && (args->fds[i].events & (POLLIN | POLLRDNORM)))
			{
				tty_term *tt = (tty_term *)f->data;
				if (tt != NULL && tt->stdinSize > 0)
				{
					args->fds[i].revents |= POLLIN;
					total++;
				}
			}
		}

		/* socket readiness via the socket-select hook. */
		if (max_lwip > 0 && g_socket_select != NULL)
		{
			fd_set tmp_r = lwip_rfds;
			fd_set tmp_w = lwip_wfds;
			int r = g_socket_select(max_lwip, &tmp_r, &tmp_w, NULL, &zero_tv);
			if (r > 0)
			{
				for (i = 0; i < nfds; i++)
				{
					int kfd = args->fds[i].fd;
					if (kfd < 0 || kfd >= MAX_FILES || kern_to_lwip[kfd] < 0)
						continue;
					int ls = kern_to_lwip[kfd];
					if (FD_ISSET(ls, &tmp_r))
					{
						args->fds[i].revents |= POLLIN;
						total++;
					}
					if (FD_ISSET(ls, &tmp_w))
					{
						args->fds[i].revents |= POLLOUT;
						total++;
					}
				}
			}
		}

		if (total > 0)
			break;

		/* timeout == 0: non-blocking, return immediately */
		if (args->timeout == 0)
			break;

		/* positive timeout: check deadline */
		if (args->timeout > 0 && TICKS_AFTER(systemVitals->sysTicks, deadline))
			break;

		sched_yield();
	}

	td->td_retval[0] = total;
	return (0);
}

int dup2(struct thread *td, u_int32_t from, u_int32_t to)
{
	return duplicate_descriptor(td, from, to);
}

int sys_dup2(struct thread *td, struct sys_dup2_args *args)
{
	int error = 0x0;
	int ret = dup2(td, args->from, args->to);

	td->td_retval[0] = ret;
	if (ret < 0)
		error = ret;

	return (error);
}

int sys_dup(struct thread *td, struct sys_dup_args *args)
{
	int i;

	if (args->fd >= O_FILES || td->o_files[args->fd] == NULL)
	{
		td->td_retval[0] = -1;
		return (EBADF);
	}

	/* Find lowest available fd slot. */
	for (i = 0; i < O_FILES; i++)
	{
		if (td->o_files[i] == NULL)
			break;
	}
	if (i >= O_FILES)
	{
		td->td_retval[0] = -1;
		return (EMFILE);
	}

	if (dup2(td, args->fd, (u_int32_t)i) == -1)
	{
		td->td_retval[0] = -1;
		return (EBADF);
	}

	td->td_retval[0] = i;
	return (0);
}
