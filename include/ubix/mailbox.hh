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

#ifndef _UBIX_MAILBOX_HH
#define _UBIX_MAILBOX_HH

#include <string>

extern "C" {
#include <sys/mpi.h>
}

namespace ubix {

/* Free function: post to any named mailbox. */
inline void
post_message(const char *dest, uint32_t type, mpi_message_t &msg)
{
	mpi_postMessage(dest, type, &msg);
}

inline void
post_message(const std::string &dest, uint32_t type, mpi_message_t &msg)
{
	mpi_postMessage(dest.c_str(), type, &msg);
}

/*
 * RAII wrapper for a process-owned MPI mailbox.
 * Call assign() before create(); the destructor calls destroy().
 */
class Mailbox {
	std::string name_;
	bool        owned_ = false;

public:
	Mailbox() = default;
	Mailbox(const Mailbox &) = delete;
	Mailbox &operator=(const Mailbox &) = delete;

	void assign(std::string name) { name_ = std::move(name); }

	bool create() {
		owned_ = (mpi_createMbox(name_.c_str()) == 0);
		return owned_;
	}
	void destroy() {
		if (owned_) {
			mpi_destroyMbox(name_.c_str());
			owned_ = false;
		}
	}
	~Mailbox() { destroy(); }

	/* Non-blocking poll: returns true if a message was fetched. */
	bool try_fetch(mpi_message_t &msg) const {
		return mpi_fetchMessage(name_.c_str(), &msg) == 0;
	}

	/* Post a message to this mailbox. */
	void post(uint32_t type, mpi_message_t &msg) const {
		mpi_postMessage(name_.c_str(), type, &msg);
	}

	const char        *c_str() const { return name_.c_str(); }
	const std::string &str()   const { return name_; }
};

} /* namespace ubix */

#endif /* _UBIX_MAILBOX_HH */
