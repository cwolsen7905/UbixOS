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

#include "mixer.hh"

#include <string.h>

namespace aural
{

bool Mixer::mix(StreamRegistry &reg, int16_t *out, unsigned frames)
{
	if (frames > PERIOD_FRAMES)
		frames = PERIOD_FRAMES;

	unsigned samples = frames * CHANNELS;
	unsigned bytes = frames * FRAME_BYTES;

	memset(accum_, 0, samples * sizeof(accum_[0]));

	bool any_active = false;
	Stream *slots = reg.slots();
	for (unsigned s = 0; s < StreamRegistry::slot_count(); s++)
	{
		Stream &st = slots[s];
		if (!st.in_use || !st.active || !st.primed)
			continue; /* unprimed streams keep buffering — don't drain yet */

		any_active = true;

		uint32_t got = aural_ring_read(st.ring, scratch_, bytes);
		unsigned got_samples = got / BYTES_PER_SAMPLE;
		int32_t gain = (int32_t)st.gain;

		for (unsigned i = 0; i < got_samples; i++)
			accum_[i] += ((int32_t)scratch_[i] * gain) >> 8;
	}

	/* Clamp the full period to s16; samples no stream supplied stay silent. */
	for (unsigned i = 0; i < samples; i++)
	{
		int32_t v = accum_[i];
		if (v > 32767)
			v = 32767;
		else if (v < -32768)
			v = -32768;
		out[i] = (int16_t)v;
	}

	return any_active;
}

} /* namespace aural */
