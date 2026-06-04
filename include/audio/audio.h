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

#ifndef _AUDIO_AUDIO_H
#define _AUDIO_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * ioctl commands for /dev/audio.
 * arg points to a uint32_t for GET/SET rate.
 */
#define AUDIO_SET_RATE 0x4101
#define AUDIO_GET_RATE 0x4102
#define AUDIO_SET_VOLUME 0x4103 /* arg: uint32_t* master volume 0..100 */
#define AUDIO_GET_VOLUME 0x4104 /* arg: uint32_t* -> 0..100 */
#define AUDIO_SET_MUTE 0x4105   /* arg: uint32_t* 0 = unmute, 1 = mute */
#define AUDIO_GET_MUTE 0x4106   /* arg: uint32_t* -> 0/1 */

	/*
	 * Open /dev/audio (or any audio device path) for writing.
	 * Returns a file descriptor >= 0 on success, -1 on error.
	 */
	int audio_open(const char *dev);

	/*
	 * Set the PCM sample rate (Hz).  Must be called before the first write.
	 * The AC'97 codec defaults to 48000 Hz after reset; this ioctl lets
	 * applications request a different rate if the codec supports VRA.
	 * Returns 0 on success, -1 on error.
	 */
	int audio_set_rate(int fd, uint32_t rate);

	/*
	 * Master mixer controls.  Volume is 0..100; mute is 0/1.  The fd may be opened
	 * read/write or write-only.  Each returns 0 on success, -1 on error; the GET
	 * variants store the current value through the pointer.
	 */
	int audio_set_volume(int fd, uint32_t pct);
	int audio_get_volume(int fd, uint32_t *pct);
	int audio_set_mute(int fd, uint32_t mute);
	int audio_get_mute(int fd, uint32_t *mute);

	/*
	 * Write raw signed 16-bit stereo little-endian PCM samples.
	 * Returns the number of bytes accepted (may be less than n if the
	 * kernel ring buffer is full), or -1 on error.
	 */
	int audio_write(int fd, const void *buf, int n);

	/*
	 * Close the audio device.
	 */
	void audio_close(int fd);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _AUDIO_AUDIO_H */
