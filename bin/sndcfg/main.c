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
 * sndcfg — apply the saved master volume/mute from the ubistry registry to the
 * audio codec at boot.  Reads /aural/volume (0..100) and /aural/mute (0/1) and
 * pushes them to /dev/audio via the AC97 mixer ioctls, so the Settings → Sound
 * choice survives a reboot (the codec otherwise resets to full volume).  Run
 * once from /etc/init.d after the audio device and ubistry are up; the Settings
 * Sound pane writes the same keys when the user changes them.
 *
 * This is the lightweight stop-gap for persistence.  A future userland audio
 * server ("aural") will own the codec and central mixer, at which point master
 * volume becomes part of that daemon rather than a one-shot applier.
 */

#include <ubistry/ubistry.h>
#include <api/ubix.h>
#include <audio/audio.h>

int main(void)
{
	int vol = 100; /* default: full volume, unmuted */
	int mute = 0;

	(void)ubistry_get_int("/aural/volume", &vol);
	(void)ubistry_get_int("/aural/mute", &mute);

	if (vol < 0)
		vol = 0;
	if (vol > 100)
		vol = 100;

	int fd = audio_open("/dev/audio");
	if (fd < 0)
	{
		/* No audio device on this machine — nothing to apply. */
		ulog(ULOG_INFO, "sndcfg: no /dev/audio, skipping");
		return (0);
	}

	audio_set_volume(fd, (unsigned int)vol);
	audio_set_mute(fd, mute ? 1u : 0u);
	audio_close(fd);

	ulogf(ULOG_INFO, "sndcfg: applied volume=%d%% mute=%d", vol, mute ? 1 : 0);
	return (0);
}
