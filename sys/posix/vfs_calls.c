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
#include <i386/signal.h>
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
/* fat_filelib.h removed — use VFS unlink() for file removal */

/* Forward-declare lwIP read/write so we can call them for socket fds. */
int lwip_send(int s, const void *dataptr, size_t size, int flags);
int lwip_recv(int s, void *mem, size_t len, int flags);
int lwip_close(int s);

/* True when an unblocked signal is pending — used to make blocking I/O
 * loops interruptible.  td must be struct thread *. */
#define SIG_PENDING_UNBLOCKED(td) ((td)->sig_pending & ~(td)->sigmask.__bits[0])

#define FD_TYPE_DIR 4

int sys_open(struct thread *td, struct sys_open_args *args)
{
	return (kern_openat(td, AT_FDCWD, args->path, args->flags, args->mode));
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
			case 2:
				/* Socket fd: tear down the lwIP netconn before freeing the
				 * descriptor.  Without this, every closed socket leaks its
				 * netconn (and lwIP socket slot) — the small MEMP_NUM_NETCONN
				 * pool is exhausted after a few connections and socket()
				 * starts failing. */
				lwip_close(fd->socket);
				if (fdestroy(td, fd, args->fd) != 0)
					klog(KLOG_ERR, "sys_close: fdestroy failed for socket fd %d", args->fd);
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
	int x = 0;
	char c = 0x0;
	char bf[2];
	volatile char *buf = args->buf;

	struct file *fd = 0x0;

	struct pipeInfo *p_fd = 0x0;
	struct pipeBuf *rp_fd = 0x0;

	size_t nbytes;

	int rp_cnt = 0;

	getfd(td, &fd, args->fd);

	/* Socket fd: route through lwIP */
	if (fd != NULL && fd->fd_type == 2)
	{
		void *kbuf = kmalloc(args->nbyte);
		if (!kbuf)
		{
			td->td_retval[0] = -1;
			return (-1);
		}
		int r = lwip_recv(fd->socket, kbuf, args->nbyte, 0);
		if (r > 0)
		{
			memcpy((void *)args->buf, kbuf, (size_t)r);
		}
		kfree(kbuf);
		td->td_retval[0] = r;
		return (0);
	}

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
			/* Blocking: wait until data arrives, the writer side closes,
			 * or a signal fires. */
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
				if (SIG_PENDING_UNBLOCKED(td))
				{
					p_fd->reader_pid = 0;
					td->td_retval[0] = -EINTR;
					return (EINTR);
				}
				sched_yield();
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
	else if (fd->fd_type == FD_TYPE_TTYV)
	{
		/* Specific virtual terminal fd: read from the bound tty_term. */
		tty_term *t = (tty_term *)fd->data;
		/* Phase 12: background process reading from its controlling tty */
		if (t != NULL && t->t_pgrp != 0 && (pid_t)_current->pgrp != t->t_pgrp)
		{
			signal_post_pgrp((pid_t)_current->pgrp, SIGTTIN);
			td->td_retval[0] = -EINTR;
			return (EINTR);
		}
		for (;;)
		{
			if (SIG_PENDING_UNBLOCKED(td))
			{
				td->td_retval[0] = -EINTR;
				return (EINTR);
			}
			if (t->stdinSize > 0)
			{
				int i;
				c = t->stdin[0];
				t->stdinSize--;
				for (i = 0; i < t->stdinSize; i++)
				{
					t->stdin[i] = t->stdin[i + 1];
				}
				buf[x++] = c;
				if (c == '\n' || x >= (int)args->nbyte)
				{
					break;
				}
			}
			else if (t->t_eof)
			{
				t->t_eof = 0;
				break; /* return x bytes (0 if line was empty = EOF) */
			}
			else
			{
				sched_yield();
			}
		}
		td->td_retval[0] = x;
	}
	else if (fd->fd != NULL)
	{
		/* Regular file with a fileDescriptor_t: read via VFS */
		td->td_retval[0] = fread(args->buf, 1, args->nbyte, fd->fd);
	}
	/* else: fd->fd == NULL = TTY placeholder (original fds 0-3 or dup2'd copy) */
	else if (_current->term != NULL && _current->term->t_type == TTY_TYPE_SERIAL)
	{
		/* Serial TTY (ttyd session): drain rx ring through line discipline,
		 * then read completed characters from the term's stdin buffer.
		 * Mirrors the VGA path: kbd_ring → tty_inject → stdin[]. */
		while (x < (int)args->nbyte)
		{
			int raw;
			while ((raw = serial_rx_getbyte()) >= 0)
			{
				tty_inject(_current->term, (char)raw);
			}

			if (_current->term->stdinSize > 0)
			{
				int i;
				c = _current->term->stdin[0];
				_current->term->stdinSize--;
				for (i = 0; i < _current->term->stdinSize; i++)
				{
					_current->term->stdin[i] = _current->term->stdin[i + 1];
				}
				buf[x++] = c;
				if (c == '\n' || x >= (int)args->nbyte)
				{
					break;
				}
			}
			else if (_current->term->t_eof)
			{
				_current->term->t_eof = 0;
				break; /* return x bytes (0 if line was empty = EOF) */
			}
			else
			{
				if (SIG_PENDING_UNBLOCKED(td))
				{
					td->td_retval[0] = -EINTR;
					return (EINTR);
				}
				sched_yield();
			}
		}
		td->td_retval[0] = x;
	}
	else
	{
		/* Phase 12: background process reading from its controlling tty */
		{
			tty_term *t_bg = _current->ct_tty ? _current->ct_tty : _current->term;
			if (t_bg != NULL && t_bg->t_pgrp != 0 && (pid_t)_current->pgrp != t_bg->t_pgrp)
			{
				signal_post_pgrp((pid_t)_current->pgrp, SIGTTIN);
				td->td_retval[0] = -EINTR;
				return (EINTR);
			}
		}
		/* VGA TTY: block until this terminal is the active foreground,
		 * then drain keyboard ring → line discipline → stdin[].
		 *
		 * When another VTY is switched in (Alt+Fx) or the GUI compositor
		 * has mapped the framebuffer (kbd_gui_mode=1), yield here rather
		 * than returning 0 bytes — returning 0 causes login to spin and
		 * spam "Login:" at full CPU speed.
		 *
		 * vesa_text_slot: deferred Ctrl+Alt+Fn from the keyboard ISR.
		 * The ISR cannot call biosCall (spawns a V86 task, needs sched_yield),
		 * so it sets the flag and we perform the switch here in task context. */
		for (;;)
		{
			if (SIG_PENDING_UNBLOCKED(td))
			{
				td->td_retval[0] = -EINTR;
				return (EINTR);
			}
			if (vesa_text_slot >= 0)
			{
				int slot = vesa_text_slot;
				vesa_text_slot = -1;
				tty_change((u_int16_t)slot);
				vesa_text_mode();
				kbd_gui_mode = 0;
			}
			if (tty_switch_slot >= 0)
			{
				int slot = tty_switch_slot;
				tty_switch_slot = -1;
				tty_change((u_int16_t)slot);
			}
			if (kbd_gui_mode || _current->term != tty_foreground)
			{
				sched_yield();
				continue;
			}
			c = getchar();
			if (c != 0x0)
			{
				buf[x++] = c;
				if (c == '\n' || c == '\r' || x >= (int)args->nbyte)
				{
					break;
				}
			}
			else if (tty_foreground != NULL && tty_foreground->t_eof)
			{
				tty_foreground->t_eof = 0;
				break; /* return x bytes (0 if line was empty = EOF) */
			}
			else
			{
				sched_yield();
			}
		}
		td->td_retval[0] = x;
	}
	return (0);
}

int sys_pread(struct thread *td, struct sys_pread_args *args)
{
	int offset = 0;
	int x = 0;
	char c = 0x0;
	char bf[2];
	volatile char *buf = args->buf;

	struct file *fd = 0x0;

	getfd(td, &fd, args->fd);

	if (fd == NULL)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	if (args->fd > 3)
	{
		offset = fd->fd->offset;
		fd->fd->offset = args->offset;
		td->td_retval[0] = fread(args->buf, args->nbyte, 1, fd->fd);
		fd->fd->offset = offset;
	}
	else
	{
		bf[1] = '\0';
		if (_current->term == tty_foreground)
		{
			c = getchar();
		}

		for (x = 0; x < args->nbyte && c != '\n';)
		{
			if (_current->term == tty_foreground)
			{

				if (c != 0x0)
				{
					buf[x++] = c;
					bf[0] = c;
					kprintf(bf);
				}

				if (c == '\n')
				{
					buf[x++] = c;
					break;
				}

				sched_yield();
				c = getchar();
			}
			else
			{
				sched_yield();
			}
		}
		if (c == '\n')
		{
			buf[x++] = '\n';
		}

		bf[0] = '\n';
		kprintf(bf);

		td->td_retval[0] = x;
	}
	return (0);
}

int sys_write(struct thread *td, struct sys_write_args *uap)
{
	char *buffer = 0x0;
	struct file *fd = 0x0;

	struct pipeInfo *p_fd = 0x0;
	struct pipeBuf *p_buf = 0x0;

	size_t nbytes;

	getfd(td, &fd, uap->fd);

	/* Socket fd: route through lwIP */
	if (fd != NULL && fd->fd_type == 2)
	{
		void *kbuf = kmalloc(uap->nbyte);
		if (!kbuf)
		{
			td->td_retval[0] = -1;
			return (-1);
		}
		memcpy(kbuf, uap->buf, uap->nbyte);
		int r = lwip_send(fd->socket, kbuf, uap->nbyte, 0);
		kfree(kbuf);
		td->td_retval[0] = (r >= 0) ? r : -1;
		return (0);
	}

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

		/* Phase 3.2: boost the blocked reader so it runs before CPU-bound tasks. */
		if (p_fd->reader_pid != 0)
		{
			kTask_t *reader = schedFindTask((u_int32_t)p_fd->reader_pid);
			if (reader != NULL)
				sched_io_wakeup(reader);
		}

		td->td_retval[0] = uap->nbyte;
	}
	else if (fd != NULL && fd->fd_type == FD_TYPE_TTYV)
	{
		/* Specific virtual terminal fd: write to the bound tty_term. */
		tty_term *t = (tty_term *)fd->data;
		/* Phase 12: background process writing to its controlling tty */
		if (t != NULL && t->t_pgrp != 0 && (pid_t)_current->pgrp != t->t_pgrp &&
		    (t->t_termios.c_lflag & TOSTOP))
		{
			signal_post_pgrp((pid_t)_current->pgrp, SIGTTOU);
			td->td_retval[0] = -EINTR;
			return (EINTR);
		}
		buffer = kmalloc(uap->nbyte + 1);
		if (!buffer)
		{
			td->td_retval[0] = -1;
			return (-1);
		}
		memset(buffer, '\0', uap->nbyte + 1);
		memcpy(buffer, uap->buf, uap->nbyte);
		if (t != NULL && t->t_type == TTY_TYPE_SERIAL)
		{
			size_t i;
			for (i = 0; i < uap->nbyte; i++)
			{
				rs232_putc(buffer[i]);
			}
		}
		else if (t != NULL)
		{
			tty_print(buffer, t);
		}
		kfree(buffer);
		td->td_retval[0] = uap->nbyte;
	}
	else if (fd != NULL && fd->fd == NULL)
	{
		/* TTY placeholder: original fds 1/2 or any dup2'd copy thereof */
		/* Phase 12: background process writing to its controlling tty */
		{
			tty_term *t_bg = _current->ct_tty ? _current->ct_tty : _current->term;
			if (t_bg != NULL && t_bg->t_pgrp != 0 && (pid_t)_current->pgrp != t_bg->t_pgrp &&
			    (t_bg->t_termios.c_lflag & TOSTOP))
			{
				signal_post_pgrp((pid_t)_current->pgrp, SIGTTOU);
				td->td_retval[0] = -EINTR;
				return (EINTR);
			}
		}
		buffer = kmalloc(uap->nbyte + 1);
		if (!buffer)
		{
			td->td_retval[0] = -1;
			return (-1);
		}
		memset(buffer, '\0', uap->nbyte + 1);
		memcpy(buffer, uap->buf, uap->nbyte);
		{
			tty_term *t_out = _current->ct_tty ? _current->ct_tty : _current->term;
			if (t_out != NULL && t_out->t_type == TTY_TYPE_SERIAL)
			{
				size_t i;
				for (i = 0; i < uap->nbyte; i++)
					rs232_putc(buffer[i]);
			}
			else if (t_out != NULL)
			{
				tty_print(buffer, t_out);
			}
			else
			{
				kprintf("%s", buffer);
			}
		}
		kfree(buffer);
		td->td_retval[0] = uap->nbyte;
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

int sys_access(struct thread *td, struct sys_access_args *args)
{
	/* XXX - This is a temporary as it always returns true */

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
		thr->td_retval[0] = fd;
		return (0);
	}

	/* /dev/console — always the system console (ttyv0). */
	if (strcmp(path, "/dev/console") == 0)
	{
		tty_term *t = tty_find(0);
		if (t == NULL)
		{
			fdestroy(thr, nfp, fd);
			thr->td_retval[0] = -ENODEV;
			return (ENODEV);
		}
		nfp->fd = NULL;
		nfp->fd_type = FD_TYPE_TTYV;
		nfp->data = t;
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
				tty_term *t = tty_find((u_int16_t)idx);
				if (t == NULL)
				{
					fdestroy(thr, nfp, fd);
					thr->td_retval[0] = ENODEV;
					return (ENODEV);
				}
				nfp->fd = NULL;
				nfp->fd_type = FD_TYPE_TTYV;
				nfp->data = t;
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
		nfp->fd = fopen(path, "rwb");
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
 * flags/data: ignored for now (pseudo-FSes need no device)
 *
 * Maps type string to the internal vfsType integer used by vfsRegisterFS,
 * then delegates to vfs_mount(). Block-device FSes (fat) are not yet
 * supported via this path — those come through the automountd Phase 6 flow.
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

	if (vfs_mount(0, 0, 0, vfs_type, mpath, fs_perms) != 0)
	{
		kprintf("sys_mount: vfs_mount(%s, %s) failed\n", uap->type, mpath);
		td->td_retval[0] = -EIO;
		return (EIO);
	}

	kprintf("sys_mount: mounted %s at %s\n", uap->type, mpath);
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
