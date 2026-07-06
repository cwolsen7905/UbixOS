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
 * stream_registry.hh — the set of live client streams.  Single responsibility:
 * allocate/free each stream's shared PCM ring, share it into the client, hand
 * out stream ids, and reap streams whose client has died.  The mixer reads the
 * rings; it does not own them.
 */

#ifndef _AURAL_STREAM_REGISTRY_HH
#define _AURAL_STREAM_REGISTRY_HH

#include <stdint.h>
#include <audio/aural_proto.h>
#include "aural_config.hh"

namespace aural
{

/** One client playback stream and its server-side view of the shared ring. */
struct Stream
{
	bool in_use = false;              /* slot allocated                         */
	bool active = false;              /* draining into the mix (START/STOP)     */
	bool primed = false;              /* ring cushion built — ok to start mixing */
	uint32_t prime_wait = 0;          /* wakes with data while waiting to prime  */
	uint32_t id = 0;                  /* opaque handle returned in AURAL_ACK    */
	int32_t client_pid = -1;          /* owning process — reaped via kill(pid,0)*/
	uint32_t gain = AURAL_GAIN_UNITY; /* per-stream Q8 gain                */
	void *region = nullptr;           /* aural-side base of the shared region   */
	uint32_t region_bytes = 0;        /* shared region size (header + data)     */
	aural_ring *ring = nullptr;       /* == region (header sits at the front)   */
	char name[AURAL_NAME_MAX] = {0};  /* client display name (mixer flyout)     */
};

/**
 * Owns the fixed table of streams.  Claim allocates a ring and shares it to the
 * client; release/reap free it.  Non-copyable (it owns OS resources).
 */
class StreamRegistry
{
      public:
	StreamRegistry() = default;
	StreamRegistry(const StreamRegistry &) = delete;
	StreamRegistry &operator=(const StreamRegistry &) = delete;

	/**
	 * Allocate a stream, share its PCM ring into @p pid, and return the slot.
	 *
	 * @param client_vaddr  out: the ring's address in the CLIENT's space (for AURAL_ACK).
	 * @param region_bytes  out: total shared region size.
	 * @param capacity      out: the ring data[] capacity.
	 * @param nak_reason    out: set to an AURAL_NAK_* code when the return is nullptr.
	 * @return the claimed Stream, or nullptr on rejection (see nak_reason).
	 */
	Stream *claim(int32_t pid,
	              uint32_t rate,
	              uint32_t channels,
	              uint32_t format,
	              uintptr_t &client_vaddr,
	              uint32_t &region_bytes,
	              uint32_t &capacity,
	              uint32_t &nak_reason);

	/** Free the stream with @p id (unmap ring + clear slot). @return true if found. */
	bool release(uint32_t id);

	/** Free every stream whose owning process is gone (kill(pid,0) fails). */
	void reap();

	/** @return the stream with @p id, or nullptr. */
	Stream *find(uint32_t id);

	/** Direct access for the mixer's active-stream sweep. */
	Stream *slots()
	{
		return slots_;
	}

	/**
	 * @return the number of allocated (in_use) streams.  Zero means no client
	 * holds an open stream — the server has nothing to poll and can block on
	 * its mailbox rather than wake on the pacing tick.
	 */
	unsigned live_count() const
	{
		unsigned n = 0;
		for (unsigned i = 0; i < MAX_STREAMS; i++)
			if (slots_[i].in_use)
				n++;
		return n;
	}
	static unsigned slot_count()
	{
		return MAX_STREAMS;
	}

      private:
	Stream slots_[MAX_STREAMS];
	uint32_t next_id_ = 1;
};

} /* namespace aural */

#endif /* _AURAL_STREAM_REGISTRY_HH */
