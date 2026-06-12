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
 * aural_server.hh — the aural mixer server: owns the "aural" MPI mailbox, the
 * audio sink, the stream registry, and the mixer, and runs the control + mix
 * loop.  The audio twin of bin/views' compositor loop.
 */

#ifndef _AURAL_SERVER_HH
#define _AURAL_SERVER_HH

#include <stdint.h>
#include <ubix/mailbox.hh>
#include <audio/aural_proto.h>
#include "aural_config.hh"
#include "audio_sink.hh"
#include "stream_registry.hh"
#include "mixer.hh"

namespace aural
{

class AuralServer
{
      public:
	AuralServer() = default;
	AuralServer(const AuralServer &) = delete;
	AuralServer &operator=(const AuralServer &) = delete;

	/**
	 * Create the mailbox, open the codec, and run the control + mix loop
	 * forever.  @return non-zero only if startup fails.
	 */
	int run();

      private:
	void dispatch(uint32_t header, void *data);
	void handle_claim(struct aural_claim_req *req);
	void handle_release(struct aural_stream_ref *ref);
	void handle_active(struct aural_stream_ref *ref, bool active);
	void handle_gain(struct aural_gain *g);
	void handle_master(struct aural_master *m);
	void handle_list(struct aural_list_req *req);

	ubix::Mailbox mbox_;
	AudioSink sink_;
	StreamRegistry reg_;
	Mixer mixer_;
	int16_t period_[PERIOD_SAMPLES];
	int reap_ctr_ = 0;
};

} /* namespace aural */

#endif /* _AURAL_SERVER_HH */
