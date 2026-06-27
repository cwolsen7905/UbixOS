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
 * audio_sink.hh — the one owner of the hardware /dev/audio device.  Single
 * responsibility: open the codec, push mixed PCM to it, and apply master
 * volume/mute (sourced from and persisted to the ubistry registry).  Knows only
 * the generic /dev/audio contract, so it is identical on i386 (AC97) and
 * aarch64 (virtio-sound).
 */

#ifndef _AURAL_AUDIO_SINK_HH
#define _AURAL_AUDIO_SINK_HH

#include <stdint.h>

namespace aural
{

class AudioSink
{
      public:
	AudioSink() = default;
	AudioSink(const AudioSink &) = delete;
	AudioSink &operator=(const AudioSink &) = delete;

	/**
	 * Open /dev/audio, set the v1 rate, and apply the saved master volume/mute
	 * from ubistry.  @return true on success.
	 */
	bool open_device();

	/** @return true once the device is open. */
	bool ok() const
	{
		return fd_ >= 0;
	}

	/**
	 * Write one mixed period to the codec.  Phase 1: non-blocking with a
	 * yield-retry until every byte is queued (the codec accepts a period at a
	 * time and refuses the rest, which paces the caller).  Phase 1 kernel work
	 * replaces this with a blocking write that returns on DMA-period drain.
	 *
	 * @return bytes written (== @p bytes on success), or -1 on error.
	 */
	int write_period(const void *pcm, unsigned bytes);

	/**
	 * Set the codec master volume/mute and, when @p persist, write the values
	 * back to ubistry so the choice survives a reboot.
	 */
	void set_master(uint32_t volume, uint32_t mute, bool persist);

      private:
	void apply_saved_settings(); /* read /aural/volume + /aural/mute from ubistry */

	int fd_ = -1;
};

} /* namespace aural */

#endif /* _AURAL_AUDIO_SINK_HH */
