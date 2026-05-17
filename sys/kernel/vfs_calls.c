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
#include <fs/vfs/file.h>
#include "../fs/fat/fat_filelib.h"

#define FD_TYPE_DIR 4

int sys_open(struct thread *td, struct sys_open_args *args)
{
	return (kern_openat(td, AT_FDCWD, args->path, args->flags, args->mode));
}

int sys_openat(struct thread *td, struct sys_openat_args *args)
{

	int error = 0x0;
	int fd = 0x0;
	struct file *nfp = 0x0;

	error = falloc(td, &nfp, &fd);

	if (error)
		return (error);

	if ((args->flag & O_WRONLY) == O_WRONLY)
		nfp->fd = fopen(args->path, "w");
	else if ((args->flag & O_RDWR) == O_RDWR)
		nfp->fd = fopen(args->path, "a");
	else
		nfp->fd = fopen(args->path, "r");

	if (nfp->fd == 0x0)
	{
		if (fdestroy(td, nfp, fd) != 0x0)
			kprintf("[%s:%i] fdestroy() failed.", __FILE__, __LINE__);

		td->td_retval[0] = -1;
		return (ENOENT);
	}

	td->td_retval[0] = fd;
	return (0);
}

int sys_close(struct thread *td, struct sys_close_args *args)
{
	struct file *fd = 0x0;
	struct pipeInfo *pFD = 0x0;

	getfd(td, &fd, args->fd);

	// kprintf("[sC:%i:0x%X:0x%X]", args->fd, fd, fd->fd);

#ifdef DEBUG_VFS_CALLS
	kprintf("[sC::0x%X:0x%X]", args->fd, fd, fd->fd);
#endif

	if (fd == 0x0)
	{
		kprintf("COULDN'T FIND FD: ", args->fd);
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
			pFD = fd->data;
			if (args->fd == pFD->rFD)
			{
				if (pFD->rfdCNT < 2)
					if (fdestroy(td, fd, args->fd) != 0x0)
						kprintf("[%s:%i] fdestroy() failed.", __FILE__, __LINE__);
				pFD->rfdCNT--;
			}

			if (args->fd == pFD->wFD)
			{
				if (pFD->wfdCNT < 2)
					if (fdestroy(td, fd, args->fd) != 0x0)
						kprintf("[%s:%i] fdestroy() failed.", __FILE__, __LINE__);
				pFD->wfdCNT--;
			}

			break;
		default:
			if (args->fd < 3)
				td->td_retval[0] = 0;
			else
			{
				if (fclose(fd->fd) != 0)
					td->td_retval[0] = -1;

				// kprintf("DESTROY: %i!", args->fd);
				if (fdestroy(td, fd, args->fd) != 0x0)
					kprintf("[%s:%i] fdestroy(0x%X, 0x%X) failed\n", __FILE__, __LINE__, fd, td->o_files[args->fd]);

				td->td_retval[0] = 0;
			}
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

	struct pipeInfo *pFD = 0x0;
	struct pipeBuf *rpFD = 0x0;

	size_t nbytes;

	int rpCNT = 0;

	getfd(td, &fd, args->fd);

	/* Check fd_type first so dup2'd pipe fds work on stdin slot (0-3) */
	if (fd != 0x0 && fd->fd_type == 3)
	{
		pFD = fd->data;
		if (fd->f_flag & O_NONBLOCK)
		{
			/* Non-blocking: return 0 immediately if no data */
			if (pFD->bCNT == 0)
			{
				td->td_retval[0] = 0;
				return (0);
			}
		}
		else
		{
			/* Blocking: wait until data arrives */
			while (pFD->bCNT == 0)
				sched_yield();
		}
		{
			nbytes = (args->nbyte - (pFD->headPB->nbytes - pFD->headPB->offset) <= 0) ? args->nbyte : (pFD->headPB->nbytes - pFD->headPB->offset);
			memcpy(args->buf, pFD->headPB->buffer + pFD->headPB->offset, nbytes);
			pFD->headPB->offset += nbytes;

			if (pFD->headPB->offset >= pFD->headPB->nbytes)
			{
				rpFD = pFD->headPB;
				pFD->headPB = pFD->headPB->next;
				kfree(rpFD);
				pFD->bCNT--;
			}

			td->td_retval[0] = nbytes;
		}
	}
	else if (args->fd > 3)
	{
		td->td_retval[0] = fread(args->buf, 1, args->nbyte, fd->fd);
	}
	else if (_current->term != NULL && _current->term->t_type == TTY_TYPE_SERIAL)
	{
		/* Serial TTY (ttyd session): drain rx ring through line discipline,
		 * then read completed characters from the term's stdin buffer.
		 * Mirrors the VGA path: kbd_ring → tty_inject → stdin[]. */
		while (x < (int)args->nbyte)
		{
			int raw;
			while ((raw = serial_rx_getbyte()) >= 0)
				tty_inject(_current->term, (char)raw);

			if (_current->term->stdinSize > 0)
			{
				int i;
				c = _current->term->stdin[0];
				_current->term->stdinSize--;
				for (i = 0; i < _current->term->stdinSize; i++)
					_current->term->stdin[i] = _current->term->stdin[i + 1];
				buf[x++] = c;
				if (c == '\n' || x >= (int)args->nbyte)
					break;
			}
			else
			{
				sched_yield();
			}
		}
		td->td_retval[0] = x;
	}
	else
	{
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
			if (vesa_text_slot >= 0)
			{
				int slot = vesa_text_slot;
				vesa_text_slot = -1;
				tty_change((uInt16)slot);
				vesa_text_mode();
				kbd_gui_mode = 0;
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
				if (c == '\n' || x >= (int)args->nbyte)
					break;
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
			c = getchar();

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
			buf[x++] = '\n';

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

	struct pipeInfo *pFD = 0x0;
	struct pipeBuf *pBuf = 0x0;

	size_t nbytes;

	getfd(td, &fd, uap->fd);

	/* Check fd_type first so dup2'd pipe fds work on stdout/stderr slots */
	if (fd != 0x0 && fd->fd_type == 3)
	{
		pFD = fd->data;
		pBuf = (struct pipeBuf *)kmalloc(sizeof(struct pipeBuf));
		pBuf->buffer = kmalloc(uap->nbyte);
		pBuf->next = 0x0;
		pBuf->offset = 0;

		memcpy(pBuf->buffer, uap->buf, uap->nbyte);

		pBuf->nbytes = uap->nbyte;

		if (pFD->tailPB)
			pFD->tailPB->next = pBuf;

		pFD->tailPB = pBuf;

		if (!pFD->headPB)
			pFD->headPB = pBuf;

		pFD->bCNT++;

		td->td_retval[0] = uap->nbyte;
	}
	else if (uap->fd == 2)
	{
		buffer = kmalloc(uap->nbyte + 1);
		memset(buffer, '\0', uap->nbyte + 1);
		memcpy(buffer, uap->buf, uap->nbyte);
		if (_current->term != NULL && _current->term->t_type == TTY_TYPE_SERIAL)
		{
			size_t i;
			for (i = 0; i < uap->nbyte; i++)
				rs232_putc(buffer[i]);
		}
		else if (_current->term != NULL)
		{
			tty_print(buffer, _current->term);
		}
		else
		{
			kprintf("%s", buffer);
		}
		kfree(buffer);
		td->td_retval[0] = uap->nbyte;
	}
	else if (uap->fd == 1)
	{
		buffer = kmalloc(uap->nbyte + 1);
		memset(buffer, '\0', uap->nbyte + 1);
		memcpy(buffer, uap->buf, uap->nbyte);
		if (_current->term != NULL && _current->term->t_type == TTY_TYPE_SERIAL)
		{
			size_t i;
			for (i = 0; i < uap->nbyte; i++)
				rs232_putc(buffer[i]);
		}
		else if (_current->term != NULL)
		{
			tty_print(buffer, _current->term);
		}
		else
		{
			kprintf("%s", buffer);
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
			td->td_retval[0] = uap->nbyte;
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
	uint8_t *buf;
	uint32_t remaining, total, namelen, reclen;

	getfd(td, &fd, args->fd);

	if (fd == NULL || fd->fd_type != FD_TYPE_DIR)
	{
		td->td_retval[0] = -1;
		return (EBADF);
	}

	buf = (uint8_t *)args->buf;
	remaining = args->count;
	total = 0;

	while (remaining > 0)
	{
		if (vfs_readdir((kDIR_t *)fd->data, &kent) != 0)
			break;

		namelen = strlen(kent.d_name);
		/*
		 * linux_dirent64 layout (musl/Linux i386 ABI):
		 *   d_ino    uint64_t  offset 0  (8 bytes)
		 *   d_off    int64_t   offset 8  (8 bytes)
		 *   d_reclen uint16_t  offset 16 (2 bytes)
		 *   d_type   uint8_t   offset 18 (1 byte)
		 *   d_name   char[]    offset 19 (variable, NUL-terminated)
		 * Total header = 19 bytes; reclen aligned to 8.
		 */
		reclen = (19 + namelen + 1 + 7) & ~7u;

		if (reclen > remaining)
			break;

		uint8_t *p = buf + total;
		memset(p, 0, reclen);
		*(uint64_t *)(p + 0) = (uint64_t)kent.d_ino;       /* d_ino */
		*(uint64_t *)(p + 8) = (uint64_t)(total + reclen); /* d_off */
		*(uint16_t *)(p + 16) = (uint16_t)reclen;
		*(uint8_t *)(p + 18) = kent.d_type;
		memcpy(p + 19, kent.d_name, namelen + 1);

		total += reclen;
		remaining -= reclen;
	}

	td->td_retval[0] = (int)total;
	return (0);
}

int sys_readlink(struct thread *thr, struct sys_readlink_args *args)
{
	/* XXX - Need to implement readlink */

	kprintf("RL: %s:\n", args->path, args->count);

	// Return Error
	thr->td_retval[0] = 2;
	return (-1);
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
			return (EINVAL);
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
		thr->td_retval[0] = -1;
		return (error);
	}

	nfp->f_flag = flags & FMASK;

	/* Directory open: use VFS dir layer instead of fopen */
	if (oflags & O_DIRECTORY)
	{
		kDIR_t *kdir = vfs_opendir(path);
		if (kdir == NULL)
		{
			fdestroy(thr, nfp, fd);
			thr->td_retval[0] = -1;
			return (ENOTDIR);
		}
		nfp->fd_type = FD_TYPE_DIR;
		nfp->data = kdir;
		nfp->fd = NULL;
		thr->td_retval[0] = fd;
		return (0);
	}

	if ((oflags & O_WRONLY) && (oflags & O_APPEND))
		nfp->fd = fopen(path, "a");
	else if (oflags & O_WRONLY)
		nfp->fd = fopen(path, "w");
	else if (oflags & O_RDWR)
		nfp->fd = fopen(path, "rwb");
	else
		nfp->fd = fopen(path, "r");

	if (nfp->fd == 0x0)
	{
		if (fdestroy(thr, nfp, fd) != 0x0)
			kprintf("[%s:%i] fdestroy() failed.", __FILE__, __LINE__);

		thr->td_retval[0] = -1;
		return (ENOENT);
	}

	thr->td_retval[0] = fd;
	return (0);
}

int sys_unlink(struct thread *td, struct sys_unlink_args *uap)
{
	int error = 0x0;
	td->td_retval[0] = fl_remove(uap->path);
	if (td->td_retval[0] != 0x0)
		kprintf("[%s:%i]Path: %s", __FILE__, __LINE__, uap->path);
	return (error);
}
