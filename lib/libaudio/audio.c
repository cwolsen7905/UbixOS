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

#include <audio/audio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <sys/ioctl.h>

int audio_open(const char *dev)
{
	return open(dev, O_WRONLY);
}

int audio_set_rate(int fd, uint32_t rate)
{
	return ioctl(fd, AUDIO_SET_RATE, &rate);
}

int audio_set_volume(int fd, uint32_t pct)
{
	return ioctl(fd, AUDIO_SET_VOLUME, &pct);
}

int audio_get_volume(int fd, uint32_t *pct)
{
	return ioctl(fd, AUDIO_GET_VOLUME, pct);
}

int audio_set_mute(int fd, uint32_t mute)
{
	return ioctl(fd, AUDIO_SET_MUTE, &mute);
}

int audio_get_mute(int fd, uint32_t *mute)
{
	return ioctl(fd, AUDIO_GET_MUTE, mute);
}

int audio_write(int fd, const void *buf, int n)
{
	int total = 0;
	const char *p = (const char *)buf;

	/*
	 * Retry loop in user mode (IF=1) so the AC97 ISR can fire between
	 * attempts.  The kernel write is non-blocking: it returns 0 when the
	 * ring is full rather than blocking with interrupts disabled.
	 */
	while (total < n)
	{
		int r = (int)write(fd, p + total, (size_t)(n - total));
		if (r < 0)
			return (-1);
		if (r > 0)
		{
			total += r;
		}
		else
		{
			/* Ring full — yield so the ISR gets a chance to drain it. */
			sched_yield();
		}
	}
	return (total);
}

void audio_close(int fd)
{
	close(fd);
}
