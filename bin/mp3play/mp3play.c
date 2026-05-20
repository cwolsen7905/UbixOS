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
 * mp3play — minimal streaming MP3 player for UbixOS.
 * Uses minimp3 (CC0) to decode frames and streams PCM to /dev/audio
 * via the AC97 driver.  Detects sample rate from the first frame and
 * programs the codec via ioctl(AUDIO_SET_RATE) before playback begins.
 *
 * File is read in 64 KB chunks so the global FAT spinlock is held only
 * briefly per chunk rather than for the entire file duration.
 *
 * Usage: mp3play <file.mp3>
 */

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include <minimp3/minimp3.h>

#include <audio/audio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
 * Input ring-buffer sizing.
 * IBUF_SIZE: total bytes allocated for the streaming read buffer.
 * IBUF_REFILL: when fewer than this many bytes remain unprocessed,
 *   slide the remainder to the front and read another chunk.
 *   Must be > the largest possible single MP3 frame (≈ 1441 bytes at
 *   320 kbps), with margin for minimp3's sync search.
 */
#define IBUF_SIZE    (64 * 1024)
#define IBUF_REFILL   4096

int
main(int argc, char *argv[])
{
	FILE               *fp;
	uint8_t            *ibuf;
	int                 ibuf_len;   /* valid bytes in ibuf[0..ibuf_len-1] */
	int                 pos;        /* next byte to decode */
	int                 audio_fd;
	mp3dec_t            dec;
	mp3dec_frame_info_t info;
	int16_t             pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	int                 samples, rate_set;
	size_t              got;

	if (argc < 2) {
		fprintf(stderr, "usage: mp3play <file.mp3>\n");
		return (1);
	}

	fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		fprintf(stderr, "mp3play: cannot open %s\n", argv[1]);
		return (1);
	}

	ibuf = (uint8_t *)malloc(IBUF_SIZE);
	if (ibuf == NULL) {
		fprintf(stderr, "mp3play: out of memory\n");
		fclose(fp);
		return (1);
	}

	audio_fd = audio_open("devfs:/audio");
	if (audio_fd < 0) {
		fprintf(stderr, "mp3play: cannot open devfs:/audio\n");
		free(ibuf);
		fclose(fp);
		return (1);
	}

	mp3dec_init(&dec);
	ibuf_len = 0;
	pos      = 0;
	rate_set = 0;

	printf("mp3play: playing %s\n", argv[1]);

	/* Fill the buffer for the first time. */
	got = fread(ibuf, 1, IBUF_SIZE, fp);
	ibuf_len = (int)got;

	while (ibuf_len - pos > 0) {
		/*
		 * Refill when the unprocessed tail shrinks below IBUF_REFILL.
		 * Slide the remainder to the front, then append a fresh read.
		 * Each fread is at most IBUF_SIZE bytes so the FAT spinlock is
		 * held for only one chunk at a time.
		 */
		if (ibuf_len - pos < IBUF_REFILL) {
			int remain = ibuf_len - pos;
			if (remain > 0)
				memmove(ibuf, ibuf + pos, (size_t)remain);
			ibuf_len = remain;
			pos      = 0;
			got = fread(ibuf + ibuf_len, 1,
			    (size_t)(IBUF_SIZE - ibuf_len), fp);
			ibuf_len += (int)got;
			if (ibuf_len == 0)
				break;
		}

		samples = mp3dec_decode_frame(&dec,
		    ibuf + pos, ibuf_len - pos, pcm, &info);

		if (info.frame_bytes == 0) {
			/* No sync found in remaining data — skip one byte and retry. */
			pos++;
			continue;
		}

		pos += info.frame_bytes;

		if (samples == 0)
			continue;   /* side-info only frame, no PCM yet */

		/* Program the codec rate from the first real frame. */
		if (!rate_set) {
			uint32_t hz = (uint32_t)info.hz;
			audio_set_rate(audio_fd, hz);
			printf("mp3play: %d Hz, %d ch\n", info.hz, info.channels);
			rate_set = 1;
		}

		/*
		 * minimp3 produces info.channels interleaved samples per
		 * channel.  AC97 ring expects stereo; mono is upmixed inline.
		 */
		if (info.channels == 2) {
			audio_write(audio_fd, pcm,
			    samples * 2 * (int)sizeof(int16_t));
		} else {
			int16_t stereo[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
			int     i;
			for (i = 0; i < samples; i++) {
				stereo[i * 2]     = pcm[i];
				stereo[i * 2 + 1] = pcm[i];
			}
			audio_write(audio_fd, stereo,
			    samples * 2 * (int)sizeof(int16_t));
		}
	}

	printf("mp3play: done\n");
	audio_close(audio_fd);
	free(ibuf);
	fclose(fp);
	return (0);
}
