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

#include <sys/descrip.h>
#include <sys/errno.h>

#include <sys/sysproto_posix.h>
#include <sys/thread.h>
#include <lib/kprintf.h>
#include <ubixos/endtask.h>
#include <lib/kmalloc.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>
#include <isa/rs232.h>
#include <isa/kbd.h>
#include <assert.h>
#include <sys/select.h>
#include <string.h>

/* Forward-declare lwip_select to avoid pulling in sockets.h macros. */
struct timeval;
struct fd_set;
int lwip_select(int, struct fd_set *, struct fd_set *, struct fd_set *, struct timeval *);

// XXX MrOlsen (2020-01-30) No longer needed -> static struct file *kern_files = 0x0;

int fcntl(struct thread *td, struct sys_fcntl_args *uap)
{
	struct file *fp = 0x0;

	struct file *dup_fp = 0x0;
	int i = 0;

	if (uap->fd < 0 || uap->fd >= O_FILES || td->o_files[uap->fd] == 0x0)
	{
		kprintf("fcntl: bad fd %i\n", uap->fd);
		return (-1);
	}

	fp = (struct file *)td->o_files[uap->fd];

	switch (uap->cmd)
	{
	case 17:
		/* First 5 Descriptors Are Reserved */
		for (i = 5; i < MAX_FILES; i++)
		{
			if (td->o_files[i] == 0x0)
			{
				dup_fp = (struct file *)kmalloc(sizeof(struct file));
				memcpy(dup_fp, fp, sizeof(struct file));

				td->o_files[i] = (void *)dup_fp;
				td->td_retval[0] = i;

				((struct file *)td->o_files[uap->fd])->fd->dup++;

				fclose(((struct file *)td->o_files[uap->fd])->fd);

				// td->o_files[uap->fd] = 0;

				if (fdestroy(td, fp, uap->fd) != 0x0)
					kprintf("[%s:%i] fdestroy(0x%X, 0x%X) failed\n", __FILE__, __LINE__, fp, td->o_files[uap->fd]);

				kprintf("FCNTL: %i, %i, 0x%X.", i, uap->fd, fp);

				break;
			}
		}
		break;
	case 3:
		if (uap->fd > 3)
			fp->f_flag = O_RDWR & O_ACCMODE;
		td->td_retval[0] = fp->f_flag;
		break;
	case 2:
		/* F_SETFD: store close-on-exec and other fd flags; silently accept */
		td->td_retval[0] = 0;
		break;
	case 4:
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
	memset(fp, 0, sizeof(struct file));

	/* First 5 Descriptors Are Reserved */
	for (i = 5; i < MAX_FILES; i++)
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

#include <sys/ioctl.h>
#include <ubixos/sched.h>
#include <ubixos/tty.h>

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
	kfree((void *)td->o_files[uap->fd]);
	td->o_files[uap->fd] = 0x0;
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
	kprintf("fstat: %i", uap->fd);
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
	int error = 0x0;

#ifdef DEBUG
	kprintf("[%s:%i]", __FILE__, __LINE__);
#endif

	*fp = (struct file *)td->o_files[fd];

	if (fp == 0x0)
		error = -1;

	return (error);
}

int sys_ioctl(struct thread *td, struct sys_ioctl_args *args)
{
	tty_term *term = _current->term;

	switch (args->com)
	{
	case TIOCGETA:
		if ((args->fd == 0 || args->fd == 1 || args->fd == 2) && term != NULL)
		{
			struct termios *t = (struct termios *)args->data;

			t->c_iflag = 0x2B02;
			t->c_oflag = 0x3;
			t->c_cflag = 0x4B00;

			/* Base: IEXTEN|ICANON|ISIG|ECHOCTL|ECHO|ECHOE|ECHOK|ECHOKE */
			t->c_lflag = IEXTEN | ICANON | ISIG | ECHOCTL | ECHO | ECHOE | ECHOK | ECHOKE;
			if (term->t_raw)
				t->c_lflag &= ~ICANON;
			if (!term->t_echo)
				t->c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOKE | ECHOCTL);

			t->c_cc[0] = 4;    /* VEOF */
			t->c_cc[1] = 255;  /* VEOL */
			t->c_cc[2] = 255;  /* VEOL2 */
			t->c_cc[3] = 127;  /* VERASE */
			t->c_cc[4] = 23;   /* VWERASE */
			t->c_cc[5] = 21;   /* VKILL */
			t->c_cc[6] = 18;   /* VREPRINT */
			t->c_cc[7] = 8;    /* spare */
			t->c_cc[8] = 3;    /* VINTR */
			t->c_cc[9] = 28;   /* VQUIT */
			t->c_cc[10] = 26;  /* VSUSP */
			t->c_cc[11] = 25;  /* VDSUSP */
			t->c_cc[12] = 17;  /* VSTART */
			t->c_cc[13] = 19;  /* VSTOP */
			t->c_cc[14] = 22;  /* VLNEXT */
			t->c_cc[15] = 15;  /* VDISCARD */
			t->c_cc[16] = 1;   /* VMIN */
			t->c_cc[17] = 0;   /* VTIME */
			t->c_cc[18] = 20;  /* VSTATUS */
			t->c_cc[19] = 255; /* spare */

			t->c_ispeed = 9600;
			t->c_ospeed = 9600;

			td->td_retval[0] = 0;
		}
		else
		{
			td->td_retval[0] = -1;
		}
		return (0);

	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF:
		if ((args->fd == 0 || args->fd == 1 || args->fd == 2) && term != NULL)
		{
			struct termios *t = (struct termios *)args->data;
			term->t_raw = (t->c_lflag & ICANON) ? 0 : 1;
			term->t_echo = (t->c_lflag & ECHO) ? 1 : 0;
			td->td_retval[0] = 0;
		}
		else
		{
			td->td_retval[0] = -1;
		}
		return (0);

	case TIOCGWINSZ:
	{
		struct winsize *win = (struct winsize *)args->data;
		win->ws_row = 25;
		win->ws_col = 80;
		win->ws_xpixel = 0;
		win->ws_ypixel = 0;
		td->td_retval[0] = 0;
		return (0);
	}

	default:
		kprintf("ioFD:%i:0x%X!", args->fd, args->com);
		td->td_retval[0] = -1;
		return (0);
	}
}

int sys_select(struct thread *td, struct sys_select_args *args)
{
	int i, nd, total;
	fd_set lwip_rfds, lwip_wfds;
	int kern_to_lwip_r[MAX_FILES]; /* kernel fd → lwIP socket for read set */
	int kern_to_lwip_w[MAX_FILES]; /* kernel fd → lwIP socket for write set */
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

	/* Scan read fd_set: separate stdin from socket fds */
	if (args->in) {
		for (i = 0; i < nd; i++) {
			if (!FD_ISSET(i, args->in))
				continue;
			if (i == 0) {
				has_stdin_rd = 1;
			} else {
				struct file *f = td->o_files[i];
				if (f && f->fd_type == 2) {
					kern_to_lwip_r[i] = f->socket;
					FD_SET(f->socket, &lwip_rfds);
					if (f->socket + 1 > max_lwip)
						max_lwip = f->socket + 1;
				}
			}
		}
		FD_ZERO(args->in);
	}

	/* Scan write fd_set */
	if (args->ou) {
		for (i = 0; i < nd; i++) {
			if (!FD_ISSET(i, args->ou))
				continue;
			struct file *f = (i < MAX_FILES) ? td->o_files[i] : NULL;
			if (f && f->fd_type == 2) {
				kern_to_lwip_w[i] = f->socket;
				FD_SET(f->socket, &lwip_wfds);
				if (f->socket + 1 > max_lwip)
					max_lwip = f->socket + 1;
			}
		}
		FD_ZERO(args->ou);
	}

	zero_tv.tv_sec = 0;
	zero_tv.tv_usec = 0;

	/* Poll loop: yield until stdin or a socket becomes ready */
	for (;;) {
		total = 0;

		/* Check TTY stdin readiness */
		if (has_stdin_rd && _current->term) {
			int stdin_ready = 0;
			if (_current->term->t_type == TTY_TYPE_SERIAL) {
				/* Drain serial rx ring so stdinSize is current */
				int raw;
				while ((raw = serial_rx_getbyte()) >= 0)
					tty_inject(_current->term, (char)raw);
				stdin_ready = (_current->term->stdinSize > 0);
			} else {
				/* VGA console: drain ring through tty_inject (echoes chars
				 * to screen) and report ready only when a full canonical
				 * line has been flushed to tty_foreground->stdin[]. */
				kbd_event_t _kev;
				while (kbd_getEvent(&_kev) == 0) {
					if (_kev.pressed) {
						uint32_t _kc = _kev.keycode;
						if (_kc != 0 && _kc < 0x100 && tty_foreground != NULL)
							tty_inject(tty_foreground, (char)(uint8_t)_kc);
					}
				}
				stdin_ready = (tty_foreground != NULL &&
				    tty_foreground->stdinSize > 0);
			}
			if (stdin_ready) {
				if (args->in)
					FD_SET(0, args->in);
				total++;
			}
		}

		/* Check sockets (non-blocking) */
		if (max_lwip > 0) {
			fd_set tmp_r = lwip_rfds;
			fd_set tmp_w = lwip_wfds;
			int r = lwip_select(max_lwip, &tmp_r, &tmp_w, NULL, &zero_tv);
			if (r > 0) {
				for (i = 0; i < nd; i++) {
					if (args->in && kern_to_lwip_r[i] >= 0 &&
					    FD_ISSET(kern_to_lwip_r[i], &tmp_r)) {
						FD_SET(i, args->in);
						total++;
					}
					if (args->ou && kern_to_lwip_w[i] >= 0 &&
					    FD_ISSET(kern_to_lwip_w[i], &tmp_w)) {
						FD_SET(i, args->ou);
						total++;
					}
				}
			}
		}

		if (total > 0)
			break;

		/* Zero-timeout poll: return immediately even if nothing ready */
		if (args->tv && args->tv->tv_sec == 0 && args->tv->tv_usec == 0)
			break;

		sched_yield();
	}

	td->td_retval[0] = total;
	return (0);
}

int dup2(struct thread *td, u_int32_t from, u_int32_t to)
{

	struct file *fp = 0x0;
	struct file *dup_fp = 0x0;

	if (to > MAX_FILES)
	{
		kprintf("TO: %i > MAX_FILES: %i", to, MAX_FILES);
		return (-1);
	}
	else if (td->o_files[to] != 0x0)
	{
		struct file *old = (struct file *)td->o_files[to];
		if (old->fd_type != 3 && old->fd != 0x0)
			fclose(old->fd);
		if (fdestroy(td, old, to) != 0x0)
			kprintf("[%s:%i] Error with fdestroy!", __FILE__, __LINE__);
	}

	fp = (struct file *)td->o_files[from];
	if (fp == 0x0)
	{
		kprintf("dup2: from fd %u is not open\n", from);
		return (-1);
	}
	if (from == to)
		return (0x0);

	dup_fp = (struct file *)kmalloc(sizeof(struct file));
	memcpy(dup_fp, fp, sizeof(struct file));

	td->o_files[to] = (void *)dup_fp;

	if (fp->fd != 0x0)
		fp->fd->dup++;

	return (0x0);
}

int sys_dup2(struct thread *td, struct sys_dup2_args *args)
{

	int error = 0x0;

	if ((td->td_retval[0] = dup2(td, args->from, args->to)) == -1)
		error = -1;

	return (error);
}
