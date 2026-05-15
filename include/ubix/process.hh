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

	bool spawn(const char *path) {
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
			char *argv[] = { (char *)"shell", nullptr };
			char *envp[] = { nullptr };
			::execve(path, argv, envp);
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

} /* namespace ubix */

#endif /* _UBIX_PROCESS_HH */
