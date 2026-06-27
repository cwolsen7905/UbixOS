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
 * aural_config.hh — server-internal sizing constants for the aural mixer.
 * The protocol/format constants visible to clients live in <audio/aural_proto.h>;
 * these are aural's private choices (period size, ring depth, slot count).
 */

#ifndef _AURAL_CONFIG_HH
#define _AURAL_CONFIG_HH

#define AURAL_DEVICE_PATH "/dev/audio"

namespace aural
{

/* v1 fixed format: 48 kHz, signed 16-bit, stereo, interleaved. */
enum
{
	CHANNELS = 2,
	BYTES_PER_SAMPLE = 2,
	FRAME_BYTES = CHANNELS * BYTES_PER_SAMPLE, /* 4 */

	/* Frames mixed and written to the codec per loop pass. */
	PERIOD_FRAMES = 1024,
	PERIOD_SAMPLES = PERIOD_FRAMES * CHANNELS,  /* 2048 */
	PERIOD_BYTES = PERIOD_FRAMES * FRAME_BYTES, /* 4096 */

	/* Per-stream shared ring data[] capacity — power of two, ~8 periods, so a
	 * client can buffer ahead and absorb scheduling jitter. */
	RING_CAPACITY = 32768,

	/* A stream is "primed" (starts being mixed/drained) once its ring has built
	 * up this cushion (~4 periods) — lets the device buffer fill before playback
	 * so continuous streams don't underrun. */
	PRIME_BYTES = 16384,

	/* ...but a short clip that never reaches the cushion still primes after this
	 * many wake-ticks (~10 ms each), so UI beeps aren't swallowed. */
	PRIME_GRACE = 3,

	/* Maximum simultaneous client streams. */
	MAX_STREAMS = 16,

	/* Periods to submit per wake before naping — enough to refill the driver's
	 * ring (it returns 0 / full well before this), so the device never starves
	 * between 10 ms naps. */
	AURAL_FEED_BURST = 12,

	/* Pacing nap between feed bursts (milliseconds). */
	AURAL_NAP_MS = 10,

	/* Feed bursts between dead-client reaper sweeps (~2.5 s at 10 ms). */
	REAP_INTERVAL = 256
};

} /* namespace aural */

#endif /* _AURAL_CONFIG_HH */
