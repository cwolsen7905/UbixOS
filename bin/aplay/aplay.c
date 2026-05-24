/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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
 * aplay — play a 440 Hz square wave for 3 seconds via /dev/audio.
 * Verifies the AC'97 driver pipeline end-to-end.
 * Format: signed 16-bit stereo little-endian PCM at 48000 Hz.
 */

#include <audio/audio.h>
#include <stdio.h>
#include <stdint.h>

#define RATE      48000
#define FREQ      440
#define DURATION  3

int
main(void)
{
	int      fd;
	int      cycle, half, i, s, total_cycles;
	int16_t  buf[RATE / FREQ * 2];   /* one full cycle, interleaved stereo */

	fd = audio_open("/dev/audio");
	if (fd < 0) {
		printf("aplay: cannot open /dev/audio\n");
		return 1;
	}

	/* Build one cycle of a 440 Hz square wave in the buffer.
	 * Each sample is a stereo pair (left, right) of int16_t.
	 * First half-cycle: +8000, second half-cycle: -8000. */
	cycle = RATE / FREQ;           /* samples per cycle */
	half  = cycle / 2;

	for (i = 0; i < cycle; i++) {
		int16_t v    = (i < half) ? 8000 : -8000;
		buf[i * 2    ] = v;        /* left */
		buf[i * 2 + 1] = v;       /* right */
	}

	printf("aplay: playing 440 Hz tone for %d seconds\n", DURATION);

	total_cycles = (RATE * DURATION) / cycle;
	for (s = 0; s < total_cycles; s++)
		audio_write(fd, buf, (int)(sizeof(buf)));

	audio_close(fd);
	printf("aplay: done\n");
	return 0;
}
