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
 * mixer.hh — sums the active streams into one output period.  Single
 * responsibility: blend.  Per-stream gain is applied here (software, Q8);
 * master volume is the codec's hardware control (AudioSink), not re-applied.
 * Phase 1 with one stream is just the N=1 case of the same sum.
 */

#ifndef _AURAL_MIXER_HH
#define _AURAL_MIXER_HH

#include <stdint.h>
#include "aural_config.hh"
#include "stream_registry.hh"

namespace aural
{

class Mixer
{
      public:
	/**
	 * Mix one period of @p frames frames from every active stream into @p out.
	 * Streams shorter than a full period contribute silence for the remainder
	 * (an underrunning client never stalls the others).
	 *
	 * @param out  destination for @p frames * CHANNELS interleaved s16 samples.
	 * @return true if at least one stream was active (so the caller should write
	 *         this period to the codec); false when nothing is playing.
	 */
	bool mix(StreamRegistry &reg, int16_t *out, unsigned frames);

      private:
	int32_t accum_[PERIOD_SAMPLES];
	int16_t scratch_[PERIOD_SAMPLES];
};

} /* namespace aural */

#endif /* _AURAL_MIXER_HH */
