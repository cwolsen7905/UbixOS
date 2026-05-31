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

#ifndef _UBIX_PROCESS_HH
#define _UBIX_PROCESS_HH

/*
 * C++ wrappers for POSIX process / file-descriptor primitives that have no
 * C++ standard equivalent.  Application code includes this header instead of
 * <unistd.h>, <signal.h>, and <fcntl.h> directly.
 */

/* _POSIX_SOURCE gates kill(), sigqueue() etc. in musl's signal.h */
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

extern "C" {
#include <sys/sys.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <api/ubix.h>
}

namespace ubix {

/*
 * Shell — RAII wrapper that forks and execs a shell subprocess, wiring
 * stdin/stdout to pipes so the caller can drive it interactively.
 */
class Shell {
	int   in_fd_  = -1;
	int   out_fd_ = -1;
	pid_t pid_    = -1;

public:
	Shell() = default;
	Shell(const Shell &) = delete;
	Shell &operator=(const Shell &) = delete;

	bool spawn(const char *path, char **argv = nullptr, char **envp = nullptr) {
		int to_shell[2], from_shell[2];
		if (::pipe(to_shell) != 0 || ::pipe(from_shell) != 0)
			return false;
		pid_ = ::fork();
		if (pid_ == 0) {
			::dup2(to_shell[0],   0);
			::dup2(from_shell[1], 1);
			::dup2(from_shell[1], 2);
			::close(to_shell[0]);
			::close(to_shell[1]);
			::close(from_shell[0]);
			::close(from_shell[1]);
			static char *default_argv[] = { (char *)"shell", nullptr };
			static char *default_envp[] = { nullptr };
			::execve(path,
			    argv  ? argv  : default_argv,
			    envp  ? envp  : default_envp);
			::_exit(1);
		}
		::close(to_shell[0]);
		::close(from_shell[1]);
		in_fd_  = to_shell[1];
		out_fd_ = from_shell[0];
		return pid_ > 0;
	}

	int read(char *buf, int n) const {
		return static_cast<int>(::read(out_fd_, buf, static_cast<unsigned>(n)));
	}

	void write(const char *buf, int n) const {
		if (in_fd_ >= 0)
			::write(in_fd_, buf, static_cast<unsigned>(n));
	}

	void set_nonblock() const {
		if (out_fd_ >= 0)
			::fcntl(out_fd_, F_SETFL, O_NONBLOCK);
	}

	void kill() const {
		if (pid_ > 0)
			::kill(pid_, SIGKILL);
	}

	void close_fds() {
		if (in_fd_  >= 0) { ::close(in_fd_);  in_fd_  = -1; }
		if (out_fd_ >= 0) { ::close(out_fd_); out_fd_ = -1; }
	}

	bool valid()    const { return pid_ > 0; }
	int  out_fd()   const { return out_fd_; }
};

/*
 * Pty — RAII wrapper that runs a shell on a real kernel pseudo-terminal.
 *
 * Unlike Shell (which uses pipes), the child's stdin/stdout/stderr are a tty,
 * so isatty() is true and tcsh enables line editing, history, and job control,
 * rendering exactly as on the text console.  The kernel renders the shell's
 * output into an 80x25 cell grid that the terminal reads back with snapshot();
 * keystrokes are fed in with inject().
 */
class Pty {
	int   slot_ = -1;
	pid_t pid_  = -1;

public:
	Pty() = default;
	Pty(const Pty &) = delete;
	Pty &operator=(const Pty &) = delete;

	bool spawn(const char *path, char **argv = nullptr, char **envp = nullptr) {
		slot_ = ::pty_alloc();
		if (slot_ < 0)
			return false;
		pid_ = ::fork();
		if (pid_ == 0) {
			char dev[24];
			::snprintf(dev, sizeof(dev), "/dev/ttyv%d", slot_);
			/* New session → no controlling tty; opening the pty without
			 * O_NOCTTY then claims it as the ctty with us as foreground
			 * pgrp (so Ctrl-C and reads target this shell). */
			::setsid();
			int fd = ::open(dev, O_RDWR);
			if (fd < 0)
				::_exit(127);
			::dup2(fd, 0);
			::dup2(fd, 1);
			::dup2(fd, 2);
			if (fd > 2)
				::close(fd);
			static char *default_argv[] = { (char *)"sh", nullptr };
			static char *default_envp[] = { nullptr };
			::execve(path,
			    argv ? argv : default_argv,
			    envp ? envp : default_envp);
			::_exit(127);
		}
		return pid_ > 0;
	}

	int inject(const char *buf, int n) const {
		return ::pty_inject(slot_, buf, n);
	}

	int snapshot(void *grid, unsigned short *x, unsigned short *y) const {
		return ::pty_snapshot(slot_, grid, x, y);
	}

	void kill() const {
		if (pid_ > 0)
			::kill(pid_, SIGKILL);
	}

	void release() {
		if (slot_ >= 0) {
			::pty_free(slot_);
			slot_ = -1;
		}
	}

	/* Reap the shell without blocking; true once it has exited. */
	bool exited() {
		if (pid_ <= 0)
			return true;
		int st = 0;
		int r = ::waitpid(pid_, &st, WNOHANG);
		if (r == pid_ || r < 0) { /* exited, or no such child left */
			pid_ = -1;
			return true;
		}
		return false;
	}

	bool  valid() const { return pid_ > 0; }
	pid_t pid()   const { return pid_; }
	int   slot()  const { return slot_; }
};

} /* namespace ubix */

#endif /* _UBIX_PROCESS_HH */
