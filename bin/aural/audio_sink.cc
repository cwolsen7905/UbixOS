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

#include "audio_sink.hh"
#include "aural_config.hh"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <audio/audio.h>       /* AUDIO_* ioctl numbers only — not the veneer */
#include <audio/aural_proto.h> /* AURAL_DEFAULT_RATE */
#include <ubistry/ubistry.h>

namespace aural
{

bool AudioSink::open_device()
{
	fd_ = open(AURAL_DEVICE_PATH, O_WRONLY);
	if (fd_ < 0)
		return false;

	uint32_t rate = AURAL_DEFAULT_RATE;
	ioctl(fd_, AUDIO_SET_RATE, &rate);

	apply_saved_settings();
	return true;
}

void AudioSink::apply_saved_settings()
{
	int vol = 100; /* registry defaults: full volume, unmuted */
	int mute = 0;

	(void)ubistry_get_int("/aural/volume", &vol);
	(void)ubistry_get_int("/aural/mute", &mute);

	if (vol < 0)
		vol = 0;
	if (vol > 100)
		vol = 100;

	uint32_t v = (uint32_t)vol;
	uint32_t m = mute ? 1u : 0u;
	ioctl(fd_, AUDIO_SET_VOLUME, &v);
	ioctl(fd_, AUDIO_SET_MUTE, &m);
}

int AudioSink::write_period(const void *pcm, unsigned bytes)
{
	/* One non-blocking submission: the driver accepts a whole period or returns
	 * 0 when its ring is full.  The caller (AuralServer) tops up the ring each
	 * tick and naps in between — no busy-retry here. */
	long r = write(fd_, pcm, bytes);
	return (r < 0) ? -1 : (int)r;
}

void AudioSink::set_master(uint32_t volume, uint32_t mute, bool persist)
{
	if (volume != AURAL_MASTER_UNCHANGED)
	{
		if (volume > 100)
			volume = 100;
		ioctl(fd_, AUDIO_SET_VOLUME, &volume);
		if (persist)
			ubistry_set_int("/aural/volume", (int)volume);
	}

	if (mute != AURAL_MASTER_UNCHANGED)
	{
		mute = mute ? 1u : 0u;
		ioctl(fd_, AUDIO_SET_MUTE, &mute);
		if (persist)
			ubistry_set_int("/aural/mute", (int)mute);
	}
}

} /* namespace aural */
